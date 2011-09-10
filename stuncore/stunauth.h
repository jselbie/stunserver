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


#ifndef STUNAUTH_H_
#define STUNAUTH_H_


const uint32_t MAX_STUN_AUTH_STRING_SIZE = 64;  // max string size for username, realm, password, nonce attributes



struct AuthAttributes
{
    // attributes in the request
    char szUser[MAX_STUN_AUTH_STRING_SIZE+1];  // the user name attribute in the request (if available)
    char szRealm[MAX_STUN_AUTH_STRING_SIZE+1]; // the realm attribute in the request (if available)
    char szNonce[MAX_STUN_AUTH_STRING_SIZE+1]; // the nonce attribute in the request (if available)
    char szLegacyPassword[MAX_STUN_AUTH_STRING_SIZE+1]; // this is not the password used in the message integrity, this is if the request provided a password in the clear (ala rfc 3478).  Not recommended, but auth providers can use it if they want.
    bool fMessageIntegrityPresent;           // true if there was a message integrity field
};


enum AuthCredentialMechanism
{
    AuthCredShortTerm,
    AuthCredLongTerm
};

enum AuthResponseType
{
    Allow,             // just send back a response without any additional attributes or integrity
    AllowConditional,  // send back a response if the integrity matches with szPassword, otherwise respond back with a 401 and a nonce/realm
    StaleNonce,        // send back 438/Stale Nonce and use the new realm/nonce provided
    Reject,            // send back a 400/Bad Request with no additional attributes
    Unauthorized       // send back a 401 with realm/nonce provided
};

struct AuthResponse
{
    AuthResponseType responseType;  // how the server should treat the response
    AuthCredentialMechanism authCredMech; 
    
    char szPassword[MAX_STUN_AUTH_STRING_SIZE+1]; // ignored if _responseType is anything other than AllowConditional
    char szRealm[MAX_STUN_AUTH_STRING_SIZE+1]; // realm attribute for challenge-response. Ignored if _authCredMech is not AuthCredLongTerm
    char szNonce[MAX_STUN_AUTH_STRING_SIZE+1]; // nonce attribute for challenge-response. Ignored if _authCredMech is not AuthCredLongTerm
};


class IStunAuth : public IRefCounted
{
public:
    virtual HRESULT DoAuthCheck(AuthAttributes* pAuthAttributes, AuthResponse* pResponse) = 0;
};


#endif
