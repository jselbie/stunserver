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
#include "stunclientlogic.h"


#include "stunclienttests.h"
#include "stunclientlogic.h"


StunClientResults::StunClientResults()
{
    Init();
}

void StunClientResults::Init()
{
    CSocketAddress addrZero;

    fBindingTestSuccess = false;
    fIsDirect = false;
    fHasOtherAddress = false;  // set to true if the basic binding request got an "other adddress" back from the server
    fBehaviorTestSuccess = false;
    behavior = UnknownBehavior;
    fFilteringTestSuccess = false;
    filtering = UnknownFiltering;
    fGotTest2Response = false;
    fGotTest3Response = false;

    addrLocal = addrZero;
    addrMapped = addrZero;
    addrPA = addrZero;
    addrAP = addrZero;
    addrAA = addrZero;
    addrMappingAP = addrZero;
    addrMappingAA = addrZero;

    errorBitmask = 0;
}

CStunClientLogic::CStunClientLogic() :
_fInitialized(false),
_timeLastMessageSent(0),
_sendCount(0),
_nTestIndex(0)
{

}


HRESULT CStunClientLogic::Initialize(StunClientLogicConfig& config)
{
    HRESULT hr = S_OK;

    // Don't ever try to "re-use" a CStunClientLogic instance after it's already been used.  Create a new instance after
    // putting a test cycle into motion.
    // Too much code in the tests expects everything in a clean state
    ChkIfA(_fInitialized, E_UNEXPECTED);


    ChkIfA(config.addrServer.IsIPAddressZero(), E_INVALIDARG);
    ChkIfA(config.addrServer.GetPort() == 0, E_INVALIDARG);


    _config = config;


    _fInitialized = true;
    
    if (_config.fTimeoutIsInstant)
    {
        _config.timeoutSeconds = 0;
    }
    else if (_config.timeoutSeconds == 0)
    {
        _config.timeoutSeconds = 3; // default to waiting for 3 seconds
    }
    

    if (_config.uMaxAttempts <= 0)
    {
        _config.uMaxAttempts = 2;
    }

    _nTestIndex = 0;

    _testlist.clear();

    // always add the binding test to start
    _test1.Init(&_config, &_results);
    _testlist.push_back(&_test1);


    if (_config.fFilteringTest)
    {
        _testFiltering2.Init(&_config, &_results);
        _testlist.push_back(&_testFiltering2);

        _testFiltering3.Init(&_config, &_results);
        _testFiltering3.RunAsTest3(true);
        _testlist.push_back(&_testFiltering3);
    }
    
    if (_config.fBehaviorTest)
    {
        _testBehavior2.Init(&_config, &_results);
        _testlist.push_back(&_testBehavior2);

        _testBehavior3.Init(&_config, &_results);
        _testBehavior3.RunAsTest3(true);
        _testlist.push_back(&_testBehavior3);
    }

    _fPreCheckRunOnTest = false;

    _timeLastMessageSent = 0;

Cleanup:
    return hr;
}

HRESULT CStunClientLogic::GetNextMessage(CRefCountedBuffer& spMsg, CSocketAddress* pAddrDest, uint32_t timeCurrentMilliseconds)
{
    HRESULT hr = S_OK;
    uint32_t diff = 0;
    IStunClientTest* pCurrentTest = NULL;
    bool fReadyToReturn = false;

    ChkIfA(_fInitialized == false, E_FAIL);
    ChkIfA(spMsg->GetAllocatedSize() == 0, E_INVALIDARG);
    ChkIfA(pAddrDest == NULL, E_INVALIDARG);

    // clients should pass in the expected size
    ChkIfA(spMsg->GetAllocatedSize() < MAX_STUN_MESSAGE_SIZE, E_INVALIDARG);


    while (fReadyToReturn==false)
    {
        if (_nTestIndex >= _testlist.size())
        {
            hr = E_STUNCLIENT_RESULTS_READY; // no more tests to run
            break;
        }

        pCurrentTest = _testlist[_nTestIndex];

        if (_fPreCheckRunOnTest==false)
        {
            // give the test an early chance to complete before sending a message (based on results of previous test)
            pCurrentTest->PreRunCheck();
            _fPreCheckRunOnTest = true;
        }


        // has this test completed or is it in a state in which it can't run?
        if (pCurrentTest->IsCompleted() || pCurrentTest->IsReadyToRun()==false)
        {
            // increment to the next test
            _nTestIndex++;
            _sendCount = 0;
            _fPreCheckRunOnTest = false;

            continue;
        }

        // Have we waited long enough for a response
        diff = (timeCurrentMilliseconds - _timeLastMessageSent) / 1000; // convert from milliseconds to seconds
        if ((diff < _config.timeoutSeconds) && (_sendCount != 0))
        {
            hr = E_STUNCLIENT_STILL_WAITING;
            break;
        }

        // have we exceeded the retry count
        if (_sendCount >= _config.uMaxAttempts)
        {
            // notify the test that it has timed out
            // this should put it in the completed state (and we increment to next test on next loop)
            pCurrentTest->NotifyTimeout();
            ASSERT(pCurrentTest->IsCompleted());
            continue;
        }

        // ok - we are ready to go fetch a message
        hr = pCurrentTest->GetMessage(spMsg, pAddrDest);
        ASSERT(SUCCEEDED(hr));
        if (FAILED(hr))
        {
            break;
        }

        // success
        _sendCount++;
        _timeLastMessageSent = timeCurrentMilliseconds;
        fReadyToReturn = true;
        hr = S_OK;
    }
Cleanup:
    return hr;
}

HRESULT CStunClientLogic::ProcessResponse(CRefCountedBuffer& spMsg, CSocketAddress& addrRemote, CSocketAddress& addrLocal)
{
    HRESULT hr = S_OK;
    IStunClientTest* pCurrentTest = NULL;

    ChkIfA(_fInitialized == false, E_FAIL);
    ChkIfA(spMsg->GetSize() == 0, E_INVALIDARG);
    ChkIf (_nTestIndex >= _testlist.size(), E_UNEXPECTED);

    pCurrentTest = _testlist[_nTestIndex];

    // passing a response to a test that is already completed ??
    ChkIfA(pCurrentTest->IsCompleted(), E_UNEXPECTED);

    hr = pCurrentTest->ProcessResponse(spMsg, addrRemote, addrLocal);
    // this likely puts the test into the completed state
    // A subsequent call to GetNextMessage will invoke the next test

Cleanup:
    return hr;
}

HRESULT CStunClientLogic::GetResults(StunClientResults* pResults)
{
    HRESULT hr=S_OK;
    ChkIfA(pResults == NULL, E_INVALIDARG);
    *pResults = _results;
Cleanup:
    return hr;
}

