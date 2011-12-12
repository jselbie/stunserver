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
#include "server.h"
#include "adapters.h"
#include "cmdlineparser.h"

#include <getopt.h>

#include "prettyprint.h"
#include "oshelper.h"
#include "stringhelper.h"


// unusual to include usage.cpp and usagelite.cpp here, but these are auto-generated resource file
#include "usage.txtcode"
#include "usagelite.txtcode"


void PrintUsage(bool fSummaryUsage)
{
    size_t width = GetConsoleWidth();
    const char* psz = fSummaryUsage ? usagelite_text : usage_text;

    // save some margin space
    if (width > 2)
    {
        width -= 2;
    }

    PrettyPrint(psz, width);
}



struct StartupArgs
{
    std::string strMode;
    std::string strPrimaryInterface;
    std::string strAltInterface;
    std::string strPrimaryPort;
    std::string strAltPort;
    std::string strFamily;
    std::string strProtocol;
    std::string strHelp;
    std::string strVerbosity;
};

#define PRINTARG(member) Logging::LogMsg(LL_DEBUG, "%s = %s", #member, args.member.length() ? args.member.c_str() : "<empty>");

void DumpStartupArgs(StartupArgs& args)
{
    Logging::LogMsg(LL_DEBUG, "\n\n--------------------------");
    PRINTARG(strMode);
    PRINTARG(strPrimaryInterface);
    PRINTARG(strAltInterface);
    PRINTARG(strPrimaryPort);
    PRINTARG(strAltPort);
    PRINTARG(strFamily);
    PRINTARG(strProtocol);
    PRINTARG(strHelp);
    PRINTARG(strVerbosity);
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

    enum ServerMode
    {
        Basic,
        Full
    };
    ServerMode mode=Basic;

    StartupArgs args = argsIn;



    ChkIfA(pConfigOut == NULL, E_INVALIDARG);


    // normalize the args.  The "trim" is not needed for command line args, but will be useful when we have an "init file" for intializing the server
    StringHelper::ToLower(args.strMode);
    StringHelper::Trim(args.strMode);

    StringHelper::Trim(args.strPrimaryInterface);

    StringHelper::Trim(args.strAltInterface);

    StringHelper::Trim(args.strPrimaryPort);

    StringHelper::Trim(args.strAltPort);

    StringHelper::Trim(args.strFamily);

    StringHelper::ToLower(args.strProtocol);
    StringHelper::Trim(args.strProtocol);



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
        if (args.strProtocol != "udp")
        {
            Logging::LogMsg(LL_ALWAYS, "Protocol argument must be 'udp' .  'tcp' and 'tls' are not supported yet");
            Chk(E_INVALIDARG);
        }
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
        Logging::LogMsg(LL_ALWAYS, "Primary port and altnernate port must be different values");
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
    cmdline.AddOption("primaryport", required_argument, &pStartupArgs->strPrimaryPort);
    cmdline.AddOption("altport", required_argument, &pStartupArgs->strAltPort);
    cmdline.AddOption("family", required_argument, &pStartupArgs->strFamily);
    cmdline.AddOption("protocol", required_argument, &pStartupArgs->strProtocol);
    cmdline.AddOption("help", no_argument, &pStartupArgs->strHelp);
    cmdline.AddOption("verbosity", required_argument, &pStartupArgs->strVerbosity);

    cmdline.ParseCommandLine(argc, argv, startindex, &fError);

    return fError ? E_INVALIDARG : S_OK;
}



// This routine will set SIGINT And SIGTERM to "blocked" status only so that the
// child worker threads will never have these events raised on them.
HRESULT InitAppExitListener()
{
    HRESULT hr = S_OK;
    sigset_t sigs;
    
    int ret;
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGINT);
    sigaddset(&sigs, SIGTERM);

    ret = pthread_sigmask(SIG_BLOCK, &sigs, NULL);
    hr = (ret == 0) ? S_OK : ERRNO_TO_HRESULT(ret);

    Logging::LogMsg(LL_DEBUG, "InitAppExitListener: x%x", hr);

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




int main(int argc, char** argv)
{
    HRESULT hr = S_OK;
    StartupArgs args;
    CStunServerConfig config;
    CRefCountedPtr<CStunServer> spServer;

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


    if (SUCCEEDED(hr))
    {
        ::DumpStartupArgs(args);
        hr = BuildServerConfigurationFromArgs(args, &config);
    }

    if (FAILED(hr))
    {
        Logging::LogMsg(LL_ALWAYS, "Error building configuration from command line options");
        PrintUsage(true);
        return -3;
    }


    DumpConfig(config);

    InitAppExitListener();

    hr = CStunServer::CreateInstance(config, spServer.GetPointerPointer());
    if (FAILED(hr))
    {
        Logging::LogMsg(LL_ALWAYS, "Unable to initialize server (error code = x%x)", hr);
        return -4;
    }

    hr = spServer->Start();
    if (FAILED(hr))
    {
        Logging::LogMsg(LL_ALWAYS, "Unable to start server (error code = x%x)", hr);
        return -5;
    }

    Logging::LogMsg(LL_DEBUG, "Successfully started server.");

    WaitForAppExitSignal();

    Logging::LogMsg(LL_DEBUG, "Server is exiting");

    spServer->Stop();
    spServer.ReleaseAndClear();

    return 0;
}

