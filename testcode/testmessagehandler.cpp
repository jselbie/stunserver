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

#include "stunauth.h"
#include "testmessagehandler.h"



CMockTransport::CMockTransport()
{
    ;
}

CMockTransport::~CMockTransport()
{
    ;
}

HRESULT CMockTransport::Reset()
{
    for (unsigned int index = 0; index < ARRAYSIZE(m_addrs); index++)
    {
        m_addrs[index] = CSocketAddress();
    }
    ClearStream();

    return S_OK;
}

HRESULT CMockTransport::ClearStream()

{
    m_outputstream.Reset();
    m_outputRole = (SocketRole)-1;
    m_addrDestination = CSocketAddress();
    return S_OK;
}


HRESULT CMockTransport::AddPP(const CSocketAddress& addr)
{
    int index = RolePP;
    m_addrs[index] = addr;
    return S_OK;
}

HRESULT CMockTransport::AddPA(const CSocketAddress& addr)
{
    int index = RolePA;
    m_addrs[index] = addr;
    return S_OK;
}


HRESULT CMockTransport::AddAP(const CSocketAddress& addr)
{
    int index = RoleAP;
    m_addrs[index] = addr;
    return S_OK;
}

HRESULT CMockTransport::AddAA(const CSocketAddress& addr)
{
    int index = RoleAA;
    m_addrs[index] = addr;
    return S_OK;
}



HRESULT CMockTransport::SendResponse(SocketRole roleOutput, const CSocketAddress& addr, CRefCountedBuffer& spResponse)
{
    m_outputRole = roleOutput;
    m_addrDestination = addr;
    m_outputstream.Write(spResponse->GetData(), spResponse->GetSize());

    return S_OK;
}

bool CMockTransport::HasAddress(SocketRole role)
{
    int index = (int)role;
    ASSERT(index >= 0);
    ASSERT(index <= 3);

    return (m_addrs[index].GetPort() != 0);
}

HRESULT CMockTransport::GetSocketAddressForRole(SocketRole role, CSocketAddress* pAddr)
{
    HRESULT hr=S_OK;

    if (HasAddress(role))
    {
       *pAddr = m_addrs[(int)role];
    }
    else
    {
        hr = E_FAIL;
    }

    return hr;
}



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
    CMockTransport::CreateInstanceNoInit(_spTransport.GetPointerPointer());
    CMockAuthShort::CreateInstanceNoInit(_spAuthShort.GetPointerPointer());
    CMockAuthLong::CreateInstanceNoInit(_spAuthLong.GetPointerPointer());
}

HRESULT CTestMessageHandler::InitBindingRequest(CStunMessageBuilder& builder)
{
    StunTransactionId transid;
    builder.AddHeader(StunMsgTypeBinding, StunMsgClassRequest);
    builder.AddRandomTransactionId(&transid);
    builder.FixLengthField();
    return S_OK;
}

HRESULT CTestMessageHandler::ValidateMappedAddress(CStunMessageReader& reader, const CSocketAddress& addrClient)
{
    HRESULT hr = S_OK;
    StunTransactionId transid;
    CSocketAddress mappedaddr;
    
    CRefCountedBuffer spBuffer;
    
    Chk(reader.GetStream().GetBuffer(&spBuffer));
    
    reader.GetTransactionId(&transid);

    //ChkA(reader.GetAttributeByType(STUN_ATTRIBUTE_XORMAPPEDADDRESS, &attrib));
    //ChkA(GetXorMappedAddress(spBuffer->GetData()+attrib.offset, attrib.size, transid, &mappedaddr));
    reader.GetXorMappedAddress(&mappedaddr);
    ChkIfA(false == addrClient.IsSameIP_and_Port(mappedaddr), E_FAIL);    
    
    //ChkA(reader.GetAttributeByType(STUN_ATTRIBUTE_MAPPEDADDRESS, &attrib));
    //ChkA(GetMappedAddress(spBuffer->GetData()+attrib.offset, attrib.size, &mappedaddr));
    
    reader.GetMappedAddress(&mappedaddr);
    ChkIfA(false == addrClient.IsSameIP_and_Port(mappedaddr), E_FAIL);    
    
Cleanup:
    return hr;
}

