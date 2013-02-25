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
#include "messagehandler.h"
#include "socketrole.h"


CStunRequestHandler::CStunRequestHandler() :
_pAuth(NULL),
_pAddrSet(NULL),
_pMsgIn(NULL),
_pMsgOut(NULL),
_integrity(), // zero-init
_error(), // zero-init
_fRequestHasResponsePort(false),
_transid(), // zero-init
_fLegacyMode(false)
{
    
}


HRESULT CStunRequestHandler::ProcessRequest(const StunMessageIn& msgIn, StunMessageOut& msgOut, TransportAddressSet* pAddressSet, /*optional*/ IStunAuth* pAuth)
{
    HRESULT hr = S_OK;
    
    CStunRequestHandler handler;
    
    // parameter checking
    ChkIfA(msgIn.pReader==NULL, E_INVALIDARG);
    ChkIfA(IsValidSocketRole(msgIn.socketrole)==false, E_INVALIDARG);
    
    ChkIfA(msgOut.spBufferOut==NULL, E_INVALIDARG);
    ChkIfA(msgOut.spBufferOut->GetAllocatedSize() < MAX_STUN_MESSAGE_SIZE, E_INVALIDARG);
    
    ChkIf(pAddressSet == NULL, E_INVALIDARG);

    // If we get something that can't be validated as a stun message, don't send back a response
    // STUN RFC may suggest sending back a "500", but I think that's the wrong approach.
    ChkIfA(msgIn.pReader->GetState() != CStunMessageReader::BodyValidated, E_UNEXPECTED);
    
    msgOut.spBufferOut->SetSize(0);
    
    // build the context object to pass around this "C" type code environment
    handler._pAuth = pAuth;
    handler._pAddrSet = pAddressSet;
    handler._pMsgIn = &msgIn;
    handler._pMsgOut = &msgOut;
    
    // pre-prep message out
    handler._pMsgOut->socketrole = handler._pMsgIn->socketrole; // output socket is the socket that sent us the message
    handler._pMsgOut->addrDest = handler._pMsgIn->addrRemote; // destination address is same as source
    
    // now call the function that does all the real work
    hr = handler.ProcessRequestImpl();
    
Cleanup:
    return hr;
}

HRESULT CStunRequestHandler::ProcessRequestImpl()
{
    HRESULT hrResult = S_OK;
    HRESULT hr = S_OK;
    

    // aliases
    CStunMessageReader &reader = *(_pMsgIn->pReader);
    
    uint16_t responseport = 0;
    
    // ignore anything that is not a request (with no response)
    ChkIf(reader.GetMessageClass() != StunMsgClassRequest, E_FAIL);
    
    // pre-prep the error message in case we wind up needing senderrorto send it
    _error.msgtype = reader.GetMessageType();
    _error.msgclass = StunMsgClassFailureResponse;
    
    
    reader.GetTransactionId(&_transid);
    _fLegacyMode = reader.IsMessageLegacyFormat();
    
    // we always try to honor the response port
    reader.GetResponsePort(&responseport);
    if (responseport != 0)
    {
        _fRequestHasResponsePort = true;
        
        if (_pMsgIn->fConnectionOriented)
        {
            // special case for TCP - we can't do a response port for connection oriented sockets
            // so just flag this request as an error
            // todo - consider relaxing this check since the calling code is going to ignore the response address anyway for TCP
            _error.errorcode = STUN_ERROR_BADREQUEST;
        }
        else
        {
            _pMsgOut->addrDest.SetPort(responseport);
        }
    }
    

    if (_error.errorcode == 0)
    {
       if (reader.GetMessageType() != StunMsgTypeBinding)
       {
            // we're going to send back an error response for requests that are not binding requests
            _error.errorcode = STUN_ERROR_BADREQUEST; // invalid request
       }
    }
    
    
    if (_error.errorcode == 0)
    {
        hrResult = ValidateAuth(); // returns S_OK if _pAuth is NULL
        
        // if auth didn't succeed, but didn't set an error code, then setup a generic error response
        if (FAILED(hrResult) && (_error.errorcode == 0))
        {
            _error.errorcode = STUN_ERROR_BADREQUEST;
        }
    }
    
    
    if (_error.errorcode == 0)
    {
        hrResult = ProcessBindingRequest();
        if (FAILED(hrResult) && (_error.errorcode == 0))
        {
            _error.errorcode = STUN_ERROR_BADREQUEST;
        }
    }
    
    if (_error.errorcode != 0)
    {
        BuildErrorResponse();
    }
    
Cleanup:
    return hr;
}

