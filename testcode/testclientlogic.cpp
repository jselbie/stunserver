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
#include "unittest.h"

#include "testmessagehandler.h"

#include "testclientlogic.h"
#include "stuntypes.h"


HRESULT CTestClientLogic::Run()
{
    HRESULT hr = S_OK;
    std::vector<NatBehavior> behaviorTests = {DirectMapping, EndpointIndependentMapping, AddressDependentMapping, AddressAndPortDependentMapping};
    std::vector<NatFiltering> filteringTests = {EndpointIndependentFiltering, AddressDependentFiltering, AddressAndPortDependentFiltering};

    
    ChkA(Test1());


    for (size_t i = 0; i < behaviorTests.size(); i++)
    {
        std::string testName = NatBehaviorToString(behaviorTests[i]);
        printf("Testing Behavior detection for %s\n", testName.c_str());
        ChkA(TestBehaviorAndFiltering(true, behaviorTests[i], false, AddressAndPortDependentFiltering));
    }

    for (size_t i = 0; i < filteringTests.size(); i++)
    {
        std::string testName = NatFilteringToString(filteringTests[i]);
        printf("Testing Filtering detection for %s\n", testName.c_str());
        ChkA(TestBehaviorAndFiltering(false, EndpointIndependentMapping, true, filteringTests[i]));
    }

    for (size_t b = 0; b < behaviorTests.size(); b++)
    {
        for (size_t f = 0; f < filteringTests.size(); f++)
        {
             std::string behaviorName = NatBehaviorToString(behaviorTests[b]);
             std::string filteringName = NatFilteringToString(filteringTests[f]);
             std::string testDescription = "Testing mix of [" + behaviorName + "] + [" + filteringName+"]";
             printf("%s\n", testDescription.c_str());
             ChkA(TestBehaviorAndFiltering(true, behaviorTests[b], true, filteringTests[f]));
        }
    }
    
Cleanup:
    return hr;
}

HRESULT CTestClientLogic::ValidateBindingRequest(CRefCountedBuffer& spMsg, StunTransactionId* pTransId)
{
    HRESULT hr = S_OK;
    CStunMessageReader reader;
    CStunMessageReader::ReaderParseState state;
    
    state = reader.AddBytes(spMsg->GetData(), spMsg->GetSize());
    ChkIfA(state != CStunMessageReader::BodyValidated, E_UNEXPECTED);
    
    ChkIfA(reader.GetMessageType() != StunMsgTypeBinding, E_UNEXPECTED);
    
    reader.GetTransactionId(pTransId);
    
    ChkIfA(false == IsTransactionIdValid(*pTransId), E_FAIL);
    
Cleanup:
    return hr;
}

HRESULT CTestClientLogic::GenerateBindingResponseMessage(const CSocketAddress& addrMapped, const StunTransactionId& transid, CRefCountedBuffer& spMsg)
{
    HRESULT hr = S_OK;
    
    CStunMessageBuilder builder;
    builder.GetStream().Attach(spMsg, true);
    
    ChkA(builder.AddBindingResponseHeader(true));
    ChkA(builder.AddTransactionId(transid));
    ChkA(builder.AddXorMappedAddress(addrMapped));
    ChkA(builder.FixLengthField());
Cleanup:    
    return hr;
}

HRESULT CTestClientLogic::GetMappedAddressForDestinationAddress(const CSocketAddress& addrDest, CSocketAddress* pAddrMapped)
{
    HRESULT hr = S_OK;
    if (addrDest.IsSameIP_and_Port(_addrServerPP))
    {
        *pAddrMapped = _addrMappedPP;
    }
    else if (addrDest.IsSameIP_and_Port(_addrServerPA))
    {
        *pAddrMapped = _addrMappedPA;
    }
    else if (addrDest.IsSameIP_and_Port(_addrServerAP))
    {
        *pAddrMapped = _addrMappedAP;
    }
    else if (addrDest.IsSameIP_and_Port(_addrServerAA))
    {
        *pAddrMapped = _addrMappedAA;
    }
    else
    {
        ASSERT(false);
        hr = E_FAIL;
    }
    
    return hr;
}

