/*
   Copyright 2011 John Selbie

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/



#include "commonincludes.h"
#include "stuncore.h"
#include "stunsocket.h"
#include "stunsocketthread.h"
#include "recvfromex.h"


CStunSocketThread::CStunSocketThread() :
_fNeedToExit(false),
_pthread((pthread_t)-1),
_fThreadIsValid(false),
_rotation(0)
{
    ;
}

CStunSocketThread::~CStunSocketThread()
{
    SignalForStop(true);
    WaitForStopAndClose();
}

HRESULT CStunSocketThread::Init(std::vector<CRefCountedStunSocket>& listSockets, IStunResponder* pResponder, IStunAuth* pAuth)
{
    HRESULT hr = S_OK;

    ChkIfA(_fThreadIsValid, E_UNEXPECTED);
    ChkIfA(pResponder == NULL, E_UNEXPECTED);

    ChkIfA(listSockets.size() <= 0, E_INVALIDARG);

    _socks = listSockets;

    _handler.SetResponder(pResponder);
    _handler.SetAuth(pAuth);

    _fNeedToExit = false;
    
    _rotation = 0;

Cleanup:
    return hr;

}

HRESULT CStunSocketThread::Start()
{
    HRESULT hr = S_OK;
    int err = 0;

    ChkIfA(_fThreadIsValid, E_UNEXPECTED);

    ChkIfA(_socks.size() <= 0, E_FAIL);


    err = ::pthread_create(&_pthread, NULL, CStunSocketThread::ThreadFunction, this);

    ChkIfA(err != 0, ERRNO_TO_HRESULT(err));
    _fThreadIsValid = true;

Cleanup:
    return hr;
}




HRESULT CStunSocketThread::SignalForStop(bool fPostMessages)
{
    size_t size = _socks.size();
    HRESULT hr = S_OK;

    _fNeedToExit = true;

    // have the socket send a message to itself
    // if another thread is sharing the same socket, this may wake that thread up to
    // but all the threads should be started and shutdown together
    if (fPostMessages)
    {
        for (size_t index = 0; index < size; index++)
        {
            char data = 'x';
            ::CSocketAddress addr(_socks[index]->GetLocalAddress());
            ::sendto(_socks[index]->GetSocketHandle(), &data, 1, 0, addr.GetSockAddr(), addr.GetSockAddrLength());
        }
    }

    return hr;
}

HRESULT CStunSocketThread::WaitForStopAndClose()
{
    void* pRetValFromThread = NULL;

    if (_fThreadIsValid)
    {
        // now wait for the thread to exit
        pthread_join(_pthread, &pRetValFromThread);
    }

    _fThreadIsValid = false;
    _pthread = (pthread_t)-1;
    _socks.clear();

    return S_OK;
}

// static
void* CStunSocketThread::ThreadFunction(void* pThis)
{
    ((CStunSocketThread*)pThis)->Run();
    return NULL;
}

int CStunSocketThread::WaitForSocketData()
{
    fd_set set = {};
    int nHighestSockValue = 0;
    size_t nSocketCount = _socks.size();
    int ret;
    CRefCountedStunSocket spSocket;
    int result = -1;
    UNREFERENCED_VARIABLE(ret); // only referenced in ASSERT
    
    // rotation gives another socket priority in the next loop
    _rotation = (_rotation + 1) % nSocketCount;
    ASSERT(_rotation >= 0);

    FD_ZERO(&set);

    for (size_t index = 0; index < nSocketCount; index++)
    {
        int sock = _socks[index]->GetSocketHandle();
        FD_SET(sock, &set);
        nHighestSockValue = (sock > nHighestSockValue) ? sock : nHighestSockValue;
    }

    // wait indefinitely for a socket
    ret = ::select(nHighestSockValue+1, &set, NULL, NULL, NULL);
    
    ASSERT(ret > 0); // This will be a benign assert, and should never happen.  But I will want to know if it does

    // now figure out which socket just got data on it
    spSocket.reset();
    for (size_t index = 0; index < nSocketCount; index++)
    {
        int indexconverted = (index + _rotation) % nSocketCount;

        int sock = _socks[indexconverted]->GetSocketHandle();
        if (FD_ISSET(sock, &set))
        {
            result = indexconverted;
            break;
        }
    }
    
    return result;
}


void CStunSocketThread::Run()
{
    size_t nSocketCount = _socks.size();
    bool fMultiSocketMode = (nSocketCount > 1);
    int recvflags = fMultiSocketMode ? MSG_DONTWAIT : 0;
    CRefCountedStunSocket spSocket = _socks[0];
    const int RECV_BUFFER_SIZE = 1500;
    CRefCountedBuffer spBuffer(new CBuffer(RECV_BUFFER_SIZE));
    int ret;
    int socketindex = 0;
    CSocketAddress remoteAddr;
    CSocketAddress localAddr;
    

    Logging::LogMsg(LL_DEBUG, "Starting listener thread");


    while (_fNeedToExit == false)
    {

        if (fMultiSocketMode)
        {
            spSocket.reset();
            socketindex = WaitForSocketData();
            
            if (_fNeedToExit)
            {
                break;
            }

            ASSERT(socketindex >= 0);
            
            if (socketindex < 0)
            {
                // just go back to waiting;
                continue;
            }
            
            spSocket = _socks[socketindex];
            ASSERT(spSocket != NULL);
        }

        // now receive the data
        spBuffer->SetSize(0);

        ret = ::recvfromex(spSocket->GetSocketHandle(), spBuffer->GetData(), spBuffer->GetAllocatedSize(), recvflags, &remoteAddr, &localAddr);

        if (Logging::GetLogLevel() >= LL_VERBOSE)
        {
            char szIPRemote[100];
            char szIPLocal[100];
            remoteAddr.ToStringBuffer(szIPRemote, 100);
            localAddr.ToStringBuffer(szIPLocal, 100);
            Logging::LogMsg(LL_VERBOSE, "recvfrom returns %d from %s on local interface %s", ret, szIPRemote, szIPLocal);
        }

        if (ret < 0)
        {
            // error
            continue;
        }

        if (_fNeedToExit)
        {
            break;
        }

        spBuffer->SetSize(ret);

        StunMessageEnvelope msg;
        msg.remoteAddr = remoteAddr;
        msg.spBuffer = spBuffer;
        msg.localSocket = spSocket->GetRole();
        msg.localAddr = localAddr;

        _handler.ProcessRequest(msg);
    }

    Logging::LogMsg(LL_DEBUG, "Thread exiting");



}




