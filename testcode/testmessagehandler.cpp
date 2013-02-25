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

#include "stunauth.h"
#include "testmessagehandler.h"


static const uint16_t c_portServerPrimary = 3478;
static const uint16_t c_portServerAlternate = 3479;
static const char* c_szIPServerPrimary = "1.2.3.4";
static const char* c_szIPServerAlternate = "1.2.3.5";
    
static const char* c_szIPLocal = "2.2.2.2";
static const uint16_t c_portLocal = 2222;
    
static const char* c_szIPMapped = "3.3.3.3";
static const uint16_t c_portMapped = 3333;



HRESULT CMockAuthShort::DoAuthCheck(AuthAttributes* pAuthAttributes, AuthResponse *pResponse)
{
    const char* c_pszAuthorizedUser = "AuthorizedUser";

    pResponse->authCredMech = AuthCredShortTerm;
    
    if (0 == strcmp(pAuthAttributes->szUser, c_pszAuthorizedUser))
    {
        strcpy(pResponse->szPassword, "password");
        pResponse->responseType = AllowConditional;
    }
    else if (pAuthAttributes->szUser[0] == '\0')
    {
        pResponse->responseType = Reject;        
    }
    else
    {
        pResponse->responseType = Unauthorized;
    }
    
    return S_OK;
}

HRESULT CMockAuthLong::DoAuthCheck(AuthAttributes* pAuthAttributes, AuthResponse* pResponse)
{
    // go ahead and return a realm/nonce.  It won't get used unless it's an error response
    bool fIsKnownUser = false;
    
    strcpy(pResponse->szRealm, "MyRealm");
    strcpy(pResponse->szNonce, "NewNonce");
    
    pResponse->authCredMech = AuthCredLongTerm;
    
    fIsKnownUser = !strcmp(pAuthAttributes->szUser, "AuthorizedUser");
    
    if  ((pAuthAttributes->fMessageIntegrityPresent == false) || (fIsKnownUser == false))
    {
        pResponse->responseType = Unauthorized;        
    }
    else
    {
        pResponse->responseType = AllowConditional;
        strcpy(pResponse->szPassword, "password");
    }
    
    return S_OK;
}


// ---------------------------------------------------------------------------------------------



CTestMessageHandler::CTestMessageHandler()
{
    CMockAuthShort::CreateInstanceNoInit(_spAuthShort.GetPointerPointer());
    CMockAuthLong::CreateInstanceNoInit(_spAuthLong.GetPointerPointer());

    ToAddr(c_szIPLocal, c_portLocal, &_addrLocal);
    ToAddr(c_szIPMapped, c_portMapped, &_addrMapped);
    
    ToAddr(c_szIPServerPrimary, c_portServerPrimary, &_addrServerPP);
    ToAddr(c_szIPServerPrimary, c_portServerAlternate, &_addrServerPA);
    ToAddr(c_szIPServerAlternate, c_portServerPrimary, &_addrServerAP);
    ToAddr(c_szIPServerAlternate, c_portServerAlternate, &_addrServerAA);
}


HRESULT CTestMessageHandler::SendHelper(CStunMessageBuilder& builderRequest, CStunMessageReader* pReaderResponse, IStunAuth* pAuth)
{
    CRefCountedBuffer spBufferRequest;
    CRefCountedBuffer spBufferResponse(new CBuffer(MAX_STUN_MESSAGE_SIZE));
    StunMessageIn msgIn;
    StunMessageOut msgOut;
    CStunMessageReader reader;
    CSocketAddress addrDest;
    TransportAddressSet tas;
    HRESULT hr = S_OK;
    
    
    InitTransportAddressSet(tas, true, true, true, true);
    
    builderRequest.GetResult(&spBufferRequest);
    
    ChkIf(CStunMessageReader::BodyValidated != reader.AddBytes(spBufferRequest->GetData(), spBufferRequest->GetSize()), E_FAIL);
    
    msgIn.fConnectionOriented = false;
    msgIn.addrLocal = _addrServerPP;
    msgIn.pReader = &reader;
    msgIn.socketrole = RolePP;
    msgIn.addrRemote = _addrMapped;
    
    msgOut.spBufferOut = spBufferResponse;
    
    ChkA(CStunRequestHandler::ProcessRequest(msgIn, msgOut, &tas, pAuth));
    
    ChkIf(CStunMessageReader::BodyValidated != pReaderResponse->AddBytes(spBufferResponse->GetData(), spBufferResponse->GetSize()), E_FAIL);
    
Cleanup:
    
    return hr;
    
}