SocketRole CTestClientLogic::GetSocketRoleForDestinationAddress(const CSocketAddress& addrDest)
{
    if (addrDest.IsSameIP_and_Port(_addrServerPP))
    {
        return RolePP;
    }
    else if (addrDest.IsSameIP_and_Port(_addrServerPA))
    {
        return RolePA;
    }
    else if (addrDest.IsSameIP_and_Port(_addrServerAP))
    {
        return RoleAP;
    }
    else if (addrDest.IsSameIP_and_Port(_addrServerAA))
    {
        return RoleAA;
    }
    ASSERT(false);
    return RolePP;
    
}


HRESULT CTestClientLogic::CommonInit(NatBehavior behavior, NatFiltering filtering)
{
    HRESULT hr = S_OK;

    CSocketAddress addrMapped(0x22222222, 6000);
    
    _addrServerPP = CSocketAddress(0xaaaaaaaa, 1001);
    _addrServerPA = CSocketAddress(0xaaaaaaaa, 1002);
    _addrServerAP = CSocketAddress(0xbbbbbbbb, 1001);
    _addrServerAA = CSocketAddress(0xbbbbbbbb, 1002);
    
    _tsa.set[RolePP].fValid = true;
    _tsa.set[RolePP].addr = _addrServerPP;
    _tsa.set[RolePA].fValid = true;
    _tsa.set[RolePA].addr = _addrServerPA;
    _tsa.set[RoleAP].fValid = true;
    _tsa.set[RoleAP].addr = _addrServerAP;
    _tsa.set[RoleAA].fValid = true;
    _tsa.set[RoleAA].addr = _addrServerAA;
    
    
    _addrLocal = CSocketAddress(0x33333333, 7000);
    
    _addrMappedPP = addrMapped;
    _addrMappedPA = addrMapped;
    _addrMappedAP = addrMapped;
    _addrMappedAA = addrMapped;
    
    _spClientLogic = boost::shared_ptr<CStunClientLogic>(new CStunClientLogic());
    
    
    switch (behavior)
    {
        case DirectMapping:
        {
            _addrMappedPP = _addrLocal;
            _addrMappedPA = _addrLocal;
            _addrMappedAP = _addrLocal;
            _addrMappedAA = _addrLocal;
            break;
        }
        
        case EndpointIndependentMapping:
        {
            break;
        }
        
        case AddressDependentMapping:
        {
            _addrMappedAP.SetPort(6002);
            _addrMappedAA.SetPort(6002);
            break;
        }
        
        case AddressAndPortDependentMapping:
        {
            _addrMappedPA.SetPort(6001);
            _addrMappedAP.SetPort(6002);
            _addrMappedAA.SetPort(6003);
            break;
        }
        
        default:
        {
            ChkA(E_FAIL);
        }
    }
    
    switch (filtering)
    {
        case EndpointIndependentFiltering:
        case DirectConnectionFiltering:
        {
            _fAllowChangeRequestPA  = true;
            _fAllowChangeRequestAA = true;
            break;
        }
        
        case AddressDependentFiltering:
        {
            _fAllowChangeRequestPA  = true;
            _fAllowChangeRequestAA = false;
            break;
        }
        
        case AddressAndPortDependentFiltering:
        {
            _fAllowChangeRequestPA  = false;
            _fAllowChangeRequestAA = false;
            break;
        }
        default:
        {
            ChkA(E_FAIL);
        }
    }

Cleanup:
    return hr;    
}


