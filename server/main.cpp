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

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "stuncore.h"
#include "server.h"
#include "tcpserver.h"
#include "adapters.h"
#include "cmdlineparser.h"

#include <getopt.h>

#include "prettyprint.h"
#include "oshelper.h"
#include "stringhelper.h"


// these are auto-generated files made from markdown sources.  See ../resources
#include "stunserver.txtcode"
#include "stunserver_lite.txtcode"
#include "resolvehostname.h"


using namespace boost::property_tree;


void PrintUsage(bool fSummaryUsage)
{
    size_t width = GetConsoleWidth();
    const char* psz = fSummaryUsage ? stunserver_lite_text : stunserver_text;

    // save some margin space
    if (width > 2)
    {
        width -= 2;
    }

    PrettyPrint(psz, width);
}


// Because strerror_r has two versions (one that returns an int and another that returns char*)
// we wrap the result into an overloaded function
char* strerror_helper(int result, char* msg, int err)
{
    if (result != 0)
    {
        sprintf(msg, "Unknown error %d", err);
    }
    return msg;
}

char* strerror_helper(char* pszResult, char*, int)
{
    return pszResult;
}


void LogHR(uint16_t level, HRESULT hr)
{
    uint32_t facility = HRESULT_FACILITY(hr);
    char msg[400];
    const char* pMsg = NULL;
    bool fGotMsg = false;
    
    if (facility == FACILITY_ERRNO)
    {
        msg[0] = '\0';
        int err = (int)(HRESULT_CODE(hr));
        
        pMsg = strerror_helper(strerror_r(err, msg, ARRAYSIZE(msg)), msg, err);

        if (pMsg)
        {
            Logging::LogMsg(level, "Error: %s", pMsg);
            fGotMsg = true;
        }
        
        if (err == EADDRINUSE)
        {
            Logging::LogMsg(level, 
                "This error likely means another application is listening on one\n"
                "or more of the same ports you are attempting to configure this\n"
                "server to listen on.  Run \"netstat -a -p -t -u\" to see a list\n"
                "of all ports in use and associated process id for each");
        }
        
    }
    
    if (fGotMsg == false)
    {
        Logging::LogMsg(level, "Error: %x", hr);
    }
}



struct StartupArgs
{
    std::string strMode;
    std::string strPrimaryInterface;
    std::string strAltInterface;
    std::string strPrimaryPort;
    std::string strAltPort;
    std::string strPrimaryAdvertised;
    std::string strAlternateAdvertised;
    std::string strFamily;
    std::string strProtocol;
    std::string strHelp;
    std::string strVerbosity;
    std::string strMaxConnections;
    std::string strDosProtect;
    std::string strConfigFile;
    std::string strReuseAddr;
    
};

#define PRINTARG(member) Logging::LogMsg(LL_DEBUG, "%s = %s", #member, args.member.length() ? args.member.c_str() : "<empty>");

void DumpStartupArgs(StartupArgs& args)
{
    Logging::LogMsg(LL_DEBUG, "\n\n--------------------------");
    PRINTARG(strMode);
    PRINTARG(strPrimaryInterface);
    PRINTARG(strAltInterface);
    PRINTARG(strPrimaryAdvertised);
    PRINTARG(strAlternateAdvertised);
    PRINTARG(strPrimaryPort);
    PRINTARG(strAltPort);
    PRINTARG(strFamily);
    PRINTARG(strProtocol);
    PRINTARG(strHelp);
    PRINTARG(strVerbosity);
    PRINTARG(strMaxConnections);
    PRINTARG(strDosProtect);
    PRINTARG(strReuseAddr);
    Logging::LogMsg(LL_DEBUG, "--------------------------\n");
}