HRESULT CTestMessageHandler::ValidateOriginAddress(CStunMessageReader& reader, SocketRole socketExpected)
{
    HRESULT hr = S_OK;
    StunAttribute attrib;
    CSocketAddress addrExpected, mappedaddr;
    
    CRefCountedBuffer spBuffer;
    Chk(reader.GetStream().GetBuffer(&spBuffer));
    
    Chk(_spTransport->GetSocketAddressForRole(socketExpected, &addrExpected));
    

    ChkA(reader.GetAttributeByType(STUN_ATTRIBUTE_RESPONSE_ORIGIN, &attrib));

    
    ChkA(GetMappedAddress(spBuffer->GetData()+attrib.offset, attrib.size, &mappedaddr));
    ChkIfA(false == addrExpected.IsSameIP_and_Port(mappedaddr), E_FAIL);    
    
    ChkIfA(socketExpected != _spTransport->m_outputRole, E_FAIL);
    
Cleanup:
    return hr;
}

HRESULT CTestMessageHandler::ValidateResponseAddress(const CSocketAddress& addr)
{
    HRESULT hr = S_OK;
    
    if (false == _spTransport->m_addrDestination.IsSameIP_and_Port(addr))
    {
        hr = E_FAIL;
    }
    
    return hr;
}



// Test1 - just do a basic binding request
HRESULT CTestMessageHandler::Test1()
{
    HRESULT hr=S_OK;
    CStunMessageBuilder builder;
    CSocketAddress clientaddr(0x12345678, 9876);
    CRefCountedBuffer spBuffer;
    CStunThreadMessageHandler handler;
    CStunMessageReader reader;
    CStunMessageReader::ReaderParseState state;
    StunMessageEnvelope message;

    _spTransport->Reset();
    _spTransport->AddPP(CSocketAddress(0xaaaaaaaa, 1234));

    InitBindingRequest(builder);

    builder.GetStream().GetBuffer(&spBuffer);

    handler.SetResponder(_spTransport);
    
    
    message.localSocket = RolePP;
    message.remoteAddr = clientaddr;
    message.spBuffer = spBuffer;
    _spTransport->GetSocketAddressForRole(message.localSocket, &(message.localAddr));
    
    handler.ProcessRequest(message);


    spBuffer.reset();
    _spTransport->GetOutputStream().GetBuffer(&spBuffer);

    state = reader.AddBytes(spBuffer->GetData(), spBuffer->GetSize());

    ChkIfA(state != CStunMessageReader::BodyValidated, E_FAIL);

    // validate that the binding response matches our expectations
    ChkA(ValidateMappedAddress(reader, clientaddr));
    
    // validate that it came from the server port we expected
    ChkA(ValidateOriginAddress(reader, RolePP));

    // did we get back the binding request we expected
    ChkA(ValidateResponseAddress(clientaddr));
    
Cleanup:

    return hr;
}


// send a binding request to a duplex server instructing it to send back on it's alternate port and alternate IP to an alternate client port
HRESULT CTestMessageHandler::Test2()
{
    HRESULT hr=S_OK;
    CStunMessageBuilder builder;
    CSocketAddress clientaddr(0x12345678, 9876);
    CSocketAddress recvaddr;
    uint16_t responsePort = 2222;
    CRefCountedBuffer spBuffer;
    CStunThreadMessageHandler handler;
    CStunMessageReader reader;
    CStunMessageReader::ReaderParseState state;
    ::StunChangeRequestAttribute changereq;
    StunMessageEnvelope message;
    
    _spTransport->Reset();
    _spTransport->AddPP(CSocketAddress(0xaaaaaaaa, 1234));
    _spTransport->AddPA(CSocketAddress(0xaaaaaaaa, 1235));
    _spTransport->AddAP(CSocketAddress(0xbbbbbbbb, 1234));
    _spTransport->AddAA(CSocketAddress(0xbbbbbbbb, 1235));
    
    InitBindingRequest(builder);
    
    builder.AddResponsePort(responsePort);
    
    changereq.fChangeIP = true;
    changereq.fChangePort = true;
    builder.AddChangeRequest(changereq);
    builder.AddResponsePort(responsePort);
    builder.GetResult(&spBuffer);
    
    message.localSocket = RolePP;
    message.remoteAddr = clientaddr;
    message.spBuffer = spBuffer;
    _spTransport->GetSocketAddressForRole(RolePP, &(message.localAddr));
    
    handler.SetResponder(_spTransport);
    handler.ProcessRequest(message);
    
    spBuffer->Reset();
    _spTransport->GetOutputStream().GetBuffer(&spBuffer);
    
    // parse the response
    state = reader.AddBytes(spBuffer->GetData(), spBuffer->GetSize());
    ChkIfA(state != CStunMessageReader::BodyValidated, E_FAIL);
    

    // validate that the binding response matches our expectations
    ChkA(ValidateMappedAddress(reader, clientaddr));
    
    ChkA(ValidateOriginAddress(reader, RoleAA));
    
    // did it get sent back to where we thought it was
    recvaddr = clientaddr;
    recvaddr.SetPort(responsePort);
    ChkA(ValidateResponseAddress(recvaddr));
    

Cleanup:

    return hr;
}