void CTestMessageHandler::ToAddr(const char* pszIP, uint16_t port, CSocketAddress* pAddr)
{
    sockaddr_in addr={};
    int result;

    UNREFERENCED_VARIABLE(result);
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    result = ::inet_pton(AF_INET, pszIP, &addr.sin_addr);
    
    ASSERT(result == 1);
    
    *pAddr = addr;
}


HRESULT CTestMessageHandler::InitBindingRequest(CStunMessageBuilder& builder)
{
    StunTransactionId transid;
    builder.AddHeader(StunMsgTypeBinding, StunMsgClassRequest);
    builder.AddRandomTransactionId(&transid);
    builder.FixLengthField();
    return S_OK;
}

HRESULT CTestMessageHandler::ValidateMappedAddress(CStunMessageReader& reader, const CSocketAddress& addrExpected, bool fLegacyOnly)
{
    HRESULT hr = S_OK;
    CSocketAddress addrMapped;
    CSocketAddress addrXorMapped;
    HRESULT hrResult;

    hrResult = reader.GetXorMappedAddress(&addrXorMapped);
    
    if (SUCCEEDED(hrResult))
    {
        ChkIfA(false == addrExpected.IsSameIP_and_Port(addrXorMapped), E_FAIL);
        ChkIfA(fLegacyOnly, E_FAIL); // legacy responses should not include XOR mapped
    }
    else
    {
        ChkIfA(fLegacyOnly==false, E_FAIL); // non-legacy responses should include XOR Mapped address
    }
    
    ChkA(reader.GetMappedAddress(&addrMapped));
    ChkIfA(false == addrExpected.IsSameIP_and_Port(addrMapped), E_FAIL);
    
Cleanup:
    return hr;
}

HRESULT CTestMessageHandler::ValidateResponseOriginAddress(CStunMessageReader& reader, const CSocketAddress& addrExpected)
{
    HRESULT hr = S_OK;
    CSocketAddress addr;
    
    ChkA(reader.GetResponseOriginAddress(&addr));
    ChkIfA(false == addrExpected.IsSameIP_and_Port(addr), E_FAIL);
    
Cleanup:
    return hr;
}



HRESULT CTestMessageHandler::ValidateOtherAddress(CStunMessageReader& reader, const CSocketAddress& addrExpected)
{
    HRESULT hr = S_OK;
    CSocketAddress addr;
    
    ChkA(reader.GetOtherAddress(&addr));
    ChkIfA(false == addrExpected.IsSameIP_and_Port(addr), E_FAIL);
    
Cleanup:
    return hr;
}

void CTestMessageHandler::InitTransportAddressSet(TransportAddressSet& tas, bool fRolePP, bool fRolePA, bool fRoleAP, bool fRoleAA)
{
    CSocketAddress addrZero;
    
    tas.set[RolePP].fValid = fRolePP;
    tas.set[RolePP].addr = fRolePP ? _addrServerPP : addrZero;
    
    tas.set[RolePA].fValid = fRolePA;
    tas.set[RolePA].addr = fRolePA ? _addrServerPA : addrZero;
    
    tas.set[RoleAP].fValid = fRoleAP;
    tas.set[RoleAP].addr = fRoleAP ? _addrServerAP : addrZero;
    
    tas.set[RoleAA].fValid = fRoleAA;
    tas.set[RoleAA].addr = fRoleAA ? _addrServerAA : addrZero;
    
}