void DumpConfig(CStunServerConfig &config)
{
    std::string strSocket;


    if (config.fHasPP)
    {
        config.addrPP.ToString(&strSocket);
        Logging::LogMsg(LL_DEBUG, "PP = %s", strSocket.c_str());
    }
    if (config.fHasPA)
    {
        config.addrPA.ToString(&strSocket);
        Logging::LogMsg(LL_DEBUG, "PA = %s", strSocket.c_str());
    }
    if (config.fHasAP)
    {
        config.addrAP.ToString(&strSocket);
        Logging::LogMsg(LL_DEBUG, "AP = %s", strSocket.c_str());
    }
    if (config.fHasAA)
    {
        config.addrAA.ToString(&strSocket);
        Logging::LogMsg(LL_DEBUG, "AA = %s", strSocket.c_str());
    }
    
    if (config.addrPrimaryAdvertised.IsIPAddressZero() == false)
    {
        config.addrPrimaryAdvertised.ToString(&strSocket);
        Logging::LogMsg(LL_DEBUG, "Primary IP will be advertised as %s", strSocket.c_str());
    }
    
    if (config.addrAlternateAdvertised.IsIPAddressZero() == false)
    {
        config.addrAlternateAdvertised.ToString(&strSocket);
        Logging::LogMsg(LL_DEBUG, "Alternate IP will be advertised as %s", strSocket.c_str());
    }
    
    Logging::LogMsg(LL_DEBUG, "Protocol = %s", config.fTCP ? "TCP" : "UDP");
    if (config.fTCP && (config.nMaxConnections>0))
    {
        Logging::LogMsg(LL_DEBUG, "Max TCP Connections per thread: %d", config.nMaxConnections);
    }
}


HRESULT ResolveAdapterName(bool fPrimary, int family, std::string& strAdapterName, uint16_t port, CSocketAddress* pAddr)
{
    HRESULT hr = S_OK;


    if (strAdapterName.length() == 0)
    {
        hr = GetBestAddressForSocketBind(fPrimary, family, port, pAddr);
        if (SUCCEEDED(hr))
        {
            pAddr->ToString(&strAdapterName);
            // strip off the port suffix
            size_t x = strAdapterName.find_last_of(':');
            if (x != std::string::npos)
            {
                strAdapterName = strAdapterName.substr(0, x);
            }
        }
    }
    else
    {
        hr = GetSocketAddressForAdapter(family, strAdapterName.c_str(), port, pAddr);
    }

    return hr;
}



