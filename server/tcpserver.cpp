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
#include "tcpserver.h"
#include "server.h"
#include "stunsocket.h"



#include "stunsocketthread.h"




#define IS_DIVISIBLE_BY(x, y)  ((x % y)==0)

static bool IsPrime(unsigned int val)
{
    unsigned int stop;
    
    if (val <= 1)
    {
        return false;
    }
    
    if ((val == 2) || (val == 3) || (val == 5))
    {
        return false;
    }
    
    if (IS_DIVISIBLE_BY(val, 2))
    {
        return false;
    }
    
    stop = (unsigned int)((int)(ceil(sqrt(val))));
    
    for (unsigned int i = 3; i <= stop; i+=2)
    {
        if (IS_DIVISIBLE_BY(val, i))
        {
            return false;        
        }
    }
    
    return true;
}

static size_t GetHashTableWidth(unsigned int maxConnections)
{
    size_t width;
    
    if (maxConnections >= 10007)
    {
        return 10007;
    }
    
    width = maxConnections;
    
    while (IsPrime(width) == false)
    {
        width++;
    }
    return width;
}

// client sockets are edge triggered
const uint32_t EPOLL_CLIENT_READ_EVENT_SET = EPOLLET | EPOLLIN | EPOLLRDHUP;
const uint32_t EPOLL_CLIENT_WRITE_EVENT_SET = EPOLLET | EPOLLOUT;

// listen sockets are always level triggered (that way, when we recover from
//    hitting a max connections condition, we don't have to worry about
//    missing a notification
const uint32_t EPOLL_LISTEN_SOCKET_EVENT_SET = EPOLLIN;

// notification pipe could go either way
const uint32_t EPOLL_PIPE_EVENT_SET = EPOLLIN;

const int c_MaxNumberOfConnectionsDefault = 10000;


CTCPStunThread::CTCPStunThread()
{
    _epoll = -1;
    _pipe[0] = _pipe[1] = -1;
    _pthread = (pthread_t)-1;
    Reset();
}

void CTCPStunThread::Reset()
{
    CloseEpoll();
    CloseListenSockets();
    ClosePipes();
    
    _fListenSocketsOnEpoll = false;
    
    memset(&_tsaListen, '\0', sizeof(_tsaListen));
    
    _fNeedToExit = false;
    _spAuth.ReleaseAndClear();
    _role = RolePP;
    
    memset(&_tsa, '\0', sizeof(_tsa));
    
    _maxConnections = c_MaxNumberOfConnectionsDefault;

    _pthread = (pthread_t)-1;
    _fThreadIsValid = false;
    
    
    _connectionpool.Reset();

    // the thread should have closed all the connections
    ASSERT(_hashConnections1.Size() == 0);
    ASSERT(_hashConnections2.Size() == 0);
    _hashConnections1.ResetTable();
    _hashConnections2.ResetTable();
    
    _pNewConnList = &_hashConnections1;
    _pOldConnList = &_hashConnections2;
    
    _timeLastSweep = time(NULL);
}



CTCPStunThread::~CTCPStunThread()
{
    Stop(); // calls Reset
    ASSERT(_pipe[0] == -1); // quick assert to make sure reset was called
}


HRESULT CTCPStunThread::CreatePipes()
{
    HRESULT hr = S_OK;
    int ret;
    
    ASSERT(_pipe[0] == -1);
    ASSERT(_pipe[1] == -1);
    
    ret = ::pipe(_pipe);
    
    ChkIf(ret == -1, ERRNOHR);
    
Cleanup:
    return hr;
}

void CTCPStunThread::ClosePipes()
{
    if (_pipe[0] != -1)
    {
        close(_pipe[0]);
        _pipe[0] = -1;
    }
    
    if (_pipe[1] != -1)
    {
        close(_pipe[1]);
        _pipe[1] = -1;
    }
}

HRESULT CTCPStunThread::NotifyThreadViaPipe()
{
    ASSERT(_pipe[1] != -1);
    int ret;
    
    // _pipe[1] is the write end of the pipe
    char ch = 'x';
    ret = write(_pipe[1], &ch, 1);
    return (ret > 0) ? S_OK : S_FALSE;
}