HRESULT CTestClientLogic::TestBehaviorAndFiltering(bool fBehaviorTest, NatBehavior behavior, bool fFilteringTest, NatFiltering filtering)
{
    HRESULT hr = S_OK;
    StunClientLogicConfig config;
    HRESULT hrRet;
    uint32_t time = 0;
    CRefCountedBuffer spMsgOut(new CBuffer(MAX_STUN_MESSAGE_SIZE));
    CRefCountedBuffer spMsgResponse(new CBuffer(MAX_STUN_MESSAGE_SIZE));
    SocketRole outputRole;
    CSocketAddress addrDummy;
    
    StunMessageIn stunmsgIn;
    StunMessageOut stunmsgOut;
    
    
    CSocketAddress addrDest;
    CSocketAddress addrMapped;
    CSocketAddress addrServerResponse; // what address the fake server responded back on
    StunClientResults results;
    StunTransactionId transid={};
    //std::string strAddr;

    
    ChkA(CommonInit(behavior, filtering));
    

    config.addrServer = _addrServerPP;
    config.fBehaviorTest = fBehaviorTest;
    config.fFilteringTest = fFilteringTest;
    config.timeoutSeconds = 5;
    config.uMaxAttempts = 10;
    
    ChkA(_spClientLogic->Initialize(config));
    
    while (true)
    {
        CStunMessageReader reader;
        
        bool fDropMessage = false;
        
        time += 1000;
        
        hrRet = _spClientLogic->GetNextMessage(spMsgOut, &addrDest, time);
        if (hrRet == E_STUNCLIENT_STILL_WAITING)
        {
            //printf("GetNextMessage returned 'still waiting'\n");
            continue;
        }
        
        if (hrRet == E_STUNCLIENT_RESULTS_READY)
        {
            //printf("GetNextMessage returned 'results ready'\n");
            break;
        }
        
        //addrDest.ToString(&strAddr);
        //printf("Client is sending stun packet to %s\n", strAddr.c_str());
        
        ChkA(GetMappedAddressForDestinationAddress(addrDest, &addrMapped));
        
        //addrMapped.ToString(&strAddr);
        //printf("Server is receiving stun packet from %s\n", strAddr.c_str());
        
        ChkA(ValidateBindingRequest(spMsgOut, &transid));
        
        // --------------------------------------------------
        
        reader.AddBytes(spMsgOut->GetData(), spMsgOut->GetSize());
        
        ChkIfA(reader.GetState() != CStunMessageReader::BodyValidated, E_UNEXPECTED);
        
        // Simulate sending the binding request and getting a response back
        stunmsgIn.socketrole = GetSocketRoleForDestinationAddress(addrDest);
        stunmsgIn.addrLocal = addrDest;
        stunmsgIn.addrRemote = addrMapped;
        stunmsgIn.fConnectionOriented = false;
        stunmsgIn.pReader = &reader;
        
        stunmsgOut.socketrole = (SocketRole)-1; // intentionally setting it wrong
        stunmsgOut.addrDest = addrDummy;  // we don't care what address the server sent back to
        stunmsgOut.spBufferOut = spMsgResponse;
        spMsgResponse->SetSize(0);
        
        ChkA(::CStunRequestHandler::ProcessRequest(stunmsgIn, stunmsgOut, &_tsa, NULL));
        
        // simulate the message coming back
        
        // make sure we got something!
        outputRole = stunmsgOut.socketrole;
        ChkIfA(::IsValidSocketRole(outputRole)==false, E_FAIL);
        
        ChkIfA(spMsgResponse->GetSize() == 0, E_FAIL);
        
        addrServerResponse = _tsa.set[stunmsgOut.socketrole].addr;
        
        // --------------------------------------------------

        
        //addrServerResponse.ToString(&strAddr);
        //printf("Server is sending back from %s\n", strAddr.c_str());
        
        // if the request went to PP, but came back from AA or AP, then it's likely a filtering test
        // decide if we need to drop the response
        
        fDropMessage = ( addrDest.IsSameIP_and_Port(_addrServerPP) &&
                         ( ((outputRole == RoleAA) && (_fAllowChangeRequestAA==false)) ||
                           ((outputRole == RolePA) && (_fAllowChangeRequestPA==false)) 
                         )
                       );
        
        
        //{
        //    CStunMessageReader::ReaderParseState state;
        //    CStunMessageReader readerDebug;
        //    state = readerDebug.AddBytes(spMsgResponse->GetData(), spMsgResponse->GetSize());
        //    if (state != CStunMessageReader::BodyValidated)
        //    {
        //        printf("Error - response from server doesn't look valid");
        //    }
        //    else
        //    {
        //        CSocketAddress addr;
        //        readerDebug.GetMappedAddress(&addr);
        //        addr.ToString(&strAddr);
        //        printf("Response from server indicates our mapped address is %s\n", strAddr.c_str());
        //    }
        //}
        
        if (fDropMessage == false)
        {
            ChkA(_spClientLogic->ProcessResponse(spMsgResponse, addrServerResponse, _addrLocal));
        }
    }
    
    // now validate the results
    results.Init(); // zero it out
    _spClientLogic->GetResults(&results);
    
    if (fBehaviorTest)
    {
        ChkIfA(results.behavior != behavior, E_UNEXPECTED);
    }
    if (fFilteringTest)
    {
        ChkIfA(results.filtering != filtering, E_UNEXPECTED);
    }
    
Cleanup:
            
    return hr;
    
}