HRESULT BuildServerConfigurationFromArgs(StartupArgs& argsIn, CStunServerConfig* pConfigOut)
{
    HRESULT hr = S_OK;

    // default values;
    int family = AF_INET;
    // bool fIsUdp = true;
    int nPrimaryPort = DEFAULT_STUN_PORT;
    int nAltPort = DEFAULT_STUN_PORT + 1;
    bool fHasAtLeastTwoAdapters = false;
    CStunServerConfig config;
    int nMaxConnections = 0;
    const char* pszPrimaryAdvertised = argsIn.strPrimaryAdvertised.c_str();
    const char* pszAltAdvertised = argsIn.strAlternateAdvertised.c_str();

    enum ServerMode
    {
        Basic,
        Full
    };
    ServerMode mode=Basic;

    StartupArgs args = argsIn;



    ChkIfA(pConfigOut == NULL, E_INVALIDARG);


    // normalize the args.  The "trim" is not needed for command line args, but will be useful when we have an "init file" for initializing the server
    StringHelper::ToLower(args.strMode);
    StringHelper::Trim(args.strMode);

    StringHelper::Trim(args.strPrimaryInterface);

    StringHelper::Trim(args.strAltInterface);

    StringHelper::Trim(args.strPrimaryPort);

    StringHelper::Trim(args.strAltPort);

    StringHelper::Trim(args.strFamily);

    StringHelper::ToLower(args.strProtocol);
    StringHelper::Trim(args.strProtocol);
    
    StringHelper::Trim(args.strPrimaryAdvertised);
    StringHelper::Trim(args.strAlternateAdvertised);



    // ---- MODE ----------------------------------------------------------
    if (args.strMode.length() > 0)
    {
        if (args.strMode == "basic")
        {
            mode = Basic;
        }
        else if (args.strMode == "full")
        {
            mode = Full;
        }
        else
        {
            Logging::LogMsg(LL_ALWAYS, "Mode must be \"full\" or \"basic\".");
            Chk(E_INVALIDARG);
        }
    }


    // ---- FAMILY --------------------------------------------------------
    family = AF_INET;
    if (args.strFamily.length() > 0)
    {
        if (args.strFamily == "4")
        {
            family = AF_INET;
        }
        else if (args.strFamily == "6")
        {
            family = AF_INET6;
        }
        else
        {
            Logging::LogMsg(LL_ALWAYS, "Family argument must be '4' or '6'");
            Chk(E_INVALIDARG);
        }
    }

    // ---- PROTOCOL --------------------------------------------------------
    if (args.strProtocol.length() > 0)
    {
        if ((args.strProtocol != "udp") && (args.strProtocol != "tcp"))
        {
            Logging::LogMsg(LL_ALWAYS, "Protocol argument must be 'udp' or 'tcp'. 'tls' is not supported yet");
            Chk(E_INVALIDARG);
        }
        
        config.fTCP = (args.strProtocol == "tcp");
    }
    
    
    // ---- MAX Connections -----------------------------------------------------
    nMaxConnections = 0;
    if (args.strMaxConnections.length() > 0)
    {
        if (config.fTCP == false)
        {
            Logging::LogMsg(LL_ALWAYS, "Max connections parameter has no meaning in UDP mode. Did you mean to specify \"--protocol=tcp ?\"");
        }
        else
        {
            hr = StringHelper::ValidateNumberString(args.strMaxConnections.c_str(), 1, 100000, &nMaxConnections);
            if (FAILED(hr))
            {
                Logging::LogMsg(LL_ALWAYS, "Max connections must be between 1-100000");
                Chk(hr);
            }
        }
        config.nMaxConnections = nMaxConnections;
    }


    // ---- PRIMARY PORT --------------------------------------------------------
    nPrimaryPort = DEFAULT_STUN_PORT;
    if (args.strPrimaryPort.length() > 0)
    {
        hr = StringHelper::ValidateNumberString(args.strPrimaryPort.c_str(), 0x0001, 0xffff, &nPrimaryPort);
        if (FAILED(hr))
        {
            Logging::LogMsg(LL_ALWAYS, "Primary port value is invalid.  Value must be between 1-65535");
            Chk(hr);
        }
    }

    // ---- ALT PORT --------------------------------------------------------
    nAltPort = DEFAULT_STUN_PORT + 1;
    if (args.strAltPort.length() > 0)
    {
        hr = StringHelper::ValidateNumberString(args.strAltPort.c_str(), 0x0001, 0xffff, &nAltPort);
        if (FAILED(hr))
        {
            Logging::LogMsg(LL_ALWAYS, "Alt port value is invalid.  Value must be between 1-65535");
            Chk(hr);
        }
    }

    if (nPrimaryPort == nAltPort)
    {
        Logging::LogMsg(LL_ALWAYS, "Primary port and alternate port must be different values");
        Chk(E_INVALIDARG);
    }


    // ---- Adapters and mode adjustment


    fHasAtLeastTwoAdapters = ::HasAtLeastTwoAdapters(family);

    if (mode == Basic)
    {
        uint16_t port = (uint16_t)((int16_t)nPrimaryPort);
        // in basic mode, if no adapter is specified, bind to all of them
        if (args.strPrimaryInterface.length() == 0)
        {
            if (family == AF_INET)
            {
                config.addrPP = CSocketAddress(0, port);
                config.fHasPP = true;
            }
            else if (family == AF_INET6)
            {
                sockaddr_in6 addr6 = {}; // zero-init
                addr6.sin6_family = AF_INET6;
                config.addrPP = CSocketAddress(addr6);
                config.addrPP.SetPort(port);
                config.fHasPP = true;
            }
        }
        else
        {
            CSocketAddress addr;
            hr = GetSocketAddressForAdapter(family, args.strPrimaryInterface.c_str(), port, &addr);
            if (FAILED(hr))
            {
                Logging::LogMsg(LL_ALWAYS, "No matching primary adapter found for %s", args.strPrimaryInterface.c_str());
                Chk(hr);
            }
            config.addrPP = addr;
            config.fHasPP = true;
        }
    }
    else  // Full mode
    {
        CSocketAddress addrPrimary;
        CSocketAddress addrAlternate;
        uint16_t portPrimary = (uint16_t)((int16_t)nPrimaryPort);
        uint16_t portAlternate = (uint16_t)((int16_t)nAltPort);

        // in full mode, we can't bind to all adapters
        // so if one isn't specified, it's best guess - just avoid duplicates
        if (fHasAtLeastTwoAdapters == false)
        {
            Logging::LogMsg(LL_ALWAYS, "There does not appear to be two or more unique IP addresses to run in full mode");
            Chk(E_UNEXPECTED);
        }

        hr = ResolveAdapterName(true, family, args.strPrimaryInterface, 0, &addrPrimary);
        if (FAILED(hr))
        {
            Logging::LogMsg(LL_ALWAYS, "Can't find address for primary interface");
            Chk(hr);
        }

        hr = ResolveAdapterName(false, family, args.strAltInterface, 0, &addrAlternate);
        if (FAILED(hr))
        {
            Logging::LogMsg(LL_ALWAYS, "Can't find address for alternate interface");
            Chk(hr);
        }

        if  (addrPrimary.IsSameIP(addrAlternate))
        {
            Logging::LogMsg(LL_ALWAYS, "Error - Primary interface and Alternate Interface appear to have the same IP address. Full mode requires two IP addresses that are unique");
            Chk(E_INVALIDARG);
        }
        

        config.addrPP = addrPrimary;
        config.addrPP.SetPort(portPrimary);
        config.fHasPP = true;

        config.addrPA = addrPrimary;
        config.addrPA.SetPort(portAlternate);
        config.fHasPA = true;

        config.addrAP = addrAlternate;
        config.addrAP.SetPort(portPrimary);
        config.fHasAP = true;

        config.addrAA = addrAlternate;
        config.addrAA.SetPort(portAlternate);
        config.fHasAA = true;

    }


    // ---- Address advertisement --------------------------------------------------------
    // handle the advertised address parameters and make sure they are valid IP address strings
    
    if (!StringHelper::IsNullOrEmpty(pszPrimaryAdvertised))
    {
        hr = ::NumericIPToAddress(family, pszPrimaryAdvertised, &config.addrPrimaryAdvertised);
        if (FAILED(hr))
        {
            Logging::LogMsg(LL_ALWAYS, "Error with --primaryadvertised. %s is not a valid IP address string", pszPrimaryAdvertised);
            Chk(hr);
        }
    }
    
    if (!StringHelper::IsNullOrEmpty(pszAltAdvertised))
    {
        if (mode != Full)
        {
            Logging::LogMsg(LL_ALWAYS, "Error. --altadvertised was specified, but --mode param was not set to FULL.");
            ChkIf(config.fHasAA, E_INVALIDARG);
        }
        
        hr = ::NumericIPToAddress(family, pszAltAdvertised, &config.addrAlternateAdvertised);
        if (FAILED(hr))
        {
            Logging::LogMsg(LL_ALWAYS, "Error with --altadvertised. %s is not a valid IP address string", pszAltAdvertised);
            Chk(hr);
        }
    }

    // ---- DDOS PROTECTION SWITCH -------------------------------------------
    config.fEnableDosProtection = (argsIn.strDosProtect.length() > 0);

    // ---- REUSE ADDRESS SWITCH -------------------------------------------
    config.fReuseAddr = (argsIn.strReuseAddr.length() > 0);

    *pConfigOut = config;
    hr = S_OK;

Cleanup:
    return hr;
}


