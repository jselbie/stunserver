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
#include "socketrole.h" // so we can re-use the "SocketRole" definitions again
#include "stunsocket.h"
#include "cmdlineparser.h"
#include "recvfromex.h"
#include "resolvehostname.h"
#include "stringhelper.h"
#include "adapters.h"
#include "oshelper.h"
#include "prettyprint.h"

// These files are in ../resources
#include "stunclient.txtcode"
#include "stunclient_lite.txtcode"


struct ClientCmdLineArgs
{
    std::string strRemoteServer;
    std::string strRemotePort;
    std::string strLocalAddr;
    std::string strLocalPort;
    std::string strMode;
    std::string strFamily;
    std::string strProtocol;
    std::string strVerbosity;
    std::string strHelp;
};

struct ClientSocketConfig
{
    int family;
    int socktype;
    CSocketAddress addrLocal;
};

void DumpConfig(StunClientLogicConfig& config, ClientSocketConfig& socketConfig)
{
    std::string strAddr;


    Logging::LogMsg(LL_DEBUG, "config.fBehaviorTest = %s", config.fBehaviorTest?"true":"false");
    Logging::LogMsg(LL_DEBUG, "config.fFilteringTest = %s", config.fFilteringTest?"true":"false");
    Logging::LogMsg(LL_DEBUG, "config.timeoutSeconds = %d", config.timeoutSeconds);
    Logging::LogMsg(LL_DEBUG, "config.uMaxAttempts = %d", config.uMaxAttempts);

    config.addrServer.ToString(&strAddr);
    Logging::LogMsg(LL_DEBUG, "config.addrServer = %s",  strAddr.c_str());

    socketConfig.addrLocal.ToString(&strAddr);
    Logging::LogMsg(LL_DEBUG, "socketconfig.addrLocal = %s", strAddr.c_str());

}

void PrintUsage(bool fSummaryUsage)
{
    size_t width = GetConsoleWidth();
    const char* psz = fSummaryUsage ? stunclient_lite_text : stunclient_text;

    // save some margin space
    if (width > 2)
    {
        width -= 2;
    }

    PrettyPrint(psz, width);
}



