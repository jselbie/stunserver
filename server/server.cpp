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
#include "server.h"
#include "ratelimiter.h"
#include "stunsocket.h"


CStunServerConfig::CStunServerConfig() :
nThreading(0),
nMaxConnections(0), // zero means default
fEnableDosProtection(false),
fReuseAddr(false),
fIsFullMode(false),
fTCP(false)
{
    ;
}

CStunServer::CStunServer() :
_tsa() // zero-init
{
    ;
}

CStunServer::~CStunServer()
{
    Shutdown();
}

HRESULT CStunServer::CreateSocket(SocketRole role, const CSocketAddress& addr, bool fReuseAddr)
{
    HRESULT hr = S_OK;
    auto spSocket = std::make_shared<CStunSocket>();
    Chk(spSocket->UDPInit(addr, role, fReuseAddr));
    ChkA(spSocket->EnablePktInfoOption(true)); // todo - why is this not always set inside UDPInit
    ChkA(spSocket->SetRecvTimeout(10000));
    _sockets.push_back(spSocket);
Cleanup:
    return hr;
}

HRESULT CStunServer::InitializeTSA(const CStunServerConfig& config)
{
    _tsa = {};

    CSocketAddress addr[4] = {config.addrPP, config.addrPA, config.addrAP, config.addrAA};
    CSocketAddress advertised[4] = {config.addrPrimaryAdvertised, config.addrPrimaryAdvertised, config.addrAlternateAdvertised, config.addrAlternateAdvertised};
    bool validity[4] = {true, config.fIsFullMode, config.fIsFullMode, config.fIsFullMode};

    static_assert(RolePP == (SocketRole)0, "RolePP expected to be 0");
    static_assert(RolePA == (SocketRole)1, "RolePA expected to be 1");
    static_assert(RoleAP == (SocketRole)2, "RoleAP expected to be 2");
    static_assert(RoleAA == (SocketRole)3, "RoleAA expected to be 3");

    for (size_t i = 0; i < 4; i++)
    {
        if (validity[i])
        {   
            _tsa.set[i].fValid = true;
            if (advertised[i].IsIPAddressZero() == false)
            {
                // set the TSA for this socket to what the configuration wants us to advertise this address for in ORIGIN and OTHER address attributes
                _tsa.set[i].addr = advertised[i];
                _tsa.set[i].addr.SetPort(addr[i].GetPort());
            }
            else
            {
                // use the socket's IP and port (OK if this is INADDR_ANY)
                // the message handler code will use the local ip for ORIGIN
                _tsa.set[i].addr = _sockets[i]->GetLocalAddress();
            }
        }
    }
    return S_OK;
}

HRESULT CStunServer::CreateSockets(const CStunServerConfig& config)
{
    // four possible config types:
    //   1. basic mode with 1 thread and 1 socket
    //   2. basic mode with N threads and N sockets
    //   3. full mode with 1 thread and 4 sockets
    //   4. full mode with 4N threads and 4N sockets

    // 1 and 2 are equivalent, so when we are in basic mode, just assume config.nThreading == 0 is the the same as config.nThreading == 1
    // So really, only three configs: basic, full single threaded, and full multi-threaded

    HRESULT hr = S_OK;
    uint32_t numberOfThreads = (config.nThreading == 0) ? 1 : config.nThreading;
    bool fReuseAddr = config.fReuseAddr || config.nThreading > 0; // allow SO_REUSEPORT to be set, even if threading was explicitly set to 1

    ASSERT(_sockets.size() == 0);
    _sockets.clear();

    if (config.fIsFullMode == false)
    {
        for (size_t i = 0; i < numberOfThreads; i++)
        {
            Chk(CreateSocket(RolePP, config.addrPP, fReuseAddr));
        }
    }
    else
    {
        // in full mode, we create 4 * numberOfThreads sockets
        for (size_t i = 0; i < numberOfThreads; i++)
        {
            Chk(CreateSocket(RolePP, config.addrPP, fReuseAddr));
            Chk(CreateSocket(RolePA, config.addrPA, fReuseAddr));
            Chk(CreateSocket(RoleAP, config.addrAP, fReuseAddr));
            Chk(CreateSocket(RoleAA, config.addrAA, fReuseAddr));
        }
    }

Cleanup:
    return hr;
}