HRESULT CTCPStunThread::CreateEpoll()
{
    ASSERT(_epoll == -1);
    _epoll = epoll_create(1000); // todo change this parameter to "max connections" (although it's likely an ignored parameter)
    if (_epoll == -1)
    {
        return ERRNOHR;
    }
    return S_OK;
}

void CTCPStunThread::CloseEpoll()
{
    if (_epoll != -1)
    {
        close(_epoll);
        _epoll = -1;
    }
}

HRESULT CTCPStunThread::AddSocketToEpoll(int sock, uint32_t events)
{
    HRESULT hr = S_OK;
    epoll_event ev = {};
    
    ASSERT(sock != -1);
    
    ev.data.fd = sock;
    ev.events = events;
    ChkIfA(epoll_ctl(_epoll, EPOLL_CTL_ADD, sock, &ev) == -1, ERRNOHR);
Cleanup:
    return hr;
}

HRESULT CTCPStunThread::AddClientSocketToEpoll(int sock)
{
    return AddSocketToEpoll(sock, EPOLL_CLIENT_READ_EVENT_SET);
}

HRESULT CTCPStunThread::DetachFromEpoll(int sock)
{
    HRESULT hr = S_OK;
    epoll_event ev={}; // pass empty ev, because some implementations of epoll_ctl can't handle a NULL event struct
    
    if (sock == -1)
    {
        return S_FALSE;
    }
    
    ChkIfA(epoll_ctl(_epoll, EPOLL_CTL_DEL, sock, &ev) == -1, ERRNOHR);
Cleanup:
    return hr;
}


HRESULT CTCPStunThread::EpollCtrl(int sock, uint32_t events)
{
    HRESULT hr = S_OK;
    
    ASSERT(sock != -1);
    
    epoll_event ev = {};
    ev.data.fd = sock;
    ev.events = events;
    
    ChkIfA(epoll_ctl(_epoll, EPOLL_CTL_MOD, sock, &ev) == -1, ERRNOHR);
Cleanup:
    return hr;
}

HRESULT CTCPStunThread::SetListenSocketsOnEpoll(bool fEnable)
{
    HRESULT hr = S_OK;
    
    if (fEnable != _fListenSocketsOnEpoll)
    {
        for (int role = 0; role < 4; role++)
        {
            int sock = _socketTable[role];
            
            if (sock == -1)
            {
                continue;
            }
            
            if (fEnable)
            {
                ChkA(AddSocketToEpoll(sock, EPOLL_LISTEN_SOCKET_EVENT_SET));
            }
            else
            {
                ChkA(DetachFromEpoll(sock));
            }
        }
        
        _fListenSocketsOnEpoll = fEnable;
    }
    
Cleanup:
    return hr;
}


HRESULT CTCPStunThread::CreateListenSockets()
{
    HRESULT hr = S_OK;
    int ret;
   
    
    for (int r = (int)RolePP; r <= (int)RoleAA; r++)
    {
        if (_tsaListen.set[r].fValid)
        {
            ChkA(_socketListenArray[r].TCPInit(_tsaListen.set[r].addr, (SocketRole)r));
            _socketTable[r] = _socketListenArray[r].GetSocketHandle();
            ChkA(_socketListenArray[r].SetNonBlocking(true));
            ret = listen(_socketTable[r], 128); // todo - figure out the right value to pass to listen
            ChkIfA(ret == -1, ERRNOHR);
            _countSocks++;
        }
        else
        {
            _socketTable[r] = -1;
        }
    }
    
    _fListenSocketsOnEpoll = false;
    
Cleanup:
    return hr;
}

void CTCPStunThread::CloseListenSockets()
{
    for (size_t r = 0; r < ARRAYSIZE(_socketTable); r++)
    {
        _socketListenArray[r].Close();
        _socketTable[r] = -1;
    }
    _countSocks = 0;
    
}

CStunSocket* CTCPStunThread::GetListenSocket(int sock)
{
    ASSERT(sock != -1);

    if (sock != -1)
    {
        for (size_t i = 0; i < ARRAYSIZE(_socketTable); i++)
        {
            if (_socketTable[i] == sock)
            {
                return &_socketListenArray[i];
            }
        }
    }
    return NULL;
}