HRESULT CreateConfigFromCommandLine(ClientCmdLineArgs& args, StunClientLogicConfig* pConfig, ClientSocketConfig* pSocketConfig)
{
    HRESULT hr = S_OK;
    StunClientLogicConfig& config = *pConfig;
    ClientSocketConfig& socketconfig = *pSocketConfig;
    int ret;
    uint16_t localport = 0;
    uint16_t remoteport = 0;
    int nPort = 0;
    char szIP[100];
    bool fTCP = false;


    config.fBehaviorTest = false;
    config.fFilteringTest = false;
    config.fTimeoutIsInstant = false;
    config.timeoutSeconds = 0; // use default
    config.uMaxAttempts = 0;

    socketconfig.family = AF_INET;
    socketconfig.socktype = SOCK_DGRAM;
    socketconfig.addrLocal = CSocketAddress(0, 0);

    ChkIfA(pConfig == NULL, E_INVALIDARG);
    ChkIfA(pSocketConfig==NULL, E_INVALIDARG);


    // family (protocol type) ------------------------------------
    if (StringHelper::IsNullOrEmpty(args.strFamily.c_str())==false)
    {
        int optvalue = atoi(args.strFamily.c_str());
        switch (optvalue)
        {
            case 4: socketconfig.family = AF_INET; break;
            case 6: socketconfig.family = AF_INET6; break;
            default:
            {
                Logging::LogMsg(LL_ALWAYS, "Family option must be either 4 or 6");
                Chk(E_INVALIDARG);
            }
        }
    }
    
    // protocol --------------------------------------------
    StringHelper::ToLower(args.strProtocol);
    if (StringHelper::IsNullOrEmpty(args.strProtocol.c_str()) == false)
    {
        if ((args.strProtocol != "udp") && (args.strProtocol != "tcp"))
        {
            Logging::LogMsg(LL_ALWAYS, "Only udp and tcp are supported protocol versions");
            Chk(E_INVALIDARG);
        }
        
        if (args.strProtocol == "tcp")
        {
            fTCP = true;
            socketconfig.socktype = SOCK_STREAM;
            config.uMaxAttempts = 1;
        }
    }

    // remote port ---------------------------------------------
    if (StringHelper::IsNullOrEmpty(args.strRemotePort.c_str()) == false)
    {
        ret = StringHelper::ValidateNumberString(args.strRemotePort.c_str(), 1, 0xffff, &nPort);
        if (ret < 0)
        {
            Logging::LogMsg(LL_ALWAYS, "Remote port must be between 1 - 65535");
            Chk(E_INVALIDARG);
        }
        remoteport = (uint16_t)(unsigned int)nPort;
    }
    else
    {
        remoteport = DEFAULT_STUN_PORT;
    }


    // remote server -----------------------------------------
    if (StringHelper::IsNullOrEmpty(args.strRemoteServer.c_str()))
    {
        Logging::LogMsg(LL_ALWAYS, "No server address specified");
        Chk(E_INVALIDARG);
    }
    
    hr = ::ResolveHostName(args.strRemoteServer.c_str(), socketconfig.family, false, &config.addrServer);
    
    if (FAILED(hr))
    {
        Logging::LogMsg(LL_ALWAYS, "Unable to resolve hostname for %s", args.strRemoteServer.c_str());
        Chk(hr);
    }
    
    config.addrServer.ToStringBuffer(szIP, ARRAYSIZE(szIP));
    Logging::LogMsg(LL_DEBUG, "Resolved %s to %s", args.strRemoteServer.c_str(), szIP);
    config.addrServer.SetPort(remoteport);


    // local port --------------------------------------------
    if (StringHelper::IsNullOrEmpty(args.strLocalPort.c_str()) == false)
    {
        ret = StringHelper::ValidateNumberString(args.strLocalPort.c_str(), 1, 0xffff, &nPort);
        if (ret < 0)
        {
            Logging::LogMsg(LL_ALWAYS, "Local port must be between 1 - 65535");
            Chk(E_INVALIDARG);
        }
        localport = (uint16_t)(unsigned int)nPort;
    }

    // local address ------------------------------------------
    if (StringHelper::IsNullOrEmpty(args.strLocalAddr.c_str()) == false)
    {
        hr = GetSocketAddressForAdapter(socketconfig.family, args.strLocalAddr.c_str(), localport, &socketconfig.addrLocal);
        if (FAILED(hr))
        {
            Logging::LogMsg(LL_ALWAYS, "Unable to find matching adapter or interface for local address option");
            Chk(hr);
        }
    }
    else
    {
        if (socketconfig.family == AF_INET6)
        {
            sockaddr_in6 addr6 = {};
            addr6.sin6_family = AF_INET6;
            socketconfig.addrLocal = CSocketAddress(addr6);
            socketconfig.addrLocal.SetPort(localport);
        }
        else
        {
            socketconfig.addrLocal = CSocketAddress(0,localport);
        }
    }

    // mode ---------------------------------------------
    StringHelper::ToLower(args.strMode);
    if (StringHelper::IsNullOrEmpty(args.strMode.c_str()) == false)
    {
        if (args.strMode == "basic")
        {
            ;
        }
        else if (args.strMode == "full")
        {
            config.fBehaviorTest = true;
            config.fFilteringTest = (fTCP == false); // impossible to to a filtering test in TCP
        }
        else if (args.strMode == "behavior")
        {
            config.fBehaviorTest = true;
        }
        else if (args.strMode == "filtering")
        {
            config.fFilteringTest = true;
        }
        else
        {
            Logging::LogMsg(LL_ALWAYS, "Mode option must be 'full', 'basic', 'behavior', or 'filtering'");
        }
    }



Cleanup:
    return hr;
}