HRESULT CStunServer::Initialize(const CStunServerConfig& config)
{
    HRESULT hr = S_OK;
    std::shared_ptr<RateLimiter> spLimiter;
    size_t numberOfThreads = 1;
    _spAuth = nullptr;

    // cleanup any thing that's going on now
    Shutdown();
    
    // optional code: create an authentication provider and initialize it here (if you want authentication)
    // set the _spAuth member to reference it
    // _spAuth = std::make_shared<CYourAuthProvider>();

    Chk(CreateSockets(config));
    ChkA(InitializeTSA(config));

    ChkIfA(_sockets.size() == 0, E_UNEXPECTED);
    ChkIfA((config.fIsFullMode && _sockets.size() < 4), E_UNEXPECTED);
    ChkIfA((config.fIsFullMode && _sockets.size() % 4), E_UNEXPECTED);

    if ((config.fIsFullMode == false) || (config.nThreading > 0))
    {
        numberOfThreads = _sockets.size();
    }

    if (config.fEnableDosProtection)
    {
        // hard coding to 25000 ip addresses
        bool fMultiThreaded = (numberOfThreads > 1);
        Logging::LogMsg(LL_DEBUG, "Creating rate limiter for ddos protection (%s)\n", fMultiThreaded ? "multi-threaded" : "single-threaded");
        spLimiter = std::shared_ptr<RateLimiter>(new RateLimiter(25000, fMultiThreaded));
    }

    Logging::LogMsg(LL_DEBUG, "Configuring multi threaded mode with %d sockets on %d threads\n", _sockets.size(), numberOfThreads);

    for (size_t i = 0; i < numberOfThreads; i++)
    {
        std::vector<std::shared_ptr<CStunSocket>> arrayOfFourSockets;
        SocketRole role;

        if (config.fIsFullMode)
        {
            size_t baseindex = (i/4) * 4;
            arrayOfFourSockets.push_back(_sockets[baseindex + 0]);
            arrayOfFourSockets.push_back(_sockets[baseindex + 1]);
            arrayOfFourSockets.push_back(_sockets[baseindex + 2]);
            arrayOfFourSockets.push_back(_sockets[baseindex + 3]);
            ASSERT(arrayOfFourSockets[0]->GetRole() == RolePP);
            ASSERT(arrayOfFourSockets[2]->GetRole() == RolePA);
            ASSERT(arrayOfFourSockets[3]->GetRole() == RoleAP);
            ASSERT(arrayOfFourSockets[3]->GetRole() == RoleAA);

            if (config.nThreading == 0)
            {
                role = (SocketRole)-1; // the thread will recognize this as "listen on all four sockets"
                ASSERT(numberOfThreads == 1);
            }
            else
            {
                role = (SocketRole)(i % 4);
            }
        }
        else
        {
            arrayOfFourSockets.push_back(_sockets[i]);
            arrayOfFourSockets.push_back(nullptr);
            arrayOfFourSockets.push_back(nullptr);
            arrayOfFourSockets.push_back(nullptr);
            role = RolePP;

            ASSERT(_sockets[i]->GetRole() == RolePP);
            ASSERT(_sockets[i]->IsValid());
        }

        CStunSocketThread* pThread = new CStunSocketThread();
        _threads.push_back(pThread);
        Chk(pThread->Init(arrayOfFourSockets, _tsa, _spAuth, role, spLimiter));
    }


Cleanup:

    if (FAILED(hr))
    {
        Shutdown();
    }

    return hr;

}

HRESULT CStunServer::Shutdown()
{
    Stop();

    for (auto pThread : _threads)
    {
        delete pThread;
    }
    _threads.clear();

    for (auto spSocket : _sockets)
    {
        if (spSocket != nullptr)
        {
            spSocket->Close();
        }
    }
    _sockets.clear();

    _spAuth.reset();
    
    return S_OK;
}


HRESULT CStunServer::Start()
{
    HRESULT hr = S_OK;
    size_t len = _threads.size();

    ChkIfA(len == 0, E_UNEXPECTED);

    for (size_t index = 0; index < len; index++)
    {
        CStunSocketThread* pThread = _threads[index];
        if (pThread != nullptr)
        {
            // set the "exit flag" that each thread looks at when it wakes up from waiting
            ChkA(pThread->Start());
        }
    }

Cleanup:
    if (FAILED(hr))
    {
        Stop();
    }

    return hr;
}

HRESULT CStunServer::Stop()
{
    size_t len = _threads.size();

    for (size_t index = 0; index < len; index++)
    {
        CStunSocketThread* pThread = _threads[index];
        if (pThread != nullptr)
        {
            // set the "exit flag" that each thread looks at when it wakes up from waiting
            pThread->SignalForStop();
        }
    }

    PostWakeupMessages();

    for (size_t index = 0; index < len; index++)
    {
        CStunSocketThread* pThread = _threads[index];

        if (pThread != nullptr)
        {
            pThread->WaitForStopAndClose();
        }
    }

    return S_OK;
}

void CStunServer::PostWakeupMessages()
{
    // This is getting harder to maintain.

    // When all the threads shared the same socket, we just had to invoke sendto once for each thread.
    // Then each thread would wakeup from its recvfrom call and detect the exit condition.

    // In the new multi-threaded design mode, where each socket has the SO_REUSEPORT option set, the packets
    // from the same source ip:port get queued into the same listening socket. (And when the socket
    // closes, there's no requeuing to another listening socket). And there's
    // some hash table lookup by which the OS maps to each.  So we can't guarantee that sending
    // N packets will get received by all N threads.

    // Workarounds: 
    //    1. Use a random port for each send, and loop multiple times
    //    2. Use a SO_RCVTIMEO on each socket so that they eventually all wake up
    //    3. Combo of 1 and 2 with a conditional variable

    size_t count = _sockets.size();


    for (size_t i = 0; i < count; i++)
    {
        CStunSocket sock;
        CSocketAddress addrLocal;

        if (_sockets[i]->GetLocalAddress().GetFamily() == AF_INET)
        {
            addrLocal = CSocketAddress(0,0);
        }
        else
        {
            sockaddr_in6 addr6 = {};
            addr6.sin6_family = AF_INET6;
            addrLocal = CSocketAddress(addr6);
        }

        // bind socket to port 0 (auto assign), using the same family as the socket we are trying to unblock
        HRESULT hr = sock.UDPInit(addrLocal, RolePP, false);
        ASSERT(SUCCEEDED(hr));

        if (SUCCEEDED(hr))
        {
            char data = 'x';
            CSocketAddress addr(_sockets[i]->GetLocalAddress());

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

            ::sendto(sock.GetSocketHandle(), &data, 1, 0, addr.GetSockAddr(), addr.GetSockAddrLength());
        }
    }

}
