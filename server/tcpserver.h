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
#include "stunconnection.h"
#include "polling.h"
#include "ratelimiter.h"



class CTCPStunThread
{
    static const int c_sweepTimeoutSeconds = 60;
    
    
    int _pipe[2];
    HRESULT CreatePipes();
    HRESULT NotifyThreadViaPipe();
    void ClosePipes();

    CRefCountedPtr<IPolling> _spPolling;
    bool _fListenSocketsOnEpoll;
    
    boost::shared_ptr<RateLimiter> _spLimiter;
    
    // epoll helpers
    HRESULT SetListenSocketsOnEpoll(bool fEnable);

    TransportAddressSet _tsaListen;  // this is not what gets passed to CStunRequestHandler, see _tsa below
    CStunSocket _socketListenArray[4];
    int _socketTable[4]; // same as _socketListenArray,but for quick lookup
    int _countSocks;
    HRESULT CreateListenSockets();
    void CloseListenSockets();
    CStunSocket* GetListenSocket(int sock);
    
    
    bool _fNeedToExit;
    CRefCountedPtr<IStunAuth> _spAuth;
    SocketRole _role;
    
    TransportAddressSet _tsa;  // this
    int _maxConnections;
    
    pthread_t _pthread;
    bool _fThreadIsValid;
    
    CConnectionPool _connectionpool;
    
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
    
    
    
    StunConnection* AcceptConnection(CStunSocket* pListenSocket);

    void ProcessConnectionEvent(int sock, uint32_t eventflags);
    
    HRESULT ReceiveBytesForConnection(StunConnection* pConn);
    HRESULT WriteBytesForConnection(StunConnection* pConn);
    
    void CloseAllConnections(StunThreadConnectionMap* pConnMap);
    void SweepDeadConnections();
    void ThreadCleanup();
    int GetTimeoutSeconds();
    bool IsConnectionCountAtMax();
    void CloseConnection(StunConnection* pConn);
    bool RateCheck(const CSocketAddress& addr);
    
    // thread members
    
    // ---------------------------------------------------------------
    
public:
    CTCPStunThread();
    ~CTCPStunThread();
    
    // tsaListen are the set of addresses we listen to connections on (either 1 address or 4 addresses)
    // tsaHandler is what gets passed to the CStunRequestHandler for formation of the "other-address" attribute
    HRESULT Init(const TransportAddressSet& tsaListen, const TransportAddressSet& tsaHandler, IStunAuth* pAuth, int maxConnections, boost::shared_ptr<RateLimiter>& spLimiter);
    HRESULT Start();
    HRESULT Stop();
};

class CTCPServer :
    public CBasicRefCount,
    public CObjectFactory<CTCPServer>,
    public IRefCounted
{
private:
    
    CTCPStunThread* _threads[4];
    
    CRefCountedPtr<IStunAuth> _spAuth;
    
    void InitTSA(TransportAddressSet* pTSA, SocketRole role, bool fValid, const CSocketAddress& addrListen, const CSocketAddress& addrAdvertise);
    
public:
    
    CTCPServer();
    virtual ~CTCPServer();
    
    
    HRESULT Initialize(const CStunServerConfig& config);
    HRESULT Shutdown();
    HRESULT Start();
    HRESULT Stop();
    
    ADDREF_AND_RELEASE_IMPL();    
    
};






#endif	/* SERVER_H */