void DumpResults(StunClientLogicConfig& config, StunClientResults& results)
{
    char szBuffer[100];
    const int buffersize = 100;
    std::string strResult;

    Logging::LogMsg(LL_ALWAYS, "Binding test: %s", results.fBindingTestSuccess?"success":"fail");
    if (results.fBindingTestSuccess)
    {
        results.addrLocal.ToStringBuffer(szBuffer, buffersize);
        Logging::LogMsg(LL_ALWAYS, "Local address: %s", szBuffer);

        results.addrMapped.ToStringBuffer(szBuffer, buffersize);
        Logging::LogMsg(LL_ALWAYS, "Mapped address: %s", szBuffer);
    }

    if (config.fBehaviorTest)
    {

        Logging::LogMsg(LL_ALWAYS, "Behavior test: %s", results.fBehaviorTestSuccess?"success":"fail");
        if (results.fBehaviorTestSuccess)
        {
            strResult = NatBehaviorToString(results.behavior);
            Logging::LogMsg(LL_ALWAYS, "Nat behavior: %s", strResult.c_str());
        }
    }

    if (config.fFilteringTest)
    {
        Logging::LogMsg(LL_ALWAYS, "Filtering test: %s", results.fFilteringTestSuccess?"success":"fail");
        if (results.fFilteringTestSuccess)
        {
            strResult = NatFilteringToString(results.filtering);
            Logging::LogMsg(LL_ALWAYS, "Nat filtering: %s", strResult.c_str());
        }
    }
}


void TcpClientLoop(StunClientLogicConfig& config, ClientSocketConfig& socketconfig)
{
    
    HRESULT hr = S_OK;
    CStunSocket stunsocket;
    CStunClientLogic clientlogic;
    int sock;
    CRefCountedBuffer spMsg(new CBuffer(1500));
    CRefCountedBuffer spMsgReader(new CBuffer(1500));
    CSocketAddress addrDest, addrLocal;
    HRESULT hrRet, hrResult;
    int ret;
    size_t bytes_sent, bytes_recv;
    size_t bytes_to_send, max_bytes_recv, remaining;
    uint8_t* pData=NULL;
    size_t readsize;
    CStunMessageReader reader;
    StunClientResults results;
    
   
    hr= clientlogic.Initialize(config);
    if (FAILED(hr))
    {
        Logging::LogMsg(LL_ALWAYS, "clientlogic.Initialize failed (hr == %x)", hr);
        Chk(hr);
    }
    
    
    while (true)
    {
    
        stunsocket.Close();
        hr = stunsocket.TCPInit(socketconfig.addrLocal, RolePP, true);
        if (FAILED(hr))
        {
            Logging::LogMsg(LL_ALWAYS, "Unable to create local socket for TCP connection (hr == %x)", hr);
            Chk(hr);
        }
        
        hrRet = clientlogic.GetNextMessage(spMsg, &addrDest, ::GetMillisecondCounter());
        
        if (hrRet == E_STUNCLIENT_RESULTS_READY)
        {
            // clean exit
            break;
        }

        // we should never get a "still waiting" return with TCP, because config.timeout is 0
        ASSERT(hrRet != E_STUNCLIENT_STILL_WAITING);
        
        if (FAILED(hrRet))
        {
            Chk(hrRet);
        }
        
        // connect to server
        sock = stunsocket.GetSocketHandle();
        
        ret = ::connect(sock, addrDest.GetSockAddr(), addrDest.GetSockAddrLength());
        
        if (ret == -1)
        {
            hrResult = ERRNOHR;
            Logging::LogMsg(LL_ALWAYS, "Can't connect to server (hr == %x)", hrResult);
            Chk(hrResult);
        }
        
        Logging::LogMsg(LL_DEBUG, "Connected to server");
        
        bytes_to_send = (int)(spMsg->GetSize());
        
        bytes_sent = 0;
        pData = spMsg->GetData();
        while (bytes_sent < bytes_to_send)
        {
            ret = ::send(sock, pData+bytes_sent, bytes_to_send-bytes_sent, 0);
            if (ret < 0)
            {
                hrResult = ERRNOHR;
                Logging::LogMsg(LL_ALWAYS, "Send failed (hr == %x)", hrResult);
                Chk(hrResult);
            }
            bytes_sent += ret;
        }
        
        Logging::LogMsg(LL_DEBUG, "Request sent - waiting for response");
        
        
        // consume the response
        reader.Reset();
        reader.GetStream().Attach(spMsgReader, true);
        pData = spMsg->GetData();
        bytes_recv = 0;
        max_bytes_recv = spMsg->GetAllocatedSize();
        remaining = max_bytes_recv;
        
        while (remaining > 0)
        {
            readsize = reader.HowManyBytesNeeded();
            
            if (readsize == 0)
            {
                break;
            }
            
            if (readsize > remaining)
            {
                // technically an error, but the client logic will figure it out
                ASSERT(false);
                break;
            }
            
            ret = ::recv(sock, pData+bytes_recv, readsize, 0);
            if (ret == 0)
            {
                // server cut us off before we got all the bytes we thought we were supposed to get?
                ASSERT(false);
                Logging::LogMsg(LL_ALWAYS, "Lost connection");
                break;
            }
            if (ret < 0)
            {
                hrResult = ERRNOHR;
                Logging::LogMsg(LL_ALWAYS, "Recv failed (hr == %x)", hrResult);
                Chk(hrResult);
            }
            
            reader.AddBytes(pData+bytes_recv, ret);
            bytes_recv += ret;
            remaining = max_bytes_recv - bytes_recv;
            spMsg->SetSize(bytes_recv);
        }
        
        
        // now feed the response into the client logic
        stunsocket.UpdateAddresses();
        addrLocal = stunsocket.GetLocalAddress();
        clientlogic.ProcessResponse(spMsg, addrDest, addrLocal);
    }
    
    stunsocket.Close();

    results.Init();
    clientlogic.GetResults(&results);
    ::DumpResults(config, results);
    
Cleanup:
    return;
    
}


