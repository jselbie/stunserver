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



#include "commonincludes.hpp"
#include "stuncore.h"
#include "stunsocket.h"
#include "stunsocketthread.h"
#include "recvfromex.h"
#include "ratelimiter.h"


CStunSocketThread::CStunSocketThread() :
_arrSendSockets(), // zero-init
_fNeedToExit(false),
_pthread((pthread_t)-1),
_fThreadIsValid(false),
_rotation(0),
_tsa() // zero-init
{
    ClearSocketArray();
}

CStunSocketThread::~CStunSocketThread()
{
    SignalForStop(true);
    WaitForStopAndClose();
}

void CStunSocketThread::ClearSocketArray()
{
    _arrSendSockets = NULL;
    _socks.clear();
}

HRESULT CStunSocketThread::Init(CStunSocket* arrayOfFourSockets, TransportAddressSet* pTSA, IStunAuth* pAuth, SocketRole rolePrimaryRecv, boost::shared_ptr<RateLimiter>& spLimiter)
{
    HRESULT hr = S_OK;
    
    bool fSingleSocketRecv = ::IsValidSocketRole(rolePrimaryRecv);
    
    ChkIfA(_fThreadIsValid, E_UNEXPECTED);

    ChkIfA(arrayOfFourSockets == NULL, E_INVALIDARG);
    ChkIfA(pTSA == NULL, E_INVALIDARG);
    
    // if this thread was configured to listen on a single socket (aka "multi-threaded mode"), then 
    // validate that it exists
    if (fSingleSocketRecv)
    {
        ChkIfA(arrayOfFourSockets[rolePrimaryRecv].IsValid()==false, E_UNEXPECTED);
    }
    
    _arrSendSockets = arrayOfFourSockets;
    
    // initialize the TSA thing
    _tsa = *pTSA;
    
    if (fSingleSocketRecv)
    {
        // only one socket to listen on
        _socks.push_back(&_arrSendSockets[rolePrimaryRecv]);
    }
    else
    {
        for (size_t i = 0; i < 4; i++)
        {
            if (_arrSendSockets[i].IsValid())
            {
                _socks.push_back(&_arrSendSockets[i]);
            }
        }
    }
    
    

    Chk(InitThreadBuffers());

    _fNeedToExit = false;
    
    _rotation = 0;
    
    _spAuth.Attach(pAuth);
    
    _spLimiter = spLimiter;

Cleanup:
    return hr;
}