HRESULT CTCPStunThread::Init(const TransportAddressSet& tsaListen, const TransportAddressSet& tsaHandler, IStunAuth* pAuth, int maxConnections)
{
    HRESULT hr = S_OK;
    int ret;
    size_t hashTableWidth;
    int countListen = 0;
    int countHandler = 0;
    
    // we shouldn't be initialized at this point
    ChkIfA(_pipe[0] != -1, E_UNEXPECTED);
    ChkIfA(_fThreadIsValid, E_UNEXPECTED);

    // Max sure we didn't accidently pass in anything crazy
    ChkIfA(_maxConnections >= 100000, E_INVALIDARG);
    
    for (size_t i = 0; i <= ARRAYSIZE(_tsa.set); i++)
    {
        countListen += tsaListen.set[i].fValid ? 1 : 0;
        countHandler += tsaHandler.set[i].fValid ? 1 : 0;
    }
    
    ChkIfA(countListen == 0, E_INVALIDARG);
    ChkIfA(countHandler == 0, E_INVALIDARG);
    
    _tsaListen = tsaListen;
    _tsa = tsaHandler;

    
    _spAuth.Attach(pAuth);

    ChkA(CreateListenSockets());
    
    ChkA(CreatePipes());
    
    ChkA(CreateEpoll()); 
    
    // add listen socket to epoll
    ASSERT(_fListenSocketsOnEpoll == false);
    ChkA(SetListenSocketsOnEpoll(true));
    
    
    // add read end of pipe to epoll so we can get notified of when a signal to exit has occurred
    ChkA(AddSocketToEpoll(_pipe[0], EPOLL_PIPE_EVENT_SET));
    
    _maxConnections = (maxConnections > 0) ? maxConnections : c_MaxNumberOfConnectionsDefault;
    
    // todo - get "max connections" from an init param
    
    hashTableWidth = GetHashTableWidth(_maxConnections);
    ret = _hashConnections1.InitTable(_maxConnections, hashTableWidth);
    ChkIfA(ret == -1, E_FAIL);
    
    ret = _hashConnections2.InitTable(_maxConnections, hashTableWidth);
    ChkIfA(ret == -1, E_FAIL);
    
    _pNewConnList = &_hashConnections1;
    _pOldConnList = &_hashConnections2;
    
    _fNeedToExit = false;
    
Cleanup:
    if (FAILED(hr))
    {
        Reset();
    }
    return hr;
}


HRESULT CTCPStunThread::Start()
{
    int ret;
    HRESULT hr = S_OK;
    
    ChkIfA(_fThreadIsValid, E_FAIL);
    
    ChkIf(_pipe[0] == -1, E_UNEXPECTED); // Init hasn't been called
    
    _fNeedToExit = false;
    ret = ::pthread_create(&_pthread, NULL, ThreadFunction, this);
    ChkIfA(ret != 0, ERRNO_TO_HRESULT(ret));
    
    _fThreadIsValid = true;
    
Cleanup:
    return hr;
}


HRESULT CTCPStunThread::Stop()
{
    void* pRetValueFromThread = NULL;
    
    if (_fThreadIsValid)
    {
        _fNeedToExit = true;
        
        // signal the thread to exit
        NotifyThreadViaPipe();

        // wait for the thread to exit
        ::pthread_join(_pthread, &pRetValueFromThread);
        
        _fThreadIsValid = false;
    }
    
    // we don't support restarting a thread (as that would require flushing _pipe)
    // so go ahead and make it impossible for that to happen
    Reset();
    return S_OK;
}

void* CTCPStunThread::ThreadFunction(void* pThis)
{
    ((CTCPStunThread*)pThis)->Run();
    return NULL;
}

bool CTCPStunThread::IsTimeoutNeeded()
{
    return ((_pNewConnList->Size() > 0) || (_pOldConnList->Size() > 0));
}

bool CTCPStunThread::IsConnectionCountAtMax()
{
    size_t size1 = _pNewConnList->Size();
    size_t size2 = _pOldConnList->Size();
    
    return ((size1 + size2) >= (size_t)_maxConnections);
}