// test simple authentication
HRESULT CTestMessageHandler::Test3()
{
    HRESULT hr=S_OK;
    CStunMessageBuilder builder1, builder2, builder3;
    CStunMessageReader reader1, reader2, reader3;
    CSocketAddress clientaddr(0x12345678, 9876);
    CRefCountedBuffer spBuffer;
    CStunThreadMessageHandler handler;
    uint16_t errorcode = 0;
    
    CStunMessageReader::ReaderParseState state;
    StunMessageEnvelope message;
    
    _spTransport->Reset();
    _spTransport->AddPP(CSocketAddress(0xaaaaaaaa, 1234));
    
    handler.SetAuth(_spAuthShort);
    handler.SetResponder(_spTransport);

    // -----------------------------------------------------------------------
    // simulate an authorized user making a request with a valid password
    InitBindingRequest(builder1);
    builder1.AddStringAttribute(STUN_ATTRIBUTE_USERNAME, "AuthorizedUser");
    builder1.AddMessageIntegrityShortTerm("password");
    builder1.GetResult(&spBuffer);
    
    message.localSocket = RolePP;
    message.remoteAddr = clientaddr;
    message.spBuffer = spBuffer;
    _spTransport->GetSocketAddressForRole(message.localSocket, &(message.localAddr));
    
    handler.ProcessRequest(message);
    
    // we expect back a response with a valid message integrity field
    spBuffer.reset();
    _spTransport->m_outputstream.GetBuffer(&spBuffer);
    
    state = reader1.AddBytes(spBuffer->GetData(), spBuffer->GetSize());
    ChkIfA(state != CStunMessageReader::BodyValidated, E_FAIL);
    ChkA(reader1.ValidateMessageIntegrityShort("password"));
    

    // -----------------------------------------------------------------------
    // simulate a user with a bad password
    spBuffer.reset();
    InitBindingRequest(builder2);
    builder2.AddStringAttribute(STUN_ATTRIBUTE_USERNAME, "WrongUser");
    builder2.AddMessageIntegrityShortTerm("wrongpassword");
    builder2.GetResult(&spBuffer);
    
    message.localSocket = RolePP;
    message.remoteAddr = clientaddr;
    message.spBuffer = spBuffer;
    _spTransport->GetSocketAddressForRole(message.localSocket, &(message.localAddr));

    _spTransport->ClearStream();
    handler.ProcessRequest(message);
    
    spBuffer.reset();
    _spTransport->m_outputstream.GetBuffer(&spBuffer);
    
    state = reader2.AddBytes(spBuffer->GetData(), spBuffer->GetSize());
    ChkIfA(state != CStunMessageReader::BodyValidated, E_FAIL);
    errorcode = 0;
    ChkA(reader2.GetErrorCode(&errorcode));
    ChkIfA(errorcode != ::STUN_ERROR_UNAUTHORIZED, E_FAIL);
    
    // -----------------------------------------------------------------------
    // simulate a client sending no credentials - we expect it to fire back with a 400/bad-request
    spBuffer.reset();
    InitBindingRequest(builder3);
    builder3.GetResult(&spBuffer);
    
    message.localSocket = RolePP;
    message.remoteAddr = clientaddr;
    message.spBuffer = spBuffer;
    _spTransport->GetSocketAddressForRole(message.localSocket, &(message.localAddr));

    _spTransport->ClearStream();
    handler.ProcessRequest(message);
    
    spBuffer.reset();
    _spTransport->m_outputstream.GetBuffer(&spBuffer);
    
    
    state = reader3.AddBytes(spBuffer->GetData(), spBuffer->GetSize());
    ChkIfA(state != CStunMessageReader::BodyValidated, E_FAIL);
    errorcode = 0;
    ChkA(reader3.GetErrorCode(&errorcode));
    ChkIfA(errorcode != ::STUN_ERROR_BADREQUEST, E_FAIL);
    
    
Cleanup:

    return hr;
    
}