// Test1 - just do a basic binding request
HRESULT CTestMessageHandler::Test1()
{
    HRESULT hr = S_OK;
    CStunMessageBuilder builder;
    CRefCountedBuffer spBuffer, spBufferOut(new CBuffer(MAX_STUN_MESSAGE_SIZE));
    CStunMessageReader reader;
    StunMessageIn msgIn;
    StunMessageOut msgOut;
    TransportAddressSet tas = {};
    
    InitTransportAddressSet(tas, true, true, true, true);
    
    ChkA(InitBindingRequest(builder));
    
  
    Chk(builder.GetResult(&spBuffer));
    
    ChkIfA(CStunMessageReader::BodyValidated != reader.AddBytes(spBuffer->GetData(), spBuffer->GetSize()), E_FAIL);

    // a message send to the PP socket on the server from the 
    msgIn.socketrole = RolePP;
    msgIn.addrRemote = _addrMapped;
    msgIn.pReader = &reader;
    msgIn.addrLocal = _addrServerPP;
    msgIn.fConnectionOriented = false;
    
    spBuffer.reset();
    
    msgOut.spBufferOut = spBufferOut;
    msgOut.socketrole = RoleAA; // deliberately wrong - so we can validate if it got changed to RolePP
    
    ChkA(CStunRequestHandler::ProcessRequest(msgIn, msgOut, &tas, NULL));
    
    reader.Reset();
    ChkIfA(CStunMessageReader::BodyValidated != reader.AddBytes(spBufferOut->GetData(), spBufferOut->GetSize()), E_FAIL);

    
    // validate that the message returned is a success response for a binding request
    ChkIfA(reader.GetMessageClass() != StunMsgClassSuccessResponse, E_FAIL);
    ChkIfA(reader.GetMessageType() != (uint16_t)StunMsgTypeBinding, E_FAIL);
    
    
    // Validate that the message came from the server port we expected
    //    and that it's the same address the server set for the origin address
    ChkIfA(msgOut.socketrole != RolePP, E_FAIL);
    ChkA(ValidateResponseOriginAddress(reader, _addrServerPP));
    ChkIfA(msgOut.addrDest.IsSameIP_and_Port(_addrMapped)==false, E_FAIL);
    
    // validate that the mapping was done correctly
    ChkA(ValidateMappedAddress(reader, _addrMapped, false));
    
    ChkA(ValidateOtherAddress(reader, _addrServerAA));

    
Cleanup:
    return hr;
}

// Test2 - send a binding request to a duplex server instructing it to send back on it's alternate port and alternate IP to an alternate client port
HRESULT CTestMessageHandler::Test2()
{
    HRESULT hr = S_OK;
    CStunMessageBuilder builder;
    CRefCountedBuffer spBuffer, spBufferOut(new CBuffer(MAX_STUN_MESSAGE_SIZE));
    CStunMessageReader reader;
    StunMessageIn msgIn;
    StunMessageOut msgOut;
    TransportAddressSet tas = {};
    uint16_t responsePort = 2222;
    StunChangeRequestAttribute changereq;
    CStunMessageReader::ReaderParseState state;
    CSocketAddress addrDestExpected;
    
    
    InitTransportAddressSet(tas, true, true, true, true);
    
    
    InitBindingRequest(builder);
    
    
    builder.AddResponsePort(responsePort);
    
    changereq.fChangeIP = true;
    changereq.fChangePort = true;
    builder.AddChangeRequest(changereq);
    builder.AddResponsePort(responsePort);
    builder.GetResult(&spBuffer);
    
    ChkIfA(CStunMessageReader::BodyValidated != reader.AddBytes(spBuffer->GetData(), spBuffer->GetSize()), E_FAIL);

    msgIn.fConnectionOriented = false;
    msgIn.addrLocal = _addrServerPP;
    msgIn.pReader = &reader;
    msgIn.socketrole = RolePP;
    msgIn.addrRemote = _addrMapped;
    
    msgOut.socketrole = RolePP; // deliberate initialized wrong
    msgOut.spBufferOut = spBufferOut;
    
    ChkA(CStunRequestHandler::ProcessRequest(msgIn, msgOut, &tas, NULL));
    
    // parse the response
    reader.Reset();
    state = reader.AddBytes(spBufferOut->GetData(), spBufferOut->GetSize());
    ChkIfA(state != CStunMessageReader::BodyValidated, E_FAIL);
    
    // validate that the message was sent back from the AA
    ChkIfA(msgOut.socketrole != RoleAA, E_FAIL);
    // validate that the server though it was sending back from the AA
    ChkA(ValidateResponseOriginAddress(reader, _addrServerAA));
    
    // validate that the message was sent to the response port requested
    addrDestExpected = _addrMapped;
    addrDestExpected.SetPort(responsePort);
    ChkIfA(addrDestExpected.IsSameIP_and_Port(msgOut.addrDest)==false, E_FAIL);
    
    // validate that the binding response came back
    ChkA(ValidateMappedAddress(reader, _addrMapped, false));
    
    // the "other" address is still AA (See RFC 3489 - section 8.1)
    ChkA(ValidateOtherAddress(reader, _addrServerAA));
    

Cleanup:

    return hr;
}