HRESULT UdpClientLoop(StunClientLogicConfig& config, const ClientSocketConfig& socketconfig)
{
    HRESULT hr = S_OK;
    CStunSocket stunSocket;
    CRefCountedBuffer spMsg(new CBuffer(MAX_STUN_MESSAGE_SIZE));
    int sock = -1;
    CSocketAddress addrDest;   // who we send to
    CSocketAddress addrRemote; // who we
    CSocketAddress addrLocal;
    int ret;
    fd_set set;
    timeval tv = {};
    std::string strAddr;
    std::string strAddrLocal;
    StunClientResults results;

    CStunClientLogic clientlogic;


    hr = clientlogic.Initialize(config);
    
    if (FAILED(hr))
    {
        Logging::LogMsg(LL_ALWAYS, "Unable to initialize client: (error = x%x)", hr);
        Chk(hr);
    }

    hr = stunSocket.UDPInit(socketconfig.addrLocal, RolePP, false);
    if (FAILED(hr))
    {
        Logging::LogMsg(LL_ALWAYS, "Unable to create local socket: (error = x%x)", hr);
        Chk(hr);
    }
    

    stunSocket.EnablePktInfoOption(true);

    sock = stunSocket.GetSocketHandle();

    // let's get a loop going!

    while (true)
    {
        HRESULT hrRet;
        spMsg->SetSize(0);
        hrRet = clientlogic.GetNextMessage(spMsg, &addrDest, GetMillisecondCounter());

        if (SUCCEEDED(hrRet))
        {
            addrDest.ToString(&strAddr);
            ASSERT(spMsg->GetSize() > 0);
            
            if (Logging::GetLogLevel() >= LL_DEBUG)
            {
                std::string strAddr;
                addrDest.ToString(&strAddr);
                Logging::LogMsg(LL_DEBUG, "Sending message to %s", strAddr.c_str());
            }

            ret = ::sendto(sock, spMsg->GetData(), spMsg->GetSize(), 0, addrDest.GetSockAddr(), addrDest.GetSockAddrLength());

            if (ret <= 0)
            {
                Logging::LogMsg(LL_DEBUG, "ERROR.  sendto failed (errno = %d)", errno);
            }
            // there's not much we can do if "sendto" fails except time out and try again
        }
        else if (hrRet == E_STUNCLIENT_STILL_WAITING)
        {
            Logging::LogMsg(LL_DEBUG, "Continuing to wait for response...");
        }
        else if (hrRet == E_STUNCLIENT_RESULTS_READY)
        {
            break;
        }
        else
        {
            Logging::LogMsg(LL_DEBUG, "Fatal error (hr == %x)", hrRet);
            Chk(hrRet);
        }


        // now wait for a response
        spMsg->SetSize(0);
        FD_ZERO(&set);
        FD_SET(sock, &set);
        tv.tv_usec = 500000; // half-second
        tv.tv_sec = config.timeoutSeconds;

        ret = select(sock+1, &set, NULL, NULL, &tv);
        if (ret > 0)
        {
            ret = ::recvfromex(sock, spMsg->GetData(), spMsg->GetAllocatedSize(), MSG_DONTWAIT, &addrRemote, &addrLocal);
            if (ret > 0)
            {
                addrLocal.SetPort(stunSocket.GetLocalAddress().GetPort()); // recvfromex doesn't fill in the dest port value, only dest IP
                addrRemote.ToString(&strAddr);
                addrLocal.ToString(&strAddrLocal);
                Logging::LogMsg(LL_DEBUG, "Got response (%d bytes) from %s on interface %s", ret, strAddr.c_str(), strAddrLocal.c_str());
                spMsg->SetSize(ret);
                clientlogic.ProcessResponse(spMsg, addrRemote, addrLocal);
            }
        }
    }


    results.Init();
    clientlogic.GetResults(&results);

    DumpResults(config, results);


Cleanup:
    return hr;
}







