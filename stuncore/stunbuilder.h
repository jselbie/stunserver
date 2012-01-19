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



#ifndef STUN_MESSAGE_H
#define STUN_MESSAGE_H


#include "datastream.h"
#include "socketaddress.h"
#include "stuntypes.h"



class CStunMessageBuilder
{
public:

private:
    CDataStream _stream;
    StunTransactionId _transactionid;
    bool _fLegacyMode;

    HRESULT AddMappedAddressImpl(uint16_t attribute, const CSocketAddress& addr);
    
    HRESULT AddMessageIntegrityImpl(uint8_t* key, size_t keysize);
    

public:
    CStunMessageBuilder();
    
    void SetLegacyMode(bool fLegacyMode);

    HRESULT AddHeader(StunMessageType msgType, StunMessageClass msgClass);
    HRESULT AddBindingRequestHeader();
    HRESULT AddBindingResponseHeader(bool fSuccess);
    
    HRESULT AddTransactionId(const StunTransactionId& transid);
    HRESULT AddRandomTransactionId(StunTransactionId* pTransId);

    HRESULT AddAttributeHeader(uint16_t attribType, uint16_t size);
    HRESULT AddAttribute(uint16_t attribType, const void* data, uint16_t size);
    HRESULT AddStringAttribute(uint16_t attribType, const char* pstr);

    HRESULT AddXorMappedAddress(const CSocketAddress& addr);
    HRESULT AddMappedAddress(const CSocketAddress& addr);
    HRESULT AddResponseOriginAddress(const CSocketAddress& other);
    HRESULT AddOtherAddress(const CSocketAddress& other);

    HRESULT AddResponsePort(uint16_t port);
    HRESULT AddPaddingAttribute(uint16_t paddingSize);

    HRESULT AddChangeRequest(const StunChangeRequestAttribute& changeAttrib);
    HRESULT AddErrorCode(uint16_t errorNumber, const char* pszReason);
    HRESULT AddUnknownAttributes(const uint16_t* arrAttributeIds, size_t count);

    HRESULT AddFingerprintAttribute();
    
    HRESULT AddUserName(const char* pszUserName);
    HRESULT AddRealm(const char* pszRealm);
    HRESULT AddNonce(const char* pszNonce);
    
    
    HRESULT AddMessageIntegrityShortTerm(const char* pszPassword);
    HRESULT AddMessageIntegrityLongTerm(const char* pszUserName, const char* pszRealm, const char* pszPassword);

    HRESULT FixLengthField();

    HRESULT GetResult(CRefCountedBuffer* pspBuffer);

    CDataStream& GetStream();
};

#endif
