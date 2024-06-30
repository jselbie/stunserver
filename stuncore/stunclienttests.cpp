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



CStunClientTestBase::CStunClientTestBase() :
_fInit(false),
_pConfig(NULL),
_pResults(NULL),
_fCompleted(false),
_transid() // zero-init
{
    ;
}

HRESULT CStunClientTestBase::Init(StunClientLogicConfig* pConfig, StunClientResults* pResults)
{
    HRESULT hr = S_OK;

    ChkIfA(pConfig == NULL, E_INVALIDARG);
    ChkIfA(pResults == NULL, E_INVALIDARG);

    _fInit = true;
    _pConfig = pConfig;
    _pResults = pResults;
    _fCompleted = false;
    memset(&_transid, 0, sizeof(_transid));

Cleanup:
    return hr;
}

HRESULT CStunClientTestBase::StartBindingRequest(CStunMessageBuilder& builder)
{
    builder.AddBindingRequestHeader();

    if (IsTransactionIdValid(_transid))
    {
        builder.AddTransactionId(_transid);
    }
    else
    {
        builder.AddRandomTransactionId(&_transid);
    }
    return S_OK;
}

HRESULT CStunClientTestBase::BasicReaderValidation(CRefCountedBuffer& spMsg, CStunMessageReader& reader)
{
    HRESULT hr = S_OK;
    CStunMessageReader::ReaderParseState readerstate;
    StunTransactionId transid;
    int cmp = 0;

    readerstate = reader.AddBytes(spMsg->GetData(), spMsg->GetSize());
    
    hr = (readerstate == CStunMessageReader::BodyValidated) ? S_OK : E_FAIL;
    if (FAILED(hr))
    {
        Logging::LogMsg(LL_DEBUG, "BasicReaderValidation - body parsing failed");
    }
    else
    {
        reader.GetTransactionId(&transid);
        cmp = memcmp(transid.id, _transid.id, sizeof(_transid));
        hr = (cmp == 0) ? S_OK : E_FAIL;
        if (FAILED(hr))
        {
            Logging::LogMsg(LL_DEBUG, "BasicReaderValidation - transaction id comparison failed");
        }
    }

    return hr;
}



bool CStunClientTestBase::IsCompleted()
{
    return _fCompleted;
}

void CStunClientTestBase::PreRunCheck()
{
    return;
}



// ----------------------------------------------------------------------------------




bool CBasicBindingTest::IsReadyToRun()
{
    // the binding test can always be run if it hasn't already been run
    return (_fCompleted == false);
}

HRESULT CBasicBindingTest::GetMessage(CRefCountedBuffer& spMsg, CSocketAddress* pAddrDest)
{
    StunChangeRequestAttribute attribChangeRequest = {};
        
    HRESULT hr = S_OK;
    ASSERT(spMsg->GetAllocatedSize() > 0);
    ASSERT(pAddrDest);
    ASSERT(_fInit);

    CStunMessageBuilder builder;
    builder.GetStream().Attach(spMsg, true);

    Chk(StartBindingRequest(builder));
    
    builder.AddChangeRequest(attribChangeRequest); // adding a blank CHANGE-REQUEST, because a JSTUN server will not respond if the request doesn't have one
    
    builder.FixLengthField();

    *pAddrDest = _pConfig->addrServer;


Cleanup:
    return hr;
}

