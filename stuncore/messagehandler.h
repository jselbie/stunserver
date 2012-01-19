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


#ifndef MESSAGEHANDLER_H_
#define MESSAGEHANDLER_H_

#include "stunauth.h"
#include "socketrole.h"

struct StunMessageIn
{
    SocketRole socketrole;         /// which socket id did the message arrive on
    CSocketAddress addrLocal;    /// What local IP address the message was received on (useful if the socket binded to INADDR_ANY)
    CSocketAddress addrRemote;   /// the address of the node that sent us the message
    CStunMessageReader* pReader;   /// reader containing a valid stun message
    bool fConnectionOriented;     // true for TCP or TLS (where we can't send back to a different port)
};

struct StunMessageOut
{
    SocketRole socketrole;         // which socket to send out to (ignored for TCP)
    CSocketAddress addrDest;       // where to send the response to (ignored for TCP)
    CRefCountedBuffer spBufferOut; // allocated by the caller - output message
};


struct StunMessageIntegrity
{
    bool fSendWithIntegrity;
    
    bool fUseLongTerm;
    char szUser[MAX_STUN_AUTH_STRING_SIZE+1]; // used for computing the message-integrity value
    char szRealm[MAX_STUN_AUTH_STRING_SIZE+1]; // used for computing the message-integrity value
    char szPassword[MAX_STUN_AUTH_STRING_SIZE+1]; // used for computing the message-integrity value
};

struct TransportAddress
{
    CSocketAddress addr;
    bool fValid; // set to false if not valid (basic mode and most TCP/SSL scenarios)
};

struct TransportAddressSet
{
    TransportAddress set[4]; // one for each socket role RolePP, RolePA, RoleAP, and RoleAA
};

struct StunErrorCode
{
    uint16_t errorcode;
    StunMessageClass msgclass;
    uint16_t msgtype;
    uint16_t attribUnknown; // for now, just send back one unknown attribute at a time
    char szNonce[MAX_STUN_AUTH_STRING_SIZE+1];
    char szRealm[MAX_STUN_AUTH_STRING_SIZE+1];
};




class CStunRequestHandler
{
public:
    static HRESULT ProcessRequest(const StunMessageIn& msgIn, StunMessageOut& msgOut, TransportAddressSet* pAddressSet, /*optional*/ IStunAuth* pAuth);
private:
    
    CStunRequestHandler();

    HRESULT ProcessBindingRequest();
    void BuildErrorResponse();
    HRESULT ValidateAuth();
    HRESULT ProcessRequestImpl();
    
    // input
    IStunAuth* _pAuth;
    TransportAddressSet* _pAddrSet;
    const StunMessageIn* _pMsgIn;
    StunMessageOut* _pMsgOut;
    
    // member variables to remember along the way
    StunMessageIntegrity _integrity;
    StunErrorCode _error;
    
    bool _fRequestHasResponsePort;
    StunTransactionId _transid;
    bool _fLegacyMode;
    
    bool HasAddress(SocketRole role);
    bool IsIPAddressZeroOrInvalid(SocketRole role);
};


#endif /* MESSAGEHANDLER_H_ */