HRESULT ParseCommandLineArgs(int argc, char** argv, int startindex, StartupArgs* pStartupArgs)
{
    CCmdLineParser cmdline;
    std::string strHelp;
    bool fError = false;

    cmdline.AddOption("mode", required_argument, &pStartupArgs->strMode);
    cmdline.AddOption("primaryinterface", required_argument, &pStartupArgs->strPrimaryInterface);
    cmdline.AddOption("altinterface", required_argument, &pStartupArgs->strAltInterface);
    cmdline.AddOption("primaryadvertised", required_argument, &pStartupArgs->strPrimaryAdvertised);
    cmdline.AddOption("altadvertised", required_argument, &pStartupArgs->strAlternateAdvertised);
    cmdline.AddOption("primaryport", required_argument, &pStartupArgs->strPrimaryPort);
    cmdline.AddOption("altport", required_argument, &pStartupArgs->strAltPort);
    cmdline.AddOption("family", required_argument, &pStartupArgs->strFamily);
    cmdline.AddOption("protocol", required_argument, &pStartupArgs->strProtocol);
    cmdline.AddOption("maxconn", required_argument, &pStartupArgs->strMaxConnections);
    cmdline.AddOption("help", no_argument, &pStartupArgs->strHelp);
    cmdline.AddOption("verbosity", required_argument, &pStartupArgs->strVerbosity);
    cmdline.AddOption("ddp", no_argument, &pStartupArgs->strDosProtect);
    cmdline.AddOption("configfile", required_argument, &pStartupArgs->strConfigFile);
    cmdline.AddOption("reuseaddr", no_argument, &pStartupArgs->strReuseAddr);

    cmdline.ParseCommandLine(argc, argv, startindex, &fError);

    return fError ? E_INVALIDARG : S_OK;
}


