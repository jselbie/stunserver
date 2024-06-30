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


#ifndef STUNCLIENTLOGIC_H
#define	STUNCLIENTLOGIC_H

#include "stunclienttests.h"
#include "stuntypes.h"

struct StunClientLogicConfig
{
    CSocketAddress addrServer;
    
    bool fTimeoutIsInstant;  // if true, then timeoutSeconds is ignored and assumed to be zero
    uint32_t timeoutSeconds; // if fTimeoutIsInstant is false, then "0" implies to use the default
    uint32_t uMaxAttempts;
    
    bool fBehaviorTest;
    bool fFilteringTest;
};


struct StunClientResults
{
    // basic binding test will set these results
    bool fBindingTestSuccess;
    bool fIsDirect;            // true if addrLocal == addrMapped
    CSocketAddress addrLocal;  // local address
    CSocketAddress addrMapped; // mapped address from PP for local address
    bool fHasOtherAddress;  // set to true if the basic binding request got an "other adddress" back from the server
    CSocketAddress addrPA;  // the other address (with primary IP) as identified by the basic binding request
    CSocketAddress addrAP;  // the other address (with primary port) as identified by the basic binding request
    CSocketAddress addrAA;  // the other address as identified by the basic binding request
    // -----------------------------------------

    // behavior state test --------------------------
    bool fBehaviorTestSuccess;
    NatBehavior behavior;
    CSocketAddress addrMappingAP; // result of binding request for AP (behavior test 2)
    CSocketAddress addrMappingAA; // result of binding request for AA (behavior test 3)
    // -----------------------------------------
    
    // filtering state test --------------------------
    bool fFilteringTestSuccess;
    NatFiltering filtering;
    bool fGotTest2Response;
    bool fGotTest3Response;
    // -----------------------------------------
    
    uint32_t errorBitmask;
    static const uint32_t SCR_TIMEOUT = 0x0001;     // a timeout occurred waiting for an expected response
    static const uint32_t SCR_NO_OTHER = 0x0002; // the server doesn't offer an alternate address/port
    
    StunClientResults();
    void Init();
};


#define FACILITY_STUN_CLIENT_LOGIC_ERR 0x101
#define E_STUNCLIENT_STILL_WAITING     ((HRESULT)0x81010001)
#define E_STUNCLIENT_RESULTS_READY     ((HRESULT)0x81010002)
#define E_STUNCLIENT_TIMEOUT           ((HRESULT)0x81010003)
#define E_STUNCLIENT_BUFFER_TOO_SMALL  ((HRESULT)0x81010004)




class CStunClientLogic
{
private:
    
    StunClientLogicConfig _config;
    StunClientResults _results;
    
    bool _fInitialized;
    
    uint32_t _timeLastMessageSent;
    uint32_t _sendCount;
    bool _fPreCheckRunOnTest;

    CBasicBindingTest _test1;
    CBehaviorTest _testBehavior2;
    CBehaviorTest _testBehavior3;
    CFilteringTest _testFiltering2;
    CFilteringTest _testFiltering3;
    
    std::vector<IStunClientTest*> _testlist;
    size_t _nTestIndex;
    
public:
    CStunClientLogic();
    
    HRESULT Initialize(StunClientLogicConfig& config);
    
    HRESULT GetNextMessage(CRefCountedBuffer& spMsg, CSocketAddress* pAddrDest, uint32_t timeCurrentMilliseconds);
    
    HRESULT ProcessResponse(CRefCountedBuffer& spMsg, CSocketAddress& addrRemote, CSocketAddress& addrLocal);
    
    HRESULT GetResults(StunClientResults* pResults);
};






#endif	/* STUNCLIENTLOGIC_H */