int main(int argc, char** argv)
{
    CCmdLineParser cmdline;
    ClientCmdLineArgs args;
    StunClientLogicConfig config;
    ClientSocketConfig socketconfig;
    bool fError = false;
    uint32_t loglevel = LL_ALWAYS;
    

#ifdef DEBUG
    loglevel = LL_DEBUG;
#endif
    Logging::SetLogLevel(loglevel);
    

    cmdline.AddNonOption(&args.strRemoteServer);
    cmdline.AddNonOption(&args.strRemotePort);
    cmdline.AddOption("localaddr", required_argument,  &args.strLocalAddr);
    cmdline.AddOption("localport", required_argument, &args.strLocalPort);
    cmdline.AddOption("mode", required_argument, &args.strMode);
    cmdline.AddOption("family", required_argument, &args.strFamily);
    cmdline.AddOption("protocol", required_argument, &args.strProtocol);
    cmdline.AddOption("verbosity", required_argument, &args.strVerbosity);
    cmdline.AddOption("help", no_argument, &args.strHelp);

    if (argc <= 1)
    {
        PrintUsage(true);
        return -1;
    }
    
    cmdline.ParseCommandLine(argc, argv, 1, &fError);
    
    if (args.strHelp.length() > 0)
    {
        PrintUsage(false);
        return -2;
    }
    
    if (fError)
    {
        PrintUsage(true);
        return -3;
    }
    

    if (args.strVerbosity.length() > 0)
    {
        int level = atoi(args.strVerbosity.c_str());
        if (level >= 0)
        {
            Logging::SetLogLevel(level);
        }
    }

    if (FAILED(CreateConfigFromCommandLine(args, &config, &socketconfig)))
    {
        Logging::LogMsg(LL_ALWAYS, "Can't start client");
        PrintUsage(true);
        return -4;
    }

    DumpConfig(config, socketconfig);
    
    if (socketconfig.socktype == SOCK_STREAM)
    {
        TcpClientLoop(config, socketconfig);
    }
    else
    {
        UdpClientLoop(config, socketconfig);
    }

    return 0;
}
