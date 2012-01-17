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

#ifndef SAMPLE_STUN_AUTH_PROVIDER
#define SAMPLE_STUN_AUTH_PROVIDER


#if 0

sampleauthprovider.h and sampleauthprovider.cpp

The stun library code knows how to generate and validate message integrity attributes
inside stun attributes.  But it relies on an implemented "auth provider" to actually
confirm a username and supply the password key.
        
It is out of the scope of this release to implement an authentication provider that
will work for everyone. One deployment may store usernames and passwords in a database.
Other deployments may rely on a ticketing service library.
 
RFC 5389 specifies two authentication mechanisms for STUN.  It is recommended
that you read it before deciding how timplement
 
"Short term credentials" is simply the case where a stun binding request
sent from client to server contains a message integrity attribute.  The message
integrity attribute is simply an attribute that contins an HMAC-SHA1 hash of
the stun msesage itself. The stun message should also contain a username attribute.
The 'key' field of the HMAC operation is a private password associated with the
username in the binding request.
 
"Long term credentials" is similar to short term credentials but involves a challenge
response phase to prevent replay attacks. This is the recommended mechanism for
STUN authentication.
 
We can also support "ticket based credentials". This is not a mechanism specified
by the STUN RFC, but has been known to work in various scenarios.  The username
and/or realm fields are overloaded to represent a "ticket" signed by an external
entity.  The auth provider knows how to validate the ticket.
 
We can also implement "legacy password" authentication.  This is simply the password
(with or without a username and realm) embedded in the clear in the stun binding
request. Not recommended unless the transport type is TLS.
        
Implementing authentication is simply implementing a class that implements
IStunAuth.  IStunAuth has only one method called "DoAuthCheck".  DoAuthCheck
is called for each incoming Stun Message.  It takes one input parameter pointer
to a struct instance of type AuthAttributes) and one "out" param which is a struct
for handing back authentication tokens back to the server.
    
  HRESULT DoAuthCheck(/*in*/ AuthAttributes* pAuthAttributes, /*out*/ AuthResponse* pResponse);
 
The AuthAttributes struct representes various attribute values received in a STUN
binding request.  They are outlined as follows:

    char szUser[MAX_STUN_AUTH_STRING_SIZE];  // the user name attribute in the request (if available)
    char szRealm[MAX_STUN_AUTH_STRING_SIZE]; // the realm attribute in the request (if available)
    char szNonce[MAX_STUN_AUTH_STRING_SIZE]; // the nonce attribute in the request (if available)
    char szLegacyPassword[MAX_STUN_AUTH_STRING_SIZE]; // this is not the password used in the message integrity, this is if the request provided a password in the clear (ala rfc 3478).  Not recommended, but auth providers can use it if they want.
    bool fMessageIntegrityPresent;  // true if there was a message integrity field

The implementation of DoAuthCheck needs to decide how to handle the AuthAttributes
and then pass back a series of results and codes through the provided AuthResponse
paramter.

The AuthResponse parameter is for indicating to the server how to authenticate a
message integrity attribute.  The implementation needs to set the following fieds 
as appropriate:

    AuthResponseType responseType;
       responseType must be set to one of the following values
         // Allow - Indicates to the server to send back a response and that no 
                    additional validation on the message integrity field is needed
                           
         // AllowConditional - Indicates to the server that a response can be sent
                    as lone as the message integrity attribute in the stun request
                    is valid with respect to szPassword (and szUserName and szNonce if in long term cred mode)
                    If the message integrity attribute is deemed invalid, then
                    a 401 is sent back instead of a binding response.

         // Reject - Indicates that the server should send back a 400 without any
                     additional attributes
                           
         // Unauthorized - Indicates that the server should send back a 401. In long term
                     cred mode, this will also send back the szNonce and szRealm fields
                     as attributes.
                           
         // StaleNonce - Indicates that the request was likely valid, but the nonce
                         attribute valid has expired

    AuthCredentialMechanism authCredMech; 
       Is either set to AuthShortTerm or AuthLongTerm to indicate to the server
       how to generate and validate message integrity fields

    szPassword
       Ignored if _responseType is anything other than AllowConditional.
       server will not send this back as an attribute.  Instead it is used
       for validating and generating message integrity attributes in the
       stun messages.
           
    szRealm
       Ignored if using short term credentials. Otherwise, it should be
       the realm field used for generating and validating message integrity fields.
       It will almost always need to be sent back in error responses to
       the client.
            
    szNonce
       A new nonce for subsequent requests in the event this request can not
 

    DoAuthCheck should return S_OK unless a fatal error occurs.  If DoAuthCheck returns
    a failure code, then 
               

    To have the server host an instance of an IStunAuth implementation, modify
    CStunServer::Initialize and CTCPServer::Initialize to create an instance of your class and initialize
    _spAuth as appropriate.
               
#endif
         


class CShortTermAuth :
    public CBasicRefCount,
    public CObjectFactory<CShortTermAuth>,
    public IStunAuth
{
public:
    virtual HRESULT DoAuthCheck(AuthAttributes* pAuthAttributes, AuthResponse* pResponse);
    ADDREF_AND_RELEASE_IMPL();
};


class CLongTermAuth :
    public CBasicRefCount,
    public CObjectFactory<CLongTermAuth>,
    public IStunAuth
{
private:
    void HmacToString(uint8_t* hmacvalue, char* pszResult);
    HRESULT CreateNonce(char* pszNonce);
    HRESULT ValidateNonce(char* pszNonce);
    
public:
    
    virtual HRESULT DoAuthCheck(AuthAttributes* pAuthAttributes, AuthResponse* pResponse);
    ADDREF_AND_RELEASE_IMPL();
};


#endif