// test long-credential authentication
HRESULT CTestMessageHandler::Test4()
{
    HRESULT hr=S_OK;
    CStunMessageBuilder builder1, builder2;
    CStunMessageReader reader1, reader2;
    CSocketAddress clientaddr(0x12345678, 9876);
    CSocketAddress addrMapped;
    CRefCountedBuffer spBuffer;
    CStunThreadMessageHandler handler;
    uint16_t errorcode = 0;
    char szNonce[MAX_STUN_AUTH_STRING_SIZE+1];
    char szRealm[MAX_STUN_AUTH_STRING_SIZE+1];
    
    CStunMessageReader::ReaderParseState state;
    StunMessageEnvelope message;
    
    _spTransport->Reset();
    _spTransport->AddPP(CSocketAddress(0xaaaaaaaa, 1234));
    
    handler.SetAuth(_spAuthLong);
    handler.SetResponder(_spTransport);

    // -----------------------------------------------------------------------
    // simulate a user making a request with no message integrity attribute (or username, or realm)
    InitBindingRequest(builder1);
    builder1.GetResult(&spBuffer);
    message.localSocket = RolePP;
    message.remoteAddr = clientaddr;
    message.spBuffer = spBuffer;
    _spTransport->GetSocketAddressForRole(message.localSocket, &(message.localAddr));
    
    handler.ProcessRequest(message);
    
    spBuffer.reset();
    _spTransport->m_outputstream.GetBuffer(&spBuffer);
    state = reader1.AddBytes(spBuffer->GetData(), spBuffer->GetSize());
    
    ChkIfA(state != CStunMessageReader::BodyValidated, E_FAIL);
    // we expect the response back will be a 401 with a provided nonce and realm
    Chk(reader1.GetErrorCode(&errorcode));
    
    ChkIfA(reader1.GetMessageClass() != ::StunMsgClassFailureResponse, E_UNEXPECTED);
    ChkIf(errorcode != ::STUN_ERROR_UNAUTHORIZED, E_UNEXPECTED);

    reader1.GetStringAttributeByType(STUN_ATTRIBUTE_REALM, szRealm, ARRAYSIZE(szRealm));
    reader1.GetStringAttributeByType(STUN_ATTRIBUTE_NONCE, szNonce, ARRAYSIZE(szNonce));
    
    
    // --------------------------------------------------------------------------------
    // now simulate the follow-up request
    _spTransport->ClearStream();
    spBuffer.reset();
    InitBindingRequest(builder2);
    builder2.AddNonce(szNonce);
    builder2.AddRealm(szRealm);
    builder2.AddUserName("AuthorizedUser");
    builder2.AddMessageIntegrityLongTerm("AuthorizedUser", szRealm, "password");
    builder2.GetResult(&spBuffer);
    
    message.localSocket = RolePP;
    message.remoteAddr = clientaddr;
    message.spBuffer = spBuffer;
    _spTransport->GetSocketAddressForRole(message.localSocket, &(message.localAddr));
    
    handler.ProcessRequest(message);
    
    spBuffer.reset();
    _spTransport->m_outputstream.GetBuffer(&spBuffer);
    
    state = reader2.AddBytes(spBuffer->GetData(), spBuffer->GetSize());
    ChkIfA(state != CStunMessageReader::BodyValidated, E_FAIL);
    ChkIfA(reader2.GetMessageClass() != ::StunMsgClassSuccessResponse, E_UNEXPECTED);
    
    // should have a mapped address
    ChkA(reader2.GetMappedAddress(&addrMapped));
    
    // and the message integrity field should be valid
    ChkA(reader2.ValidateMessageIntegrityLong("AuthorizedUser", szRealm, "password"));
    
    
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