HRESULT LoadConfigsFromFile(const std::string& filename, std::vector<StartupArgs>& configurations)
{
    ptree tree;
    bool error = false;
    std::string errorMessage;
    
    /* EXAMPLE configuration file
     {
      "configurations": [
        {
          "description" : "UDP Full Mode listening on default ports",
          "mode": "full",
          "family": "4",
          "protocol": "udp",
          "ddp": "1"
        },
        {
          "description" : "TCP Full Mode listening on default port",
          "mode": "full",
          "family": "6",
          "protocol": "tcp",
          "ddp": "1"
        }
      ]
    }
    */
    
    try
    {
        read_json(filename, tree);
        ptree root = tree.get_child("configurations");
        
        for (ptree::iterator itor = root.begin(); itor != root.end(); itor++)
        {
            const ptree& child = itor->second;
            StartupArgs args;

            args.strMode = child.get("mode", "");
            args.strPrimaryInterface = child.get("primaryinterface", "");
            args.strAltInterface = child.get("altinterface", "");
            args.strPrimaryAdvertised = child.get("primaryadvertised", "");
            args.strAlternateAdvertised = child.get("altadvertised", "");
            args.strPrimaryPort = child.get("primaryport", "");
            args.strAltPort = child.get("altport", "");
            args.strFamily = child.get("family", "");
            args.strProtocol = child.get("protocol", "");
            args.strMaxConnections = child.get("maxconn", "");
            args.strDosProtect = child.get("ddp", "");
            args.strReuseAddr = child.get("reuseaddr", "");
            
            configurations.push_back(args);
        }
    }
    catch(ptree_error& ex1)
    {
        Logging::LogMsg(LL_ALWAYS, "Error processing configuration file: %s", ex1.what());
        error = true;
    }
    
    if (!error && configurations.size() == 0)
    {
        Logging::LogMsg(LL_ALWAYS, "File is valid json, but no configurations found");
        error = true;
    }
    
    if (error)
    {
        configurations.clear();
        return E_FAIL;
    }
    
    return S_OK;
}
    
HRESULT BlockSignal(int sig)
{
    HRESULT hr = S_OK;
    int result = 0;
    sigset_t signalset;
    sigset_t oldset;
    
    sigemptyset(&signalset);
    sigemptyset(&oldset);
    
    sigaddset(&signalset, sig);
    
    // blocks the signal on *this* thread and all child threads spawned with pthread_create
    result = pthread_sigmask(SIG_BLOCK, &signalset, &oldset);
    
    if (result != 0)
    {
        hr = ERRNO_TO_HRESULT(result);
        Logging::LogMsg(LL_DEBUG, "BlockSignal: x%x", hr);
    }
    
    return hr;
}


// after all the child threads have initialized with a signal mask that blocks SIGINT and SIGTERM
// then the main thread UI can just sit on WaitForAppExitSignal and wait for CTRL-C to get pressed
void WaitForAppExitSignal()
{
    while (true)
    {
        sigset_t sigs;
        sigemptyset(&sigs);
        sigaddset(&sigs, SIGINT);
        sigaddset(&sigs, SIGTERM);
        int sig = 0;
        
        int ret = sigwait(&sigs, &sig);
        Logging::LogMsg(LL_DEBUG, "sigwait returns %d (errno==%d)", ret, (ret==-1)?0:errno);
        if ((sig == SIGINT) || (sig == SIGTERM))
        {
            break;
        }
    }
}



HRESULT StartUDP(CRefCountedPtr<CStunServer>& spServer, CStunServerConfig& config)
{
    HRESULT hr;
    
    hr = CStunServer::CreateInstance(config, spServer.GetPointerPointer());
    if (FAILED(hr))
    {
        Logging::LogMsg(LL_ALWAYS, "Unable to initialize UDP server (error code = x%x)", hr);
        LogHR(LL_ALWAYS, hr);
        return hr;
    }

    hr = spServer->Start();
    if (FAILED(hr))
    {
        Logging::LogMsg(LL_ALWAYS, "Unable to start UDP server (error code = x%x)", hr);
        LogHR(LL_ALWAYS, hr);
        return hr;
    }
    
    return S_OK;
}