void CStunRequestHandler::BuildErrorResponse()
{
    CStunMessageBuilder builder;
    CRefCountedBuffer spBuffer;
    

    _pMsgOut->spBufferOut->SetSize(0);
    builder.GetStream().Attach(_pMsgOut->spBufferOut, true);
    
    // set RFC 3478 mode if the request appears to be that way
    builder.SetLegacyMode(_fLegacyMode);
    
    builder.AddHeader((StunMessageType)_error.msgtype, _error.msgclass);
    builder.AddTransactionId(_transid);
    builder.AddErrorCode(_error.errorcode, "FAILED");
    
    if ((_error.errorcode == ::STUN_ERROR_UNKNOWNATTRIB) && (_error.attribUnknown != 0))
    {
        builder.AddUnknownAttributes(&_error.attribUnknown, 1);
    }
    else if ((_error.errorcode == ::STUN_ERROR_STALENONCE) || (_error.errorcode == ::STUN_ERROR_UNAUTHORIZED))
    {
        if (_error.szNonce[0])
        {
            builder.AddStringAttribute(STUN_ATTRIBUTE_NONCE, _error.szNonce);
        }
        
        if (_error.szRealm[0])
        {
            builder.AddStringAttribute(STUN_ATTRIBUTE_REALM, _error.szRealm);
        }
    }

    builder.FixLengthField();
    builder.GetResult(&spBuffer);

    ASSERT(spBuffer->GetSize() != 0);
    ASSERT(spBuffer == _pMsgOut->spBufferOut);

    return;
}