HRESULT CBasicBindingTest::ProcessResponse(CRefCountedBuffer& spMsg, CSocketAddress& addrRemote, CSocketAddress& addrLocal)
{
    HRESULT hr = S_OK;
    CStunMessageReader reader;
    CSocketAddress addrMapped;
    CSocketAddress addrOther;
    bool fHasOtherAddress = false;

    Chk(BasicReaderValidation(spMsg, reader));

    hr = reader.GetXorMappedAddress(&addrMapped);
    if (FAILED(hr))
    {
        hr = reader.GetMappedAddress(&addrMapped);
    }
    Chk(hr); // again drop the message if we can't parse the binding response

    fHasOtherAddress = SUCCEEDED(reader.GetOtherAddress(&addrOther));

    // ok, we got a response.  So we are done
    _fCompleted = true;
    _pResults->fBindingTestSuccess = true;
    _pResults->fIsDirect = addrLocal.IsSameIP_and_Port(addrMapped);
    _pResults->addrLocal = addrLocal;
    _pResults->addrMapped = addrMapped;
    _pResults->fHasOtherAddress = fHasOtherAddress;


    if (fHasOtherAddress)
    {
        _pResults->addrAA = addrOther;

        _pResults->addrPA = _pConfig->addrServer;
        _pResults->addrPA.SetPort(addrOther.GetPort());

        _pResults->addrAP = addrOther;
        _pResults->addrAP.SetPort(_pConfig->addrServer.GetPort());

        if (Logging::GetLogLevel() >= LL_DEBUG)
        {
            char sz[100];
            addrOther.ToStringBuffer(sz, 100);
            Logging::LogMsg(LL_DEBUG, "Other address is %s\n",sz);
        }

    }

Cleanup:
    return hr;
}

void CBasicBindingTest::NotifyTimeout()
{
    // we timed out - mark the request as failed and get out of here
    _fCompleted = true;
    _pResults->fBindingTestSuccess = false; // should already be false
}

// ----------------------------------------------------------------------------------



CBehaviorTest::CBehaviorTest()
{
    this->_fIsTest3 = false;
}

void CBehaviorTest::PreRunCheck()
{

    if (_fIsTest3 == false)
    {
        // we don't need to run BehaviorTest2 or BehaviorTest3 if we know we are direct
        if (_pResults->fBindingTestSuccess && _pResults->fIsDirect)
        {
            _fCompleted = true;
            _pResults->behavior = ::DirectMapping;
            _pResults->fBehaviorTestSuccess = true;
        }
    }
}

bool CBehaviorTest::IsReadyToRun()
{
    // we can run if the CBasicBindingTest succeeded and we have an "other" address
    bool fRet = ((_fCompleted==false) && _pResults->fBindingTestSuccess && _pResults->fHasOtherAddress && (_pResults->fBehaviorTestSuccess==false));

    if (_fIsTest3)
    {
        // check to see that test2 succeeded before allowing test3 to run
        fRet = (fRet && (_pResults->addrMappingAP.IsIPAddressZero() == false));
    }

    return fRet;

}

HRESULT CBehaviorTest::GetMessage(CRefCountedBuffer& spMsg, CSocketAddress* pAddrDest)
{
    HRESULT hr = S_OK;
    ASSERT(spMsg->GetAllocatedSize() > 0);
    ASSERT(pAddrDest);
    
    StunChangeRequestAttribute attribChangeRequest = {};

    CStunMessageBuilder builder;
    builder.GetStream().Attach(spMsg, true);
    StartBindingRequest(builder);
    
    builder.AddChangeRequest(attribChangeRequest); // adding a blank CHANGE-REQUEST, because a JSTUN server will not respond if the request doesn't have one
    
    builder.FixLengthField();

    if (_fIsTest3 == false)
    {
        Logging::LogMsg(LL_DEBUG, "Preparing message for behavior test #2 (destination=AP)");
        *pAddrDest = _pResults->addrAP;
    }
    else
    {
        Logging::LogMsg(LL_DEBUG, "Preparing message for behavior test #2 (destination=AA)");
        *pAddrDest = _pResults->addrAA;
    }

    return hr;
}