// test simple authentication
HRESULT CTestMessageHandler::Test3()
{
    CStunMessageBuilder builder1, builder2, builder3;
    CStunMessageReader readerResponse;
    uint16_t errorcode = 0;
    HRESULT hr = S_OK;
    

    // -----------------------------------------------------------------------
    // simulate an authorized user making a request with a valid password
    
    ChkA(InitBindingRequest(builder1));
    builder1.AddStringAttribute(STUN_ATTRIBUTE_USERNAME, "AuthorizedUser");
    builder1.AddMessageIntegrityShortTerm("password");
    builder1.FixLengthField();
    
    ChkA(SendHelper(builder1, &readerResponse, _spAuthShort));
    ChkA(readerResponse.ValidateMessageIntegrityShort("password"));
    

    // -----------------------------------------------------------------------
    // simulate a user with a bad password
    readerResponse.Reset();
    InitBindingRequest(builder2);
    builder2.AddStringAttribute(STUN_ATTRIBUTE_USERNAME, "WrongUser");
    builder2.AddMessageIntegrityShortTerm("wrongpassword");
    builder2.FixLengthField();
    
    ChkA(SendHelper(builder2, &readerResponse, _spAuthShort))
    
    errorcode = 0;
    ChkA(readerResponse.GetErrorCode(&errorcode));
    ChkIfA(errorcode != ::STUN_ERROR_UNAUTHORIZED, E_FAIL);
    
    // -----------------------------------------------------------------------
    // simulate a client sending no credentials - we expect it to fire back with a 400/bad-request
    readerResponse.Reset();
    ChkA(InitBindingRequest(builder3));
    
    ChkA(SendHelper(builder3, &readerResponse, _spAuthShort));
    
    errorcode = 0;
    ChkA(readerResponse.GetErrorCode(&errorcode));
    ChkIfA(errorcode != ::STUN_ERROR_BADREQUEST, E_FAIL);
    
Cleanup:
    return hr;
    
}


// test long-credential authentication
HRESULT CTestMessageHandler::Test4()
{
    HRESULT hr=S_OK;
    CStunMessageBuilder builder1, builder2;
    CStunMessageReader readerResponse;
    CSocketAddress addrMapped;
    uint16_t errorcode = 0;
    char szNonce[MAX_STUN_AUTH_STRING_SIZE+1];
    char szRealm[MAX_STUN_AUTH_STRING_SIZE+1];
    

    // -----------------------------------------------------------------------
    // simulate a user making a request with no message integrity attribute (or username, or realm)
    InitBindingRequest(builder1);
    builder1.FixLengthField();
    
    ChkA(SendHelper(builder1, &readerResponse, _spAuthLong));
    
    Chk(readerResponse.GetErrorCode(&errorcode));
    
    ChkIfA(readerResponse.GetMessageClass() != ::StunMsgClassFailureResponse, E_UNEXPECTED);
    ChkIf(errorcode != ::STUN_ERROR_UNAUTHORIZED, E_UNEXPECTED);

    readerResponse.GetStringAttributeByType(STUN_ATTRIBUTE_REALM, szRealm, ARRAYSIZE(szRealm));
    readerResponse.GetStringAttributeByType(STUN_ATTRIBUTE_NONCE, szNonce, ARRAYSIZE(szNonce));
    
    
    // --------------------------------------------------------------------------------
    // now simulate the follow-up request
    readerResponse.Reset();
    InitBindingRequest(builder2);
    builder2.AddNonce(szNonce);
    builder2.AddRealm(szRealm);
    builder2.AddUserName("AuthorizedUser");
    builder2.AddMessageIntegrityLongTerm("AuthorizedUser", szRealm, "password");
    builder2.FixLengthField();
    
    ChkA(SendHelper(builder2, &readerResponse, _spAuthLong));
    
    ChkIfA(readerResponse.GetMessageClass() != ::StunMsgClassSuccessResponse, E_UNEXPECTED);
    
    // should have a mapped address
    ChkA(readerResponse.GetMappedAddress(&addrMapped));
    
    // and the message integrity field should be valid
    ChkA(readerResponse.ValidateMessageIntegrityLong("AuthorizedUser", szRealm, "password"));
    
Cleanup:
    return hr;
}


HRESULT CTestMessageHandler::Run()
{
    
    HRESULT hr = S_OK;

    Chk(Test1());
    Chk(Test2());
    Chk(Test3());
    Chk(Test4());
    
Cleanup:
    return hr;
}