void CTCPStunThread::Run()
{
    
    Logging::LogMsg(LL_DEBUG, "Starting TCP listening thread (%d sockets)\n", _countSocks);
    
    _timeLastSweep = time(NULL);
    
    while (_fNeedToExit == false)
    {
        // wait for a notification
        epoll_event ev = {};
        int timeout = -1; // wait forever
        CStunSocket* pListenSocket = NULL;

        int ret;

        if (IsTimeoutNeeded())
        {
            timeout = CTCPStunThread::c_sweepTimeoutMilliseconds;
        }
        
        // turn off epoll eventing from the listen sockets if we are at max connections
        // otherwise, make sure it is enabled.
        SetListenSocketsOnEpoll(IsConnectionCountAtMax() == false);
        
        ret = ::epoll_wait(_epoll, &ev, 1, timeout);
        
        if ( _fNeedToExit || (ev.data.fd == _pipe[0]) )
        {
            break;
        }
        
        if (ret > 0)
        {
            if (Logging::GetLogLevel() >= LL_VERBOSE)
            {
                Logging::LogMsg(LL_VERBOSE, "socket %d: %x (%s%s%s%s%s%s)", ev.data.fd, ev.events,
                    (ev.events&EPOLLIN) ? " EPOLLIN " : "",
                    (ev.events&EPOLLOUT) ? " EPOLLOUT " : "",
                    (ev.events&EPOLLRDHUP) ? " EPOLLRDHUP " : "",
                    (ev.events&EPOLLHUP) ? " EPOLLHUP " : "",
                    (ev.events&EPOLLERR) ? " EPOLLERR " : "",
                    (ev.events&EPOLLPRI) ? " EPOLLPRI " : "");
            }
            
            pListenSocket = GetListenSocket(ev.data.fd);
            if (pListenSocket)
            {
                StunConnection* pConn = AcceptConnection(pListenSocket);
                
                // as an optimization - see if we can do a read on the new connection
                if (pConn)
                {
                    ReceiveBytesForConnection(pConn);
                }
            }
            else
            {
                ProcessConnectionEvent(ev.data.fd, ev.events);
            }
        }
        
        // close any connection that we haven't heard from in a while
        SweepDeadConnections();
    }
    
    ThreadCleanup();
    
    Logging::LogMsg(LL_DEBUG, "TCP Thread exiting");
}

void CTCPStunThread::ProcessConnectionEvent(int sock, uint32_t eventflags)
{
    StunConnection** ppConn = NULL;
    StunConnection* pConn = NULL;
    
    ppConn = _pNewConnList->Lookup(sock);
    if (ppConn == NULL)
    {
        ppConn = _pOldConnList->Lookup(sock);
    }
    
    if ((ppConn == NULL) || (*ppConn == NULL))
    {
        Logging::LogMsg(LL_DEBUG, "Warning - ProcessConnectionEvent could not resolve socket into connection (socket == %d)", sock);
        return;
    }
    
    pConn = *ppConn;
    
    // if event flags is an error or a hangup, that's ok, the subsequent call below will consume the error and close the connection as appropriate
    
    if (pConn->_state == ConnectionState_Receiving)
    {
        ReceiveBytesForConnection(pConn);
    }
    else if (pConn->_state == ConnectionState_Transmitting)
    {
        WriteBytesForConnection(pConn);
    }
    else if (pConn->_state == ConnectionState_Closing)
    {
        ConsumeRemoteClose(pConn);
    }
        
    
}


StunConnection* CTCPStunThread::AcceptConnection(CStunSocket* pListenSocket)
{
    int listensock = pListenSocket->GetSocketHandle();
    SocketRole role = pListenSocket->GetRole();
    int clientsock = -1;
    int socktmp = -1;
    sockaddr_storage addrClient;
    socklen_t socklen = sizeof(addrClient);
    StunConnection* pConn = NULL;
    HRESULT hr = S_OK;
    int insertresult;
    int err;
    
    ASSERT(listensock != -1);
    ASSERT(::IsValidSocketRole(role));

    socktmp = ::accept(listensock, (sockaddr*)&addrClient, &socklen);
    

    err = errno;
    Logging::LogMsg(LL_VERBOSE, "accept returns %d (errno == %d)", socktmp, (socktmp<0)?err:0);

    ChkIfA(socktmp == -1, E_FAIL);
    
    clientsock = socktmp;
    
    pConn = _connectionpool.GetConnection(clientsock, role);
    ChkIfA(pConn == NULL, E_FAIL); // Our connection pool has nothing left to give, only thing to do is abort this connection and close the socket
    socktmp = -1;
    
    ChkA(pConn->_stunsocket.SetNonBlocking(true));
    ChkA(AddClientSocketToEpoll(clientsock));
    
    // add connection to our tracking tables
    pConn->_idHashTable = (_pNewConnList == &_hashConnections1) ? 1 : 2;
    insertresult = _pNewConnList->Insert(clientsock, pConn);
    
    // out of space in the lookup tables?
    ChkIfA(insertresult == -1, E_FAIL);
    
    if (Logging::GetLogLevel() >= LL_VERBOSE)
    {
        char szIPRemote[100];
        char szIPLocal[100];
        pConn->_stunsocket.GetLocalAddress().ToStringBuffer(szIPLocal, ARRAYSIZE(szIPLocal));
        pConn->_stunsocket.GetRemoteAddress().ToStringBuffer(szIPRemote, ARRAYSIZE(szIPRemote));
        Logging::LogMsg(LL_VERBOSE, "accepting new connection on socket %d from %s on interface %s", pConn->_stunsocket.GetSocketHandle(), szIPRemote, szIPLocal);
    } 
    
    
Cleanup:
    
    if (FAILED(hr))
    {
        CloseConnection(pConn);
        pConn = NULL;
        if (socktmp != -1)
        {
            close(socktmp);
        }
    }

    return pConn;
}

