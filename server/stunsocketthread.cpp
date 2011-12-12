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
_rotation(0),
_tsa() // zero-init
{
    ;
}

CStunSocketThread::~CStunSocketThread()
{
    SignalForStop(true);
    WaitForStopAndClose();
}

HRESULT CStunSocketThread::Init(std::vector<CRefCountedStunSocket>& listSockets, IStunAuth* pAuth)
{
    HRESULT hr = S_OK;
    
    ChkIfA(_fThreadIsValid, E_UNEXPECTED);

    ChkIfA(listSockets.size() <= 0, E_INVALIDARG);
    
    _socks = listSockets;

    // initialize the TSA thing
    memset(&_tsa, '\0', sizeof(_tsa));
    for (size_t i = 0; i < _socks.size(); i++)
    {
        SocketRole role = _socks[i]->GetRole();
        ASSERT(_tsa.set[role].fValid == false); // two sockets for same role?
        _tsa.set[role].fValid = true;
        _tsa.set[role].addr = _socks[i]->GetLocalAddress();
    }

    Chk(InitThreadBuffers());

    _fNeedToExit = false;
    
    _rotation = 0;
    
    _spAuth.Attach(pAuth);

Cleanup:
    return hr;
}

HRESULT CStunSocketThread::InitThreadBuffers()
{
    HRESULT hr = S_OK;
    
    _reader.Reset();
    
    _spBufferReader = CRefCountedBuffer(new CBuffer(1500));
    _spBufferIn = CRefCountedBuffer(new CBuffer(1500));
    _spBufferOut = CRefCountedBuffer(new CBuffer(1500));
    
    _reader.GetStream().Attach(_spBufferReader, true);
    
    _msgIn.fConnectionOriented = false;
    _msgIn.pReader = &_reader;
    _msgOut.spBufferOut = _spBufferOut;
    
    return hr;
}

void CStunSocketThread::UninitThreadBuffers()
{
    _reader.Reset();
    _spBufferReader.reset();
    _spBufferIn.reset();
    _spBufferOut.reset();
    
    _msgIn.pReader = NULL;
    _msgOut.spBufferOut.reset();
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
    
    UninitThreadBuffers();

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
    
    
    int ret;
    int socketindex = 0;

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
        _spBufferIn->SetSize(0);

        ret = ::recvfromex(spSocket->GetSocketHandle(), _spBufferIn->GetData(), _spBufferIn->GetAllocatedSize(), recvflags, &_msgIn.addrRemote, &_msgIn.addrLocal);

        if (Logging::GetLogLevel() >= LL_VERBOSE)
        {
            char szIPRemote[100];
            char szIPLocal[100];
            _msgIn.addrRemote.ToStringBuffer(szIPRemote, 100);
            _msgIn.addrLocal.ToStringBuffer(szIPLocal, 100);
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

        _spBufferIn->SetSize(ret);
        
        _msgIn.socketrole = spSocket->GetRole();
        
        
        // --------------------------------------------------------------------
        // now let's handle this message and get the response back out
        
        ProcessRequestAndSendResponse();
    }

    Logging::LogMsg(LL_DEBUG, "Thread exiting");
}

                        
HRESULT CStunSocketThread::ProcessRequestAndSendResponse()
{
    HRESULT hr = S_OK;
    int sendret = -1;
    int sockout = -1;

    // Reset the reader object and re-attach the buffer
    _reader.Reset();
    _spBufferReader->SetSize(0);
    _reader.GetStream().Attach(_spBufferReader, true);
    
    // Consume the message and just validate that it is a stun message
    _reader.AddBytes(_spBufferIn->GetData(), _spBufferIn->GetSize());
    ChkIf(_reader.GetState() != CStunMessageReader::BodyValidated, E_FAIL);
    
    // msgIn and msgOut are already initialized
    
    Chk(CStunRequestHandler::ProcessRequest(_msgIn, _msgOut, &_tsa, _spAuth));

    ASSERT(_tsa.set[_msgOut.socketrole].fValid);
    sockout = GetSocketForRole(_msgOut.socketrole);
    
    ASSERT(sockout != -1);
    // find the socket that matches the role specified by msgOut
    sendret = ::sendto(sockout, _spBufferOut->GetData(), _spBufferOut->GetSize(), 0, _msgOut.addrDest.GetSockAddr(), _msgOut.addrDest.GetSockAddrLength());
    
    
    if (Logging::GetLogLevel() >= LL_VERBOSE)
    {
        Logging::LogMsg(LL_VERBOSE, "sendto returns %d (err == %d)\n", sendret, errno);
    }

        
Cleanup:
    return hr;
}

int CStunSocketThread::GetSocketForRole(SocketRole role)
{
    int sock = -1;
    size_t len = _socks.size();
    
    ASSERT(::IsValidSocketRole(role));
    ASSERT(_tsa.set[role].fValid);
    
    for (size_t i = 0; i < len; i++)
    {
        if (_socks[i]->GetRole() == role)
        {
            sock = _socks[i]->GetSocketHandle();
        }
    }
    
    return sock;
}