HRESULT CTestClientLogic::Test1()
{
    HRESULT hr = S_OK;
    HRESULT hrTmp = 0;
    CStunClientLogic clientlogic;    
    ::StunClientLogicConfig config;
    CRefCountedBuffer spMsgOut(new CBuffer(MAX_STUN_MESSAGE_SIZE));
    CRefCountedBuffer spMsgIn(new CBuffer(MAX_STUN_MESSAGE_SIZE));
    StunClientResults results;
    StunTransactionId transid;
    
    CSocketAddress addrDest;
    
    CSocketAddress addrServerPP = CSocketAddress(0xaaaaaaaa, 1001);
    CSocketAddress addrLocal =  CSocketAddress(0xdddddddd, 4444); 
    CSocketAddress addrMapped = CSocketAddress(0xeeeeeeee, 5555);
    
    config.addrServer = addrServerPP;
    config.fBehaviorTest = false;
    config.fFilteringTest = false;
    config.timeoutSeconds = 10;
    config.uMaxAttempts = 2;
    config.fTimeoutIsInstant = false;
    
    ChkA(clientlogic.Initialize(config));
    
    ChkA(clientlogic.GetNextMessage(spMsgOut, &addrDest, 0));
    
    // we expect to get back a message for the serverPP
    ChkIfA(addrDest.IsSameIP_and_Port(addrServerPP)==false, E_UNEXPECTED);
    
    // check to make sure out timeout logic appears to work
    hrTmp = clientlogic.GetNextMessage(spMsgOut, &addrDest, 1);
    ChkIfA(hrTmp != E_STUNCLIENT_STILL_WAITING, E_UNEXPECTED);

    // now we should get a dupe of what we had before
    ChkA(clientlogic.GetNextMessage(spMsgOut, &addrDest, 11000));
    
    // the message should be a binding request
    ChkA(ValidateBindingRequest(spMsgOut, &transid));
    
    // now let's generate a response 
    ChkA(GenerateBindingResponseMessage(addrMapped, transid, spMsgIn));
    ChkA(clientlogic.ProcessResponse(spMsgIn, addrServerPP, addrLocal));
    
    // results should be ready
    hrTmp = clientlogic.GetNextMessage(spMsgOut, &addrDest, 12000);
    ChkIfA(hrTmp != E_STUNCLIENT_RESULTS_READY, E_UNEXPECTED);
    
    ChkA(clientlogic.GetResults(&results));
    
    // results should have a successful binding result
    ChkIfA(results.fBindingTestSuccess==false, E_UNEXPECTED);
    ChkIfA(results.fIsDirect, E_UNEXPECTED);
    ChkIfA(results.addrMapped.IsSameIP_and_Port(addrMapped)==false, E_UNEXPECTED);
    ChkIfA(results.addrLocal.IsSameIP_and_Port(addrLocal)==false, E_UNEXPECTED);
    
Cleanup:
    return hr;
}

