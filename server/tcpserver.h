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


#ifndef STUN_TCP_SERVER_H
#define	STUN_TCP_SERVER_H

#include "stuncore.h"
#include "stunauth.h"
#include "server.h"
#include "fasthash.h"
#include "messagehandler.h"


enum StunConnectionState
{
    ConnectionState_Idle,
    ConnectionState_Receiving,
    ConnectionState_Transmitting,
    ConnectionState_Closing,     // shutdown has been called, waiting for close notification on other end
};


struct StunConnection
{
    time_t _timeStart;
    StunConnectionState _state;
    CStunSocket _stunsocket;
    CStunMessageReader _reader;
    CRefCountedBuffer _spReaderBuffer;
    CRefCountedBuffer _spOutputBuffer;  // contains the response
    size_t _txCount;     // number of bytes of response transmitted thus far
    int _idHashTable; // hints at which hash table the connection got inserted into
};



class CTCPStunThread
{
    static const int c_sweepTimeoutSeconds = 60;
    static const int c_sweepTimeoutMilliseconds = c_sweepTimeoutSeconds * 1000;
    
    int _pipe[2];
    
    HRESULT CreatePipes();
    HRESULT NotifyThreadViaPipe();
    void ClosePipes();
    
    int _epoll;
    bool _fListenSocketOnEpoll;
    HRESULT CreateEpoll();
    void CloseEpoll();
    
    enum ClientEpollMode
    {
        WantReadEvents =  1,  
        WantWriteEvents = 2,
    };

    // epoll helpers
    HRESULT AddSocketToEpoll(int sock, uint32_t events);
    HRESULT AddClientSocketToEpoll(int sock);
    HRESULT DetachFromEpoll(int sock);
    HRESULT EpollCtrl(int sock, uint32_t events);
    HRESULT SetListenSocketOnEpoll(bool fEnable);

    CSocketAddress _addrListen;
    CStunSocket _socketListen;
    HRESULT CreateListenSocket();
    void CloseListenSocket();
    
    
    bool _fNeedToExit;
    CRefCountedPtr<IStunAuth> _spAuth;
    SocketRole _role;
    
    TransportAddressSet _tsa;
    int _maxConnections;
    
    pthread_t _pthread;
    bool _fThreadIsValid;
    
    // this is the function that runs in a thread
    void Run();
    
    void Reset();
    
    static void* ThreadFunction(void* pThis);
    
    // ---------------------------------------------------------------
    // thread data
    
    // maps socket back to connection
    typedef FastHashDynamic<int, StunConnection*> StunThreadConnectionMap;
    
    StunThreadConnectionMap _hashConnections1;
    StunThreadConnectionMap _hashConnections2;
    
    StunThreadConnectionMap* _pNewConnList;
    StunThreadConnectionMap* _pOldConnList;
    time_t _timeLastSweep;
    
    
    // buffer pool helpers
    StunConnection* CreateNewConnection(int sock);
    void ReleaseConnection(StunConnection* pConn);
    
    
    StunConnection* AcceptConnection();

    void ProcessConnectionEvent(int sock, uint32_t eventflags);
    
    HRESULT ReceiveBytesForConnection(StunConnection* pConn);
    HRESULT WriteBytesForConnection(StunConnection* pConn);
    HRESULT ConsumeRemoteClose(StunConnection* pConn);
    
    void CloseAllConnections(StunThreadConnectionMap* pConnMap);
    void SweepDeadConnections();
    void ThreadCleanup();
    bool IsTimeoutNeeded();
    bool IsConnectionCountAtMax();
    void CloseConnection(StunConnection* pConn);
    
    // thread members
    
    // ---------------------------------------------------------------
    
public:
    CTCPStunThread();
    ~CTCPStunThread();
    
    HRESULT Init(const CSocketAddress& addrListen, IStunAuth* pAuth, SocketRole role, int maxConnections);
    HRESULT Start();
    HRESULT Stop();
};






#endif	/* SERVER_H */