HRESULT CTCPStunThread::ReceiveBytesForConnection(StunConnection* pConn)
{
    uint8_t buffer[MAX_STUN_MESSAGE_SIZE];
    size_t bytesneeded;
    int bytesread;
    HRESULT hr = S_OK;
    CStunMessageReader::ReaderParseState readerstate;
    int err;
    
    int sock = pConn->_stunsocket.GetSocketHandle();
    
    while (true)
    {
        ASSERT(pConn->_state == ConnectionState_Receiving);
        ASSERT(pConn->_reader.GetState() != CStunMessageReader::ParseError);
        ASSERT(pConn->_reader.GetState() != CStunMessageReader::BodyValidated);
        
        bytesneeded = pConn->_reader.HowManyBytesNeeded();
        
        ChkIfA(bytesneeded == 0, E_UNEXPECTED);
        
        bytesread = recv(sock, buffer, bytesneeded, 0);
        
        err = errno;
        Logging::LogMsg(LL_VERBOSE, "recv on socket %d returns %d (errno=%d)", sock, bytesread, (bytesread<0)?err:0);
        
        
        if ((bytesread < 0) && ((err == EWOULDBLOCK) || (err==EAGAIN)) )
        {
            // no more bytes to be consumed - bail out of here and return success
            break;
        }
        

        // any other error (or an EOF/shutdown notification) means the connection is dead
        ChkIf(bytesread <= 0, E_FAIL);
        
        // we got data, now let's feed it into the reader
        readerstate = pConn->_reader.AddBytes(buffer, bytesread);
        
        ChkIf(readerstate == CStunMessageReader::ParseError, E_FAIL);
        
        if (readerstate == CStunMessageReader::BodyValidated)
        {
            StunMessageIn msgIn;
            StunMessageOut msgOut;
            
            msgIn.addrLocal = pConn->_stunsocket.GetLocalAddress();
            msgIn.addrRemote = pConn->_stunsocket.GetRemoteAddress();
            msgIn.fConnectionOriented = true;
            msgIn.pReader = &pConn->_reader;
            msgIn.socketrole = pConn->_stunsocket.GetRole();
            
            msgOut.spBufferOut = pConn->_spOutputBuffer;
            
            Chk(CStunRequestHandler::ProcessRequest(msgIn, msgOut, &_tsa, _spAuth));
            
            // success - transition to the response state
            pConn->_state = ConnectionState_Transmitting;
            
            // change the socket such that we only listen for "write events"
            Chk(EpollCtrl(sock, EPOLL_CLIENT_WRITE_EVENT_SET));
            
            // optimization - go ahead and try to send the response
            WriteBytesForConnection(pConn);
            // WriteBytesForConnection will close the connection on error
            // And it might call ConsumeRemoteClose, which will also null it out
            
            // so we can't assume the connection is still alive.  And if it's not alive, pConn likely got deleted
            // either refetch from the hash tables, or invent an out parameter on WriteBytesForConnection and ConsumeRemoteClose to better propagate the close state of the connection
            pConn = NULL;
            
            break;
        }
        
        // keep trying to read more bytes
    }
        
Cleanup:
    if (FAILED(hr))
    {
        CloseConnection(pConn);
    }

    return hr;
}

