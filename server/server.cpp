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



CStunServerConfig::CStunServerConfig() :
fHasPP(false),
fHasPA(false),
fHasAP(false),
fHasAA(false),
fMultiThreadedMode(false),
fTCP(false),
nMaxConnections(0), // zero means default
fEnableDosProtection(false),
fReuseAddr(false)
{
    ;
}



CStunServer::CStunServer() :
_arrSockets() // zero-init
{
    ;
}

CStunServer::~CStunServer()
{
    Shutdown();
}

HRESULT CStunServer::AddSocket(TransportAddressSet* pTSA, SocketRole role, const CSocketAddress& addrListen, const CSocketAddress& addrAdvertise, bool fSetReuseFlag)
{
    HRESULT hr = S_OK;
    
    ASSERT(IsValidSocketRole(role));
    
    Chk(_arrSockets[role].UDPInit(addrListen, role, fSetReuseFlag));
    ChkA(_arrSockets[role].EnablePktInfoOption(true));


#ifdef DEBUG
    {
        CSocketAddress addrLocal = _arrSockets[role].GetLocalAddress();

        // addrListen is the address we asked the socket to listen on via a call to bind()
        // addrLocal is the socket address returned by getsockname after the socket is binded

        // I can't think of any case where addrListen != addrLocal
        // the ports will be different if addrListen.GetPort() is 0, but that
        // should never happen.
        
        // but if the assert below fails, I want to know about it
        ASSERT(addrLocal.IsSameIP_and_Port(addrListen));
    }
#endif

    pTSA->set[role].fValid = true;    
    if (addrAdvertise.IsIPAddressZero() == false)
    {
        // set the TSA for this socket to what the configuration wants us to advertise this address for in ORIGIN and OTHER address attributes
        pTSA->set[role].addr = addrAdvertise;
        pTSA->set[role].addr.SetPort(addrListen.GetPort()); // use the original port
    }
    else
    {
        pTSA->set[role].addr = addrListen; // use the socket's IP and port (OK if this is INADDR_ANY)
    }
    
Cleanup:
    return hr;
}

HRESULT CStunServer::Initialize(const CStunServerConfig& config)
{
    HRESULT hr = S_OK;
    int socketcount = 0;
    CRefCountedPtr<IStunAuth> _spAuth;
    TransportAddressSet tsa = {};
    boost::shared_ptr<RateLimiter> spLimiter;

    // cleanup any thing that's going on now
    Shutdown();
    
    // optional code: create an authentication provider and initialize it here (if you want authentication)
    // set the _spAuth member to reference it
    // Chk(CYourAuthProvider::CreateInstanceNoInit(&_spAuth));
    
    // Create the sockets and initialize the TSA thing
    if (config.fHasPP)
    {
        Chk(AddSocket(&tsa, RolePP, config.addrPP, config.addrPrimaryAdvertised, config.fReuseAddr));
        socketcount++;
    }

    if (config.fHasPA)
    {
        Chk(AddSocket(&tsa, RolePA, config.addrPA, config.addrPrimaryAdvertised, config.fReuseAddr));
        socketcount++;
    }

    if (config.fHasAP)
    {
        Chk(AddSocket(&tsa, RoleAP, config.addrAP, config.addrAlternateAdvertised, config.fReuseAddr));
        socketcount++;
    }

    if (config.fHasAA)
    {
        Chk(AddSocket(&tsa, RoleAA, config.addrAA, config.addrAlternateAdvertised, config.fReuseAddr));
        socketcount++;
    }

    ChkIf(socketcount == 0, E_INVALIDARG);

    if (config.fEnableDosProtection)
    {
        Logging::LogMsg(LL_DEBUG, "Creating rate limiter for ddos protection\n");
        // hard coding to 25000 ip addresses
        spLimiter = boost::shared_ptr<RateLimiter>(new RateLimiter(25000, config.fMultiThreadedMode));
    }

    if (config.fMultiThreadedMode == false)
    {
        Logging::LogMsg(LL_DEBUG, "Configuring single threaded mode\n");
        
        // create one thread for all the sockets
        CStunSocketThread* pThread = new CStunSocketThread();
        ChkIf(pThread==NULL, E_OUTOFMEMORY);

        _threads.push_back(pThread);
        
        Chk(pThread->Init(_arrSockets, &tsa, _spAuth, (SocketRole)-1, spLimiter));
    }
    else
    {
        Logging::LogMsg(LL_DEBUG, "Configuring multi-threaded mode\n");

        // one thread for every socket
        CStunSocketThread* pThread = NULL;
        for (size_t index = 0; index < ARRAYSIZE(_arrSockets); index++)
        {
            if (_arrSockets[index].IsValid())
            {
                SocketRole rolePrimaryRecv = _arrSockets[index].GetRole();
                ASSERT(rolePrimaryRecv == (SocketRole)index);
                pThread = new CStunSocketThread();
                ChkIf(pThread==NULL, E_OUTOFMEMORY);
                _threads.push_back(pThread);
                Chk(pThread->Init(_arrSockets, &tsa, _spAuth, rolePrimaryRecv, spLimiter));
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

HRESULT CStunServer::Shutdown()
{
    size_t len;

    Stop();

    // release the sockets and the thread

    for (size_t index = 0; index < ARRAYSIZE(_arrSockets); index++)
    {
        _arrSockets[index].Close();
    }

    len = _threads.size();
    for (size_t index = 0; index < len; index++)
    {
        CStunSocketThread* pThread = _threads[index];
        delete pThread;
        _threads[index] = NULL;
    }
    _threads.clear();
    
    _spAuth.ReleaseAndClear();
    
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
        if (pThread != NULL)
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
        if (pThread != NULL)
        {
            // set the "exit flag" that each thread looks at when it wakes up from waiting
            pThread->SignalForStop(false);
        }
    }


    for (size_t index = 0; index < len; index++)
    {
        CStunSocketThread* pThread = _threads[index];

        // Post a bunch of empty buffers to get the threads unblocked from whatever socket call they are on
        // In multi-threaded mode, this may wake up a different thread.  But that's ok, since all threads start and stop together
        if (pThread != NULL)
        {
            pThread->SignalForStop(true);
        }
    }

    for (size_t index = 0; index < len; index++)
    {
        CStunSocketThread* pThread = _threads[index];

        if  (pThread != NULL)
        {
            pThread->WaitForStopAndClose();
        }
    }


    return S_OK;
}




