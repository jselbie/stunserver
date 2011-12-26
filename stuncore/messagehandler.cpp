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
#include "stunresponder.h"
#include "messagehandler.h"


CStunThreadMessageHandler::CStunThreadMessageHandler()
{
    CRefCountedBuffer spReaderBuffer(new CBuffer(1500));
    CRefCountedBuffer spResponseBuffer(new CBuffer(1500));

    _spReaderBuffer.swap(spReaderBuffer);
    _spResponseBuffer.swap(spResponseBuffer);
}

CStunThreadMessageHandler::~CStunThreadMessageHandler()
{
    ;
}

void CStunThreadMessageHandler::SetResponder(IStunResponder* pTransport)
{
    _spStunResponder = pTransport;
}

void CStunThreadMessageHandler::SetAuth(IStunAuth* pAuth)
{
    _spAuth = pAuth;
}

void CStunThreadMessageHandler::ProcessRequest(StunMessageEnvelope& message)
{
    CStunMessageReader reader;
    CStunMessageReader::ReaderParseState state;
    uint16_t responsePort = 0;
    HRESULT hr = S_OK;


    ChkIfA(_spStunResponder == NULL, E_FAIL);

    _spReaderBuffer->SetSize(0);
    _spResponseBuffer->SetSize(0);
    _message = message;

    _addrResponse = message.remoteAddr;
    _socketOutput = message.localSocket;
    _fRequestHasResponsePort = false;
    
    // zero out _error without the overhead of zero'ing out every byte in the strings
    _error.errorcode = 0;
    _error.szNonce[0] = 0;
    _error.szRealm[0] = 0;
    _error.attribUnknown = 0;
    
    _integrity.fSendWithIntegrity = false;
    _integrity.szUser[0] = '\0';
    _integrity.szRealm[0] = '\0';
    _integrity.szPassword[0] = '\0';
    
    
    // attach the temp buffer to reader
    reader.GetStream().Attach(_spReaderBuffer, true);


    reader.SetAllowLegacyFormat(true);

    // parse the request
    state = reader.AddBytes(message.spBuffer->GetData(), message.spBuffer->GetSize());

    // If we get something that can't be validated as a stun message, don't send back a response
    // STUN RFC may suggest sending back a "500", but I think that's the wrong approach.
    ChkIf (state != CStunMessageReader::BodyValidated, E_FAIL);
    
    // Regardless of what we send back, let's always attempt to honor a response port request
    // Fix the destination port if the client asked for us to send back to another port
    if (SUCCEEDED(reader.GetResponsePort(&responsePort)))
    {
        _addrResponse.SetPort(responsePort);
        _fRequestHasResponsePort = true;
    }

    reader.GetTransactionId(&_transid);

    // ignore anything that is not a request (with no response)
    ChkIf(reader.GetMessageClass() != StunMsgClassRequest, E_FAIL);

    // pre-prep the error message in case we wind up needing to send it
    _error.msgtype = reader.GetMessageType();
    _error.msgclass = StunMsgClassFailureResponse;

    if (reader.GetMessageType() != StunMsgTypeBinding)
    {
        // we're going to send back an error response
        _error.errorcode = STUN_ERROR_BADREQUEST; // invalid request
    }
    else
    {
        // handle authentication - but only if an auth provider has been set
        hr = ValidateAuth(reader);

        // if auth succeeded, then carry on to handling the request
        if (SUCCEEDED(hr) && (_error.errorcode==0))
        {
            // handle the binding request
            hr = ProcessBindingRequest(reader);
        }

        // catch all for any case where an error occurred
        if (FAILED(hr) && (_error.errorcode==0))
        {
            _error.errorcode = STUN_ERROR_BADREQUEST;
        }
    }

    if (_error.errorcode != 0)
    {
        // if either ValidateAuth or ProcessBindingRequest set an errorcode, or a fatal error occurred
        SendErrorResponse();
    }
    else
    {
        SendResponse();
    }


Cleanup:
    return;
}

void CStunThreadMessageHandler::SendResponse()
{
    HRESULT hr = S_OK;

    ChkIfA(_spStunResponder == NULL, E_FAIL);
    ChkIfA(_spResponseBuffer->GetSize() <= 0, E_FAIL);

    Chk(_spStunResponder->SendResponse(_socketOutput, _addrResponse, _spResponseBuffer));

Cleanup:
    return;
}

void CStunThreadMessageHandler::SendErrorResponse()
{
    HRESULT hr = S_OK;

    CStunMessageBuilder builder;
    CRefCountedBuffer spBuffer;

    _spResponseBuffer->SetSize(0);
    builder.GetStream().Attach(_spResponseBuffer, true);

    builder.AddHeader((StunMessageType)_error.msgtype, _error.msgclass);
    builder.AddTransactionId(_transid);
    builder.AddErrorCode(_error.errorcode, "FAILED  "); // quick fix.  "two spaces" added for rfc 3478. A better fix will exist in version 1.1
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

    ChkIfA(_spStunResponder == NULL, E_FAIL);

    builder.GetResult(&spBuffer);

    ASSERT(spBuffer->GetSize() != 0);
    ASSERT(spBuffer == _spResponseBuffer);

    _spStunResponder->SendResponse(_socketOutput, _addrResponse, spBuffer);

Cleanup:
    return;

}

