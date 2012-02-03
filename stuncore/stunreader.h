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



#ifndef STUN_MESSAGE_READER_H
#define STUN_MESSAGE_READER_H



#include "stuntypes.h"
#include "datastream.h"
#include "socketaddress.h"
#include "fasthash.h"


class CStunMessageReader
{
public:
    enum ReaderParseState
    {
        HeaderNotRead,
        HeaderValidated,
        BodyValidated,
        ParseError
    };

private:
    CDataStream _stream;
    
    bool _fAllowLegacyFormat; // if true, allows for messages of type RFC 3489 (no magic cookie) to be accepted
    bool _fMessageIsLegacyFormat; // set by readheader - true if the stun_magic_cookie is missing, but the message appears to be intact otherwise

    ReaderParseState _state;

    static const size_t MAX_NUM_ATTRIBUTES = 30;
    
    typedef FastHash<uint16_t, StunAttribute, MAX_NUM_ATTRIBUTES, 53> AttributeHashTable; // 53 is a prime number for a reasonable table width
    
    AttributeHashTable _mapAttributes;
    
    // special index values for message integrity attribute validation
    int _indexFingerprint;
    int _indexMessageIntegrity;
    int _countAttributes;
    

    StunTransactionId _transactionid;
    uint16_t _msgTypeNormalized;
    StunMessageClass _msgClass;
    uint16_t _msgLength;

    HRESULT ReadHeader();
    HRESULT ReadBody();

    HRESULT GetAddressHelper(uint16_t attribType, CSocketAddress* pAddr);
    
    HRESULT ValidateMessageIntegrity(uint8_t* key, size_t keylength);
    
public:
    CStunMessageReader();
    
    void Reset();
    
    void SetAllowLegacyFormat(bool fAllowLegacyFormat);
    
    ReaderParseState AddBytes(const uint8_t* pData, uint32_t size);
    uint16_t HowManyBytesNeeded();
    ReaderParseState GetState();

    bool IsMessageLegacyFormat();
    
    bool HasFingerprintAttribute();
    bool IsFingerprintAttributeValid();
    
    bool HasMessageIntegrityAttribute();
    HRESULT ValidateMessageIntegrityShort(const char* pszPassword);
    HRESULT ValidateMessageIntegrityLong(const char* pszUser, const char* pszRealm, const char* pszPassword);
    
    HRESULT GetAttributeByType(uint16_t attributeType, StunAttribute* pAttribute);
    HRESULT GetAttributeByIndex(int index, StunAttribute* pAttribute);
    int GetAttributeCount();

    void GetTransactionId(StunTransactionId* pTransId );
    StunMessageClass GetMessageClass();
    uint16_t GetMessageType();

    HRESULT GetResponsePort(uint16_t *pPort);
    HRESULT GetChangeRequest(StunChangeRequestAttribute* pChangeRequest);
    HRESULT GetPaddingAttributeSize(uint16_t* pSizePadding);
    HRESULT GetErrorCode(uint16_t* pErrorNumber);

    HRESULT GetBuffer(CRefCountedBuffer* pRefBuffer);
    
    
    HRESULT GetXorMappedAddress(CSocketAddress* pAddress);
    HRESULT GetMappedAddress(CSocketAddress* pAddress);
    HRESULT GetOtherAddress(CSocketAddress* pAddress);
    HRESULT GetResponseOriginAddress(CSocketAddress* pAddress);
    
    HRESULT GetStringAttributeByType(uint16_t attributeType, char* pszValue, /*in-out*/ size_t size);
    

    CDataStream& GetStream();

};

#endif