HRESULT CStunRequestHandler::ProcessBindingRequest()
{
    CStunMessageReader& reader = *(_pMsgIn->pReader);
    
    bool fRequestHasPaddingAttribute = false;
    SocketRole socketOutput = _pMsgIn->socketrole; // initialize to be from the socket we received from
    StunChangeRequestAttribute changerequest = {};
    bool fSendOtherAddress = false;
    bool fSendOriginAddress = false;
    SocketRole socketOther;
    CSocketAddress addrOrigin;
    CSocketAddress addrOther;
    CStunMessageBuilder builder;
    uint16_t paddingSize = 0;
    HRESULT hrResult;

    
    _pMsgOut->spBufferOut->SetSize(0);
    builder.GetStream().Attach(_pMsgOut->spBufferOut, true);

    // if the client request smells like RFC 3478, then send the resposne back in the same way
    builder.SetLegacyMode(_fLegacyMode);

    // check for an alternate response port
    // check for padding attribute (todo - figure out how to inject padding into the response)
    // check for a change request and validate we can do it. If so, set _socketOutput. If not, fill out _error and return.
    // determine if we have an "other" address to notify the caller about


    // did the request come with a padding request
    if (SUCCEEDED(reader.GetPaddingAttributeSize(&paddingSize)))
    {
        // todo - figure out how we're going to get the MTU size of the outgoing interface
        fRequestHasPaddingAttribute = true;
    }
    
    // as per 5780, section 6.1, If the Request contained a PADDING attribute...
    // "If the Request also contains the RESPONSE-PORT attribute the server MUST return an error response of type 400."
    if (_fRequestHasResponsePort && fRequestHasPaddingAttribute)
    {
        _error.errorcode = STUN_ERROR_BADREQUEST;
        return E_FAIL;
    }
    
    // handle change request logic and figure out what "other-address" attribute is going to be
    // Some clients (like jstun) will send a change-request attribute with neither the IP or PORT flag set
    // So ignore this block of code in that case (because the fConnectionOriented check below could fail)
    hrResult = reader.GetChangeRequest(&changerequest);
    if (SUCCEEDED(hrResult) && (changerequest.fChangeIP || changerequest.fChangePort))
    {
        if (changerequest.fChangeIP)
        {
            socketOutput = SocketRoleSwapIP(socketOutput);
        }
        if(changerequest.fChangePort)
        {
            socketOutput = SocketRoleSwapPort(socketOutput);
        }

        // IsValidSocketRole just validates the enum, not whether or not we can send on it
        ASSERT(IsValidSocketRole(socketOutput));

        // now, make sure we have the ability to send from another socket
        // For TCP/TLS, we can't send back from another port
        if ((HasAddress(socketOutput) == false) || _pMsgIn->fConnectionOriented)
        {
            // send back an error. We're being asked to respond using another address that we don't have a socket for
            _error.errorcode = STUN_ERROR_BADREQUEST;
            return E_FAIL;
        }
    }    

    // If we're only working one socket, then that's ok, we just don't send back an "other address" unless we have all four sockets configured
    // now here's a problem. If we binded to "INADDR_ANY", all of the sockets will have "0.0.0.0" for an address (same for IPV6)
    // So we effectively can't send back "other address" if don't really know our own IP address
    // Fortunately, recvfromex and the ioctls on the socket allow address discovery a bit better
    
    // For TCP, we can send back an other-address.  But it is only meant as as
    // a hint to the client that he can try another server to infer NAT behavior
    // Change-requests are disallowed
    
    // Note - As per RFC 5780 and RFC 3489, "other address" (aka "changed address")
    // attribute is always the ip and port opposite of where the request was
    // received on, irrespective of the client sending a change-requset that influenced
    // the value of socketOutput value above.
    
    fSendOtherAddress = HasAddress(RolePP) && HasAddress(RolePA) && HasAddress(RoleAP) && HasAddress(RoleAA);

    if (fSendOtherAddress)
    {
        socketOther = SocketRoleSwapIP(SocketRoleSwapPort(_pMsgIn->socketrole));
        // so if our ip address is "0.0.0.0", disable this attribute
        fSendOtherAddress = (IsIPAddressZeroOrInvalid(socketOther) == false);
        
        // so if the local address of the other socket isn't known (e.g. ip == "0.0.0.0"), disable this attribute
        if (fSendOtherAddress)
        {
            addrOther = _pAddrSet->set[socketOther].addr;
        }
    }

    // What's our address origin?
    addrOrigin = _pAddrSet->set[socketOutput].addr;
    if (addrOrigin.IsIPAddressZero())
    {
        // Since we're sending back from the IP address we received on, we can just use the address the message came in on
        // Otherwise, we don't actually know it
        if (socketOutput == _pMsgIn->socketrole)
        {
            addrOrigin = _pMsgIn->addrLocal;
        }
    }
    fSendOriginAddress = (false == addrOrigin.IsIPAddressZero());

    // Success - we're all clear to build the response
    _pMsgOut->socketrole = socketOutput;
    

    builder.AddHeader(StunMsgTypeBinding, StunMsgClassSuccessResponse);
    builder.AddTransactionId(_transid);
    
    // paranoia - just to be consistent with Vovida, send the attributes back in the same order it does
    // I suspect there are clients out there that might be hardcoded to the ordering
    
    // MAPPED-ADDRESS
    // SOURCE-ADDRESS (RESPONSE-ORIGIN)
    // CHANGED-ADDRESS (OTHER-ADDRESS)
    // XOR-MAPPED-ADDRESS (XOR-MAPPED_ADDRESS-OPTIONAL)
    
    builder.AddMappedAddress(_pMsgIn->addrRemote);

    if (fSendOriginAddress)
    {
        builder.AddResponseOriginAddress(addrOrigin); // pass true to send back SOURCE_ADDRESS, otherwise, pass false to send back RESPONSE-ORIGIN
    }

    if (fSendOtherAddress)
    {
        builder.AddOtherAddress(addrOther); // pass true to send back CHANGED-ADDRESS, otherwise, pass false to send back OTHER-ADDRESS
    }

    // send back the XOR-MAPPED-ADDRESS (encoded as an optional message for legacy clients)
    builder.AddXorMappedAddress(_pMsgIn->addrRemote);
    
    
    // finally - if we're supposed to have a message integrity attribute as a result of authorization, add it at the very end
    if (_integrity.fSendWithIntegrity)
    {
        if (_integrity.fUseLongTerm == false)
        {
            builder.AddMessageIntegrityShortTerm(_integrity.szPassword);
        }
        else
        {
            builder.AddMessageIntegrityLongTerm(_integrity.szUser, _integrity.szRealm, _integrity.szPassword);
        }
    }

    builder.FixLengthField();

    return S_OK;
}