HRESULT CStunThreadMessageHandler::ProcessBindingRequest(CStunMessageReader& reader)
{

    HRESULT hrTmp;
    bool fRequestHasPaddingAttribute = false;
    SocketRole socketOutput = _message.localSocket;
    StunChangeRequestAttribute changerequest = {};
    bool fSendOtherAddress = false;
    bool fSendOriginAddress = false;
    SocketRole socketOther;
    CSocketAddress addrOrigin;
    CSocketAddress addrOther;
    CStunMessageBuilder builder;
    uint16_t paddingSize = 0;
    bool fLegacyFormat = false; // set to true if the client appears to be rfc3489 based instead of based on rfc 5789


    _spResponseBuffer->SetSize(0);
    builder.GetStream().Attach(_spResponseBuffer, true);
    
    fLegacyFormat = reader.IsMessageLegacyFormat();

    // check for an alternate response port
    // check for padding attribute (todo - figure out how to inject padding into the response)
    // check for a change request and validate we can do it.  If so, set _socketOutput.  If not, fill out _error and return.
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
    if (SUCCEEDED(reader.GetChangeRequest(&changerequest)))
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
        if (_spStunResponder->HasAddress(socketOutput) == false)
        {
            // send back an error.  We're being asked to respond using another address that we don't have a socket for
            _error.errorcode = STUN_ERROR_BADREQUEST;
            return E_FAIL;
        }
    }

    // If we're only working one socket, then that's ok, we just don't send back an "other address" unless we have all four sockets confgiured

    // now here's a problem.  If we binded to "INADDR_ANY", all of the sockets will have "0.0.0.0" for an address (same for IPV6)
    // So we effectively can't send back "other address" if don't really know our own IP address
    // Fortunately, recvfromex and the ioctls on the socket allow address discovery a bit better

    fSendOtherAddress = (_spStunResponder->HasAddress(RolePP) && _spStunResponder->HasAddress(RolePA) && _spStunResponder->HasAddress(RoleAP) && _spStunResponder->HasAddress(RoleAA));

    if (fSendOtherAddress)
    {
        socketOther = SocketRoleSwapIP(SocketRoleSwapPort(_message.localSocket));

        hrTmp = _spStunResponder->GetSocketAddressForRole(socketOther, &addrOther);
        ASSERT(SUCCEEDED(hrTmp));

        // so if our ip address is "0.0.0.0", disable this attribute
        fSendOtherAddress = (SUCCEEDED(hrTmp) && (addrOther.IsIPAddressZero()==false));
    }

    // What's our address origin?
    VERIFY(SUCCEEDED(_spStunResponder->GetSocketAddressForRole(socketOutput, &addrOrigin)));
    if (addrOrigin.IsIPAddressZero())
    {
        // Since we're sending back from the IP address we received on, we can just use the address the message came in on
        // Otherwise, we don't actually know it
        if (socketOutput == _message.localSocket)
        {
            addrOrigin = _message.localAddr;
        }
    }
    fSendOriginAddress = (false == addrOrigin.IsIPAddressZero());

    // Success - we're all clear to build the response

    _socketOutput = socketOutput;

    _spResponseBuffer->SetSize(0);
    builder.GetStream().Attach(_spResponseBuffer, true);

    builder.AddHeader(StunMsgTypeBinding, StunMsgClassSuccessResponse);
    builder.AddTransactionId(_transid);
    builder.AddMappedAddress(_message.remoteAddr);

    if (fLegacyFormat == false)
    {
        builder.AddXorMappedAddress(_message.remoteAddr);
    }

    if (fSendOriginAddress)
    {
        builder.AddResponseOriginAddress(addrOrigin);
    }

    if (fSendOtherAddress)
    {
        builder.AddOtherAddress(addrOther, fLegacyFormat); // pass true to send back CHANGED-ADDRESS, otherwise, pass false to send back OTHER-ADDRESS
    }
    
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


HRESULT CStunThreadMessageHandler::ValidateAuth(CStunMessageReader& reader)
{
    AuthAttributes authattributes;
    AuthResponse authresponse;
    HRESULT hr = S_OK;
    HRESULT hrRet = S_OK;
    
    if (_spAuth == NULL)
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
    
    Chk(_spAuth->DoAuthCheck(&authattributes, &authresponse));
    
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
        // validate the message in    // if either ValidateAuth or ProcessBindingRequest set an errorcode....

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


