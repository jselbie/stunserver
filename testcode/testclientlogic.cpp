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
#include "unittest.h"

#include "testmessagehandler.h"

#include "testclientlogic.h"


HRESULT CTestClientLogic::Run()
{
    HRESULT hr = S_OK;
    
    ChkA(Test1());
    
    printf("Testing detection for DirectMapping\n");
    ChkA(TestBehaviorAndFiltering(true, DirectMapping, false, EndpointIndependentFiltering));
    
    printf("Testing detection for EndpointIndependent mapping\n");
    ChkA(TestBehaviorAndFiltering(true, EndpointIndependentMapping, false, EndpointIndependentFiltering));
    
    printf("Testing detection for AddressDependentMapping\n");
    ChkA(TestBehaviorAndFiltering(true, AddressDependentMapping, false, EndpointIndependentFiltering));
    
    printf("Testing detection for AddressAndPortDependentMapping\n");
    ChkA(TestBehaviorAndFiltering(true, AddressAndPortDependentMapping, false, EndpointIndependentFiltering));
    
    printf("Testing detection for EndpointIndependentFiltering\n");
    ChkA(TestBehaviorAndFiltering(true, EndpointIndependentMapping, true, EndpointIndependentFiltering));

    printf("Testing detection for AddressDependentFiltering\n");
    ChkA(TestBehaviorAndFiltering(true, EndpointIndependentMapping, true, AddressDependentFiltering));
    
    printf("Testing detection for AddressAndPortDependentFiltering\n");
    ChkA(TestBehaviorAndFiltering(true, EndpointIndependentMapping, true, AddressAndPortDependentFiltering));
    
    
    
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
    
    _addrLocal = CSocketAddress(0x33333333, 7000);
    
    _addrMappedPP = addrMapped;
    _addrMappedPA = addrMapped;
    _addrMappedAP = addrMapped;
    _addrMappedAA = addrMapped;
    
    _spClientLogic = boost::shared_ptr<CStunClientLogic>(new CStunClientLogic());
    
    Chk(CMockTransport::CreateInstanceNoInit(_spTransport.GetPointerPointer()));
    
    _spHandler = boost::shared_ptr<CStunThreadMessageHandler>(new CStunThreadMessageHandler);
    _spHandler->SetResponder(_spTransport);
    
    _spTransport->AddPP(_addrServerPP);
    _spTransport->AddPA(_addrServerPA);
    _spTransport->AddAP(_addrServerAP);
    _spTransport->AddAA(_addrServerAA);
    
    
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
    CRefCountedBuffer spMsgOut(new CBuffer(1500));
    CRefCountedBuffer spMsgIn;
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
        bool fDropMessage = false;
        StunMessageEnvelope envelope;
        
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
        
        envelope.localAddr = addrDest;
        envelope.remoteAddr = addrMapped;
        envelope.spBuffer = spMsgOut;
        envelope.localSocket = GetSocketRoleForDestinationAddress(addrDest);

        
        _spTransport->ClearStream();
        _spHandler->ProcessRequest(envelope);
        
        // simulate the message coming back

        // make sure we got something!
        ChkIfA(_spTransport->m_outputRole == ((SocketRole)-1), E_FAIL);
        
        if (spMsgIn != NULL)
        {
            spMsgIn->SetSize(0);
        }
        
        _spTransport->m_outputstream.GetBuffer(&spMsgIn);
        ChkIf(spMsgIn->GetSize() == 0, E_FAIL);
        
        ChkA(_spTransport->GetSocketAddressForRole(_spTransport->m_outputRole, &addrServerResponse));
        
        //addrServerResponse.ToString(&strAddr);
        //printf("Server is sending back from %s\n", strAddr.c_str());
        
        // if the request went to PP, but came back from AA or AP, then it's likely a filtering test
        // decide if we need to drop the response
        
        fDropMessage = ( addrDest.IsSameIP_and_Port(_addrServerPP) &&
                         ( ((_spTransport->m_outputRole == RoleAA) && (_fAllowChangeRequestAA==false)) ||
                           ((_spTransport->m_outputRole == RolePA) && (_fAllowChangeRequestPA==false)) 
                         )
                       );
        
        if (fDropMessage == false)
        {
            ChkA(_spClientLogic->ProcessResponse(spMsgIn, addrServerResponse, _addrLocal));
        }
    }
    
    // now validate the results
    results.Init(); // zero it out
    _spClientLogic->GetResults(&results);
    
    ChkIfA(results.behavior != behavior, E_UNEXPECTED);
    
Cleanup:
    return hr;
    
}


HRESULT CTestClientLogic::Test1()
{
    HRESULT hr = S_OK;
    HRESULT hrTmp = 0;
    CStunClientLogic clientlogic;    
    ::StunClientLogicConfig config;
    CRefCountedBuffer spMsgOut(new CBuffer(1500));
    CRefCountedBuffer spMsgIn(new CBuffer(1500));
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