HRESULT CStunRequestHandler::ValidateAuth()
{
    AuthAttributes authattributes;
    AuthResponse authresponse;
    HRESULT hr = S_OK;
    HRESULT hrRet = S_OK;
    
    // aliases
    CStunMessageReader& reader = *(_pMsgIn->pReader);
    
    if (_pAuth == NULL)
    {
        return S_OK; // nothing to do if there is no auth mechanism in place
    }
    
    memset(&authattributes, '\0', sizeof(authattributes));
    memset(&authresponse, '\0', sizeof(authresponse));
    
    reader.GetStringAttributeByType(STUN_ATTRIBUTE_USERNAME, authattributes.szUser, ARRAYSIZE(authattributes.szUser));
    reader.GetStringAttributeByType(STUN_ATTRIBUTE_REALM, authattributes.szRealm, ARRAYSIZE(authattributes.szRealm));
    reader.GetStringAttributeByType(STUN_ATTRIBUTE_NONCE, authattributes.szNonce, ARRAYSIZE(authattributes.szNonce));
    reader.GetStringAttributeByType(::STUN_ATTRIBUTE_LEGACY_PASSWORD, authattributes.szLegacyPassword, ARRAYSIZE(authattributes.szLegacyPassword));
    authattributes.fMessageIntegrityPresent = reader.HasMessageIntegrityAttribute();
    
    Chk(_pAuth->DoAuthCheck(&authattributes, &authresponse));
    
    // enforce that everything is null terminated
    authresponse.szNonce[ARRAYSIZE(authresponse.szNonce)-1] = 0;
    authresponse.szRealm[ARRAYSIZE(authresponse.szRealm)-1] = 0;
    authresponse.szPassword[ARRAYSIZE(authresponse.szPassword)-1] = 0;

    // now decide how to handle the auth
    if (authresponse.responseType == StaleNonce)
    {
        _error.errorcode = STUN_ERROR_STALENONCE;
    }
    else if (authresponse.responseType == Unauthorized)
    {
        _error.errorcode = STUN_ERROR_UNAUTHORIZED;
    }
    else if (authresponse.responseType == Reject)
    {
        _error.errorcode = STUN_ERROR_BADREQUEST;
    }
    else if (authresponse.responseType == Allow)
    {
        // nothing to do!
    }
    else if (authresponse.responseType == AllowConditional)
    {
        // validate the message in // if either ValidateAuth or ProcessBindingRequest set an errorcode....

        if (authresponse.authCredMech == AuthCredLongTerm)
        {
            hrRet = reader.ValidateMessageIntegrityLong(authattributes.szUser, authattributes.szRealm, authresponse.szPassword);
        }
        else
        {
            hrRet = reader.ValidateMessageIntegrityShort(authresponse.szPassword);
        }
        
        if (SUCCEEDED(hrRet))
        {
            _integrity.fSendWithIntegrity = true;
            _integrity.fUseLongTerm = (authresponse.authCredMech == AuthCredLongTerm);
            
            COMPILE_TIME_ASSERT(sizeof(_integrity.szPassword)==sizeof(authresponse.szPassword));
            
            strcpy(_integrity.szPassword, authresponse.szPassword);
            strcpy(_integrity.szUser, authattributes.szUser);
            strcpy(_integrity.szRealm, authattributes.szRealm);
        }
        else
        {
            // bad password - so now turn this thing into a 401
            _error.errorcode = STUN_ERROR_UNAUTHORIZED;
        }
    }
    
    if ((_error.errorcode == STUN_ERROR_UNAUTHORIZED) || (_error.errorcode == STUN_ERROR_STALENONCE))
    {
        strcpy(_error.szRealm, authresponse.szRealm);
        strcpy(_error.szNonce, authresponse.szNonce);
    }

Cleanup:
    return hr;
}

bool CStunRequestHandler::HasAddress(SocketRole role)
{
    return (_pAddrSet && ::IsValidSocketRole(role) && _pAddrSet->set[role].fValid);
}

bool CStunRequestHandler::IsIPAddressZeroOrInvalid(SocketRole role)
{
    bool fValid = HasAddress(role) && (_pAddrSet->set[role].addr.IsIPAddressZero()==false);
    return !fValid;
}