HRESULT StartTCP(CRefCountedPtr<CTCPServer>& spTCPServer, CStunServerConfig& config)
{
    HRESULT hr;
    
    hr = CTCPServer::CreateInstance(config, spTCPServer.GetPointerPointer());
    if (FAILED(hr))
    {
        Logging::LogMsg(LL_ALWAYS, "Unable to initialize TCP server (error code = x%x)", hr);
        LogHR(LL_ALWAYS, hr);
        return hr;
    }
    
    hr = spTCPServer->Start();
    if (FAILED(hr))
    {
        Logging::LogMsg(LL_ALWAYS, "Unable to start TCP server (error code = x%x)", hr);
        LogHR(LL_ALWAYS, hr);
        return hr;
    }
    
    return S_OK;
    
}

int main(int argc, char** argv)
{
    HRESULT hr = S_OK;
    StartupArgs args;
    std::vector<StartupArgs> argsVector;
    int serverindex = 1;
    
    typedef CRefCountedPtr<CStunServer> UdpServerPtr;
    typedef CRefCountedPtr<CTCPServer> TcpServerPtr;
    
    std::vector<UdpServerPtr> udpServers;
    std::vector<TcpServerPtr> tcpServers;
    
     // block sigpipe so that socket send calls from raising SIGPIPE
    signal(SIGPIPE, SIG_IGN);
    BlockSignal(SIGPIPE);
    
    // Block SIGTERM and SIGINT such that the child threads will never get that signal (so that subsequent WaitForAppExitSignal hooks on *this* thread)
    BlockSignal(SIGTERM);
    BlockSignal(SIGINT);
    

#ifdef DEBUG
    Logging::SetLogLevel(LL_DEBUG);
#else
    Logging::SetLogLevel(LL_ALWAYS);
#endif


    hr = ParseCommandLineArgs(argc, argv, 1, &args);
    if (FAILED(hr))
    {
        PrintUsage(true);
        return -1;
    }

    if (args.strHelp.length() > 0)
    {
        PrintUsage(false);
        return -2;
    }

    if (args.strVerbosity.length() > 0)
    {
        int loglevel = (uint32_t)atoi(args.strVerbosity.c_str());

        if (loglevel >= 0)
        {
            Logging::SetLogLevel((uint32_t)loglevel);
        }
    }
    
    if (args.strConfigFile.empty() == false)
    {
        hr = LoadConfigsFromFile(args.strConfigFile, argsVector);
        if (FAILED(hr))
        {
            Logging::LogMsg(LL_ALWAYS, "Can't process configuration file");
            return -3;
        }
    }
    else
    {
        argsVector.push_back(args);
    }

    if (SUCCEEDED(hr))
    {
        for (std::vector<StartupArgs>::iterator itor = argsVector.begin(); itor != argsVector.end(); itor++)
        {
            CStunServerConfig config;
            StartupArgs args = *itor;
            
            Logging::LogMsg(LL_DEBUG, "Starting server %d", serverindex);
            serverindex++;
            
            ::DumpStartupArgs(args);
            hr = BuildServerConfigurationFromArgs(args, &config);
            if (FAILED(hr))
            {
                Logging::LogMsg(LL_ALWAYS, "Error building configuration from options given");
                break;
            }
            DumpConfig(config);
            
            if (config.fTCP)
            {
                TcpServerPtr spTcpServer;
                hr = StartTCP(spTcpServer, config);
                
                if (SUCCEEDED(hr))
                {
                    tcpServers.push_back(spTcpServer);
                }
            }
            else
            {
                UdpServerPtr spUdpServer;
                hr = StartUDP(spUdpServer, config);
                if (SUCCEEDED(hr))
                {
                    udpServers.push_back(spUdpServer);
                }
            }
            
            if (FAILED(hr))
            {
                break;
            }
        }
    }
    

    if (SUCCEEDED(hr))
    {
        Logging::LogMsg(LL_DEBUG, "Successfully started server.");
        WaitForAppExitSignal();
    }


    Logging::LogMsg(LL_DEBUG, "Server is exiting");
    
    
    for (std::vector<UdpServerPtr>::iterator itor = udpServers.begin(); itor != udpServers.end(); itor++)
    {
        Logging::LogMsg(LL_DEBUG, "Shutting down UDP server");
        UdpServerPtr server = *itor;
        server->Stop();
    }
    
    for (std::vector<TcpServerPtr>::iterator itor = tcpServers.begin(); itor != tcpServers.end(); itor++)
    {
        Logging::LogMsg(LL_DEBUG, "Shutting down TCP server");
        TcpServerPtr server = *itor;
        server->Stop();
    }
    
    return 0;
}