HRESULT CStunSocketThread::InitThreadBuffers()
{
    HRESULT hr = S_OK;
    
    _reader.Reset();
    
    _spBufferReader = CRefCountedBuffer(new CBuffer(MAX_STUN_MESSAGE_SIZE));
    _spBufferIn = CRefCountedBuffer(new CBuffer(MAX_STUN_MESSAGE_SIZE));
    _spBufferOut = CRefCountedBuffer(new CBuffer(MAX_STUN_MESSAGE_SIZE));
    
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

    HRESULT hr = S_OK;

    _fNeedToExit = true;

    // have the socket send a message to itself
    // if another thread is sharing the same socket, this may wake that thread up to
    // but all the threads should be started and shutdown together
    if (fPostMessages)
    {
        for (size_t index = 0; index < _socks.size(); index++)
        {
            char data = 'x';
            
            ASSERT(_socks[index] != NULL);
            
            CSocketAddress addr(_socks[index]->GetLocalAddress());
            

            // If no specific adapter was binded to, IP will be 0.0.0.0
            // Linux evidently treats 0.0.0.0 IP as loopback (and works)
            // On Windows you can't send to 0.0.0.0. sendto will fail - switch to sending to localhost
            if (addr.IsIPAddressZero())
            {
                CSocketAddress addrLocal;
                CSocketAddress::GetLocalHost(addr.GetFamily(), &addrLocal);
                addrLocal.SetPort(addr.GetPort());
                addr = addrLocal;
            }
            
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
    
    ClearSocketArray();
    
    UninitThreadBuffers();
    
    return S_OK;
}

// static
void* CStunSocketThread::ThreadFunction(void* pThis)
{
    ((CStunSocketThread*)pThis)->Run();
    return NULL;
}

// returns an index into _socks, not _arrSockets
CStunSocket* CStunSocketThread::WaitForSocketData()
{
    fd_set set = {};
    int nHighestSockValue = 0;
    int ret;
    CStunSocket* pReadySocket = NULL;
    UNREFERENCED_VARIABLE(ret); // only referenced in ASSERT
    size_t nSocketCount = _socks.size();
    
    
    // rotation gives another socket priority in the next loop
    _rotation = (_rotation + 1) % nSocketCount;
    ASSERT(_rotation >= 0);

    FD_ZERO(&set);

    for (size_t index = 0; index < nSocketCount; index++)
    {
        ASSERT(_socks[index] != NULL);
        int sock = _socks[index]->GetSocketHandle();
        ASSERT(sock != -1);
        FD_SET(sock, &set);
        nHighestSockValue = (sock > nHighestSockValue) ? sock : nHighestSockValue;
    }

    // wait indefinitely for a socket
    ret = ::select(nHighestSockValue+1, &set, NULL, NULL, NULL);
    
    ASSERT(ret > 0); // This will be a benign assert, and should never happen.  But I will want to know if it does

    // now figure out which socket just got data on it
    for (size_t index = 0; index < nSocketCount; index++)
    {
        int indexconverted = (index + _rotation) % nSocketCount;
        int sock = _socks[indexconverted]->GetSocketHandle();
        
        ASSERT(sock != -1);
        
        if (FD_ISSET(sock, &set))
        {
            pReadySocket = _socks[indexconverted];
            break;
        }
    }
    
    ASSERT(pReadySocket != NULL);
    
    return pReadySocket;
}


void CStunSocketThread::Run()
{
    size_t nSocketCount = _socks.size();
    bool fMultiSocketMode = (nSocketCount > 1);
    int recvflags = fMultiSocketMode ? MSG_DONTWAIT : 0;
    CStunSocket* pSocket = _socks[0];
    int ret;
    char szIPRemote[100] = {};
    char szIPLocal[100] = {};
    bool allowed_to_pass = true;

    
    int sendsocketcount = 0;

    sendsocketcount += (int)(_tsa.set[RolePP].fValid);
    sendsocketcount += (int)(_tsa.set[RolePA].fValid);
    sendsocketcount += (int)(_tsa.set[RoleAP].fValid);
    sendsocketcount += (int)(_tsa.set[RoleAA].fValid);

    Logging::LogMsg(LL_DEBUG, "Starting listener thread (%d recv sockets, %d send sockets)", _socks.size(), sendsocketcount);

    while (_fNeedToExit == false)
    {

        if (fMultiSocketMode)
        {
            pSocket = WaitForSocketData();
            
            if (_fNeedToExit)
            {
                break;
            }

            ASSERT(pSocket != NULL);
            
            if (pSocket == NULL)
            {
                // just go back to waiting;
                continue;
            }
        }
        
        ASSERT(pSocket != NULL);

        // now receive the data
        _spBufferIn->SetSize(0);

        ret = ::recvfromex(pSocket->GetSocketHandle(), _spBufferIn->GetData(), _spBufferIn->GetAllocatedSize(), recvflags, &_msgIn.addrRemote, &_msgIn.addrLocal);

        // recvfromex no longer sets the port value on the local address
        if (ret >= 0)
        {
            _msgIn.addrLocal.SetPort(pSocket->GetLocalAddress().GetPort());
        }
        

        if (Logging::GetLogLevel() >= LL_VERBOSE)
        {
            _msgIn.addrRemote.ToStringBuffer(szIPRemote, 100);
            _msgIn.addrLocal.ToStringBuffer(szIPLocal, 100);
        }
        else
        {
            szIPRemote[0] = '\0';
            szIPLocal[0] = '\0';
        }
        
        Logging::LogMsg(LL_VERBOSE, "recvfrom returns %d from %s on local interface %s", ret, szIPRemote, szIPLocal);

        allowed_to_pass = (_spLimiter.get() != NULL) ? _spLimiter->RateCheck(_msgIn.addrRemote) : true;
        
        if (allowed_to_pass == false)
        {
            Logging::LogMsg(LL_VERBOSE, "RateLimiter signals false for packet from %s", szIPRemote);
        }

        if ((ret < 0) || (allowed_to_pass == false))
        {
            // error
            continue;
        }

        if (_fNeedToExit)
        {
            break;
        }

        _spBufferIn->SetSize(ret);
        
        _msgIn.socketrole = pSocket->GetRole();
        
        
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
    ASSERT(_arrSendSockets[_msgOut.socketrole].IsValid());
    sockout = _arrSendSockets[_msgOut.socketrole].GetSocketHandle();
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