HRESULT CTCPStunThread::WriteBytesForConnection(StunConnection* pConn)
{
    HRESULT hr = S_OK;
    int sock = pConn->_stunsocket.GetSocketHandle();
    int sent = -1;
    uint8_t* pData = NULL;
    size_t bytestotal, bytesremaining;
    bool fForceClose = false;
    int err;

    
    
    ASSERT(pConn != NULL);    
    
    pData = pConn->_spOutputBuffer->GetData();
    bytestotal = pConn->_spOutputBuffer->GetSize();
    
    
    while (true)
    {
        ASSERT(pConn->_state == ConnectionState_Transmitting);
        ASSERT(bytestotal > pConn->_txCount);
        
        bytesremaining = bytestotal - pConn->_txCount;
        
        sent = ::send(sock, pData + pConn->_txCount, bytesremaining, 0);

        err = errno;

        // Can't send any more bytes, come back again later
        ChkIf( ((sent == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))), S_OK);
        
        Logging::LogMsg(LL_VERBOSE, "send on socket %d returns %d (errno=%d)", sock, sent, (sent<0)?err:0);

        // general connection error
        ChkIf(sent == -1, E_FAIL);
        
        // can "send" ever return 0?
        ChkIfA(sent == 0, E_UNEXPECTED);
        
        pConn->_txCount += sent;
        
        // txCount should never exceed the total output message size, right?
        ASSERT(pConn->_txCount <= bytestotal);
        
        if (pConn->_txCount >= bytestotal)
        {
            pConn->_state = ConnectionState_Closing;
            
            shutdown(sock, SHUT_WR);
            // go back to listening for read events
            ChkA(EpollCtrl(sock, EPOLL_CLIENT_READ_EVENT_SET));

            ConsumeRemoteClose(pConn);
            
            // so we can't assume the connection is still alive.  And if it's not alive, pConn likely got deleted
            // either refetch from the hash tables, or invent an out parameter on WriteBytesForConnection and ConsumeRemoteClose to better propagate the close state of the connection
            pConn = NULL;

            
            break;
        }
        // loop back and try to send the remaining bytes
    }
    
Cleanup:    
    if ((FAILED(hr) || fForceClose))
    {
        CloseConnection(pConn);
    }
    
    return hr;
}

HRESULT CTCPStunThread::ConsumeRemoteClose(StunConnection* pConn)
{
    uint8_t buffer[MAX_STUN_MESSAGE_SIZE];
    HRESULT hr = S_OK;
    int ret;
    int err;
    
    ASSERT(pConn != NULL);
    int sock = pConn->_stunsocket.GetSocketHandle();
    
    ASSERT(sock != -1);
    
    while (true)
    {
        ret = ::recv(sock, buffer, sizeof(buffer), 0);
        err = errno;
        
        if ((ret < 0) && ((errno == EWOULDBLOCK) || (errno == EAGAIN)))
        {
            // still waiting
            hr = S_FALSE;
            break;
        }
        
        Logging::LogMsg(LL_VERBOSE, "ConsumeRemoteClose.  recv for socket %d returned %d (errno=%d)", sock, ret, (ret<0)?err:0);
        
        if (ret <= 0)
        {
            // whether it was a clean error (0) or some other error, we are done
            // that's it, we're done
            CloseConnection(pConn);
            pConn = NULL;
            break;
        }
    }
    
    return hr;
}



void CTCPStunThread::CloseConnection(StunConnection* pConn)
{
    if (pConn)
    {
        int sock = pConn->_stunsocket.GetSocketHandle();
        
        Logging::LogMsg(LL_VERBOSE, "Closing socket %d\n", sock);
        
        DetachFromEpoll(pConn->_stunsocket.GetSocketHandle());
        pConn->_stunsocket.Close();
        
        // now figure out which hash table we were in
        if (pConn->_idHashTable == 1)
        {
            VERIFY(_hashConnections1.Remove(sock) != -1);
        }
        else if (pConn->_idHashTable == 2)
        {
            VERIFY(_hashConnections2.Remove(sock) != -1);
        }
        else
        {
            ASSERT(pConn->_idHashTable == -1);
        }
        
        _connectionpool.ReleaseConnection(pConn);
    }
}


