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

#include "stunresponder.h"
#include "stunauth.h"

struct StunMessageEnvelope
{
    SocketRole localSocket;      /// which socket id did the message arrive on
    CSocketAddress localAddr;    /// What local IP address the message was received on (useful if the socket binded to INADDR_ANY)
    CSocketAddress remoteAddr;   /// the address of the node that sent us the message
    CRefCountedBuffer spBuffer;  /// the data in the message
};

struct StunMessageIntegrity
{
    bool fSendWithIntegrity;
    
    bool fUseLongTerm;
    char szUser[MAX_STUN_AUTH_STRING_SIZE+1]; // used for computing the message-integrity value
    char szRealm[MAX_STUN_AUTH_STRING_SIZE+1]; // used for computing the message-integrity value
    char szPassword[MAX_STUN_AUTH_STRING_SIZE+1]; // used for computing the message-integrity value
};

class CStunThreadMessageHandler
{
private:
    
    
    CRefCountedPtr<IStunResponder> _spStunResponder;
    
    CRefCountedPtr<IStunAuth> _spAuth;
    
    CRefCountedBuffer _spReaderBuffer;
    CRefCountedBuffer _spResponseBuffer;

    StunMessageEnvelope _message;   // the message, including where it came from, who it was sent to, and the socket id
    CSocketAddress _addrResponse;   // where do we went the response back to go back to
    bool _fRequestHasResponsePort;  // true if the request has a response port attribute
    SocketRole _socketOutput;       // which socket do we send the response on?
    StunTransactionId _transid;
    StunMessageIntegrity _integrity;
    
    struct StunErrorCode
    {
        uint16_t errorcode;
        StunMessageClass msgclass;
        uint16_t msgtype;
        uint16_t attribUnknown; // for now, just send back one unknown attribute at a time
        char szNonce[MAX_STUN_AUTH_STRING_SIZE+1];
        char szRealm[MAX_STUN_AUTH_STRING_SIZE+1];
    };
    
    StunErrorCode _error;

    HRESULT ProcessBindingRequest(CStunMessageReader& reader);

    void SendErrorResponse();
    void SendResponse();
    
    HRESULT ValidateAuth(CStunMessageReader& reader);
    
    

public:
    CStunThreadMessageHandler();
    ~CStunThreadMessageHandler();

    void SetResponder(IStunResponder* pTransport);
    void SetAuth(IStunAuth* pAuth);
    void ProcessRequest(StunMessageEnvelope& message);
};


#endif /* MESSAGEHANDLER_H_ */