HRESULT CBehaviorTest::ProcessResponse(CRefCountedBuffer& spMsg, CSocketAddress& addrRemote, CSocketAddress& addrLocal)
{
    HRESULT hr = S_OK;
    CStunMessageReader reader;
    CSocketAddress addrMapped;


    Chk(BasicReaderValidation(spMsg, reader));

    hr = reader.GetXorMappedAddress(&addrMapped);
    if (FAILED(hr))
    {
        hr = reader.GetMappedAddress(&addrMapped);
    }
    Chk(hr); // again drop the message if we can't parse the binding response

    _fCompleted = true;


    if (_fIsTest3)
    {
        _pResults->addrMappingAA = addrMapped;
        _pResults->fBehaviorTestSuccess = true;

        if (addrMapped.IsSameIP_and_Port(_pResults->addrMappingAP))
        {
            _pResults->behavior = ::AddressDependentMapping;
        }
        else
        {
            _pResults->behavior = ::AddressAndPortDependentMapping;
        }
    }
    else
    {
        _pResults->addrMappingAP = addrMapped;
        if (addrMapped.IsSameIP_and_Port(_pResults->addrMapped))
        {
            _pResults->fBehaviorTestSuccess = true;
            _pResults->behavior = ::EndpointIndependentMapping;
        }
    }

Cleanup:
    return hr;
}

void CBehaviorTest::NotifyTimeout()
{
    // the behavior test fails if it never got a response
    _fCompleted = true;
    _pResults->fBehaviorTestSuccess = false;
}

void CBehaviorTest::RunAsTest3(bool fSetAsTest3)
{
    _fIsTest3 = fSetAsTest3;
}



// ----------------------------------------------------------------------------------


CFilteringTest::CFilteringTest()
{
    _fIsTest3 = false;
}


bool CFilteringTest::IsReadyToRun()
{
    // we can run if the CBasicBindingTest succeeded and we have an "other" address
    bool fRet = ((_fCompleted==false) && _pResults->fBindingTestSuccess && _pResults->fHasOtherAddress && (_pResults->fFilteringTestSuccess==false) && (_pResults->fGotTest2Response==false));

    return fRet;
}

HRESULT CFilteringTest::GetMessage(CRefCountedBuffer& spMsg, CSocketAddress* pAddrDest)
{
    CStunMessageBuilder builder;
    StunChangeRequestAttribute change;


    builder.GetStream().Attach(spMsg, true);

    StartBindingRequest(builder);

    *pAddrDest = _pConfig->addrServer;

    if (_fIsTest3 == false)
    {
        Logging::LogMsg(LL_DEBUG, "Preparing message for filtering test #2 (ChangeRequest=AA)");
        change.fChangeIP = true;
        change.fChangePort = true;
        builder.AddChangeRequest(change);
    }
    else
    {
        Logging::LogMsg(LL_DEBUG, "Preparing message for filtering test #3 (ChangeRequest=PA)");
        change.fChangeIP = false;
        change.fChangePort = true;
        builder.AddChangeRequest(change);
    }

    builder.FixLengthField();


    return S_OK;
}


HRESULT CFilteringTest::ProcessResponse(CRefCountedBuffer& spMsg, CSocketAddress& addrRemote, CSocketAddress& addrLocal)
{
    HRESULT hr = S_OK;
    CStunMessageReader reader;


    Chk(BasicReaderValidation(spMsg, reader));

    // BasicReaderValidation will check the transaction ID!
    // we don't really care what's in the response other than if the transactionID is correct

    _fCompleted = true;

    if (_fIsTest3)
    {
        _pResults->fGotTest3Response = true;
        _pResults->fFilteringTestSuccess = true;
        _pResults->filtering = ::AddressDependentFiltering;
    }
    else
    {
        // if we got a response back from the other IP and Port, then we have independent filtering
        _pResults->fGotTest2Response = true;
        _pResults->fFilteringTestSuccess = true;
        _pResults->filtering = ::EndpointIndependentFiltering;
    }


Cleanup:
    return hr;
}

void CFilteringTest::NotifyTimeout()
{
    // in the filtering test, it's expected to not get a response for test2 or test3
    _fCompleted = true;

    // if we didn't get a response in test3, that implies we never got a response in test2 (because we don't run test3 if test2 got a response)
    if (_fIsTest3)
    {
        _pResults->fFilteringTestSuccess = true;
        _pResults->filtering = ::AddressAndPortDependentFiltering;
    }

}

void CFilteringTest::RunAsTest3(bool fSetAsTest3)
{
    _fIsTest3 = fSetAsTest3;
}