void CTCPStunThread::CloseAllConnections(StunThreadConnectionMap* pConnMap)
{
    StunThreadConnectionMap::Item* pItem = pConnMap->LookupByIndex(0);
    while (pItem)
    {
        CloseConnection(pItem->value);
        pItem = pConnMap->LookupByIndex(0);
    }
}

void CTCPStunThread::SweepDeadConnections()
{
    time_t timeCurrent = time(NULL);
    StunThreadConnectionMap* pSwap = NULL;


    // todo - make the timeout scale to the number of active connections
    if ((timeCurrent - _timeLastSweep) >= c_sweepTimeoutSeconds)
    {
        if  (_pOldConnList->Size())
        {
            Logging::LogMsg(LL_VERBOSE, "SweepDeadConnections closing %d connections", _pOldConnList->Size());
        }
        
        CloseAllConnections(_pOldConnList);
        
        _timeLastSweep = time(NULL);
        
        
        pSwap = _pOldConnList;
        _pOldConnList = _pNewConnList;
        _pNewConnList = pSwap;
    }
}


void CTCPStunThread::ThreadCleanup()
{
    CloseAllConnections(_pOldConnList);
    CloseAllConnections(_pNewConnList);
}





// ------------------------------------------------------------------

CTCPServer::CTCPServer()
{
    for (size_t i = 0; i < ARRAYSIZE(_threads); i++)
    {
        _threads[i] = NULL;
    }
}

CTCPServer::~CTCPServer()
{
    Logging::LogMsg(LL_DEBUG, "~CTCPServer() - exiting");
    Stop();
}


HRESULT CTCPServer::Initialize(const CStunServerConfig& config)
{
    HRESULT hr = S_OK;
    TransportAddressSet tsaListen;
    TransportAddressSet tsaHandler;
    
    ChkIfA(_threads[0] != NULL, E_UNEXPECTED); // we can't already be initialized, right?
    

    // tsaHandler is sort of a hack for TCP.  It's really just a glorified indication to the the
    // CStunRequestHandler code to figure out if can offer a CHANGED-ADDRESS attribute.
    
    tsaHandler.set[RolePP].fValid = config.fHasPP;
    tsaHandler.set[RolePP].addr = config.addrPP;

    tsaHandler.set[RolePA].fValid = config.fHasPA;
    tsaHandler.set[RolePA].addr = config.addrPA;

    tsaHandler.set[RoleAP].fValid = config.fHasAP;
    tsaHandler.set[RoleAP].addr = config.addrAP;

    tsaHandler.set[RoleAA].fValid = config.fHasAA;
    tsaHandler.set[RoleAA].addr = config.addrAA;    
    
    if (config.fMultiThreadedMode == false)
    {
        tsaListen = tsaHandler;
        _threads[0] = new CTCPStunThread();
        
        // todo - max connections needs to be a config param!        
        // todo - create auth
        ChkA(_threads[0]->Init(tsaListen, tsaHandler, NULL, 1000));
    }
    else
    {
        for (int threadindex = 0; threadindex < 4; threadindex++)
        {
            memset(&tsaListen, '\0', sizeof(tsaListen));
            
            if (tsaHandler.set[threadindex].fValid)
            {
                tsaListen.set[threadindex] = tsaHandler.set[threadindex];
                
                _threads[threadindex] = new CTCPStunThread();
                
                // todo - max connections needs to be a config param!        
                // todo - create auth
                Chk(_threads[threadindex]->Init(tsaListen, tsaHandler, NULL, 1000));
            }
        }
    }
    
Cleanup:
    if (FAILED(hr))
    {
        Shutdown();
    }
    return hr;
}

HRESULT CTCPServer::Shutdown()
{
    for (int role = (int)RolePP; role <= (int)RoleAA; role++)
    {
        delete _threads[role];
        _threads[role] = NULL;
    }
    return S_OK;
}

HRESULT CTCPServer::Start()
{
    HRESULT hr = S_OK;
    
    for (int role = (int)RolePP; role <= (int)RoleAA; role++)
    {
        if (_threads[role])
        {
            ChkA(_threads[role]->Start());
        }
    }
Cleanup:
    return hr;
}

HRESULT CTCPServer::Stop()
{
    // for now shutdown and stop are equivalent
    // we don't really support restarting a server anyway
    // todo - clean this up
    Shutdown();
    return S_OK;
}





