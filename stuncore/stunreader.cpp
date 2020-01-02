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

#include "stunreader.h"
#include "stunutils.h"
#include "socketaddress.h"
#include <boost/crc.hpp>

#ifndef __APPLE__
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#else
#define COMMON_DIGEST_FOR_OPENSSL
#include <CommonCrypto/CommonCrypto.h>
#endif

#include "stunauth.h"
#include "fasthash.h"





CStunMessageReader::CStunMessageReader()
{
    Reset();
}

void CStunMessageReader::Reset()
{
    _fAllowLegacyFormat = true;
    _fMessageIsLegacyFormat = false;
    _state = HeaderNotRead;
    _mapAttributes.Reset();
    
    _indexFingerprint = -1;
    _indexMessageIntegrity = -1;
    _countAttributes = 0;
    
    memset(&_transactionid, '\0', sizeof(_transactionid));
    _msgTypeNormalized = 0xffff;
    _msgClass = StunMsgClassInvalidMessageClass;
    _msgLength = 0;
    _stream.Reset();
}


void CStunMessageReader::SetAllowLegacyFormat(bool fAllowLegacyFormat)
{
    _fAllowLegacyFormat = fAllowLegacyFormat;
}

bool CStunMessageReader::IsMessageLegacyFormat()
{
    return _fMessageIsLegacyFormat;
}

uint16_t CStunMessageReader::HowManyBytesNeeded()
{
    size_t currentSize = _stream.GetSize();
    switch (_state)
    {
    case HeaderNotRead:
        BOOST_ASSERT(STUN_HEADER_SIZE > currentSize);
        return STUN_HEADER_SIZE - currentSize;
    case HeaderValidated:
        BOOST_ASSERT((_msgLength+STUN_HEADER_SIZE) > currentSize);
        return (_msgLength+STUN_HEADER_SIZE) - currentSize;
    default:
    	return 0;
    }
    return 0;
}

bool CStunMessageReader::HasFingerprintAttribute()
{
    StunAttribute *pAttrib = _mapAttributes.Lookup(STUN_ATTRIBUTE_FINGERPRINT);
    return (pAttrib != NULL);
}

bool CStunMessageReader::IsFingerprintAttributeValid()
{
    HRESULT hr = S_OK;
    StunAttribute* pAttrib = _mapAttributes.Lookup(STUN_ATTRIBUTE_FINGERPRINT);
    CRefCountedBuffer spBuffer;
    size_t size=0;
    boost::crc_32_type crc;
    uint32_t computedValue=1;
    uint32_t readValue=0;
    uint8_t* ptr = NULL;


    // the fingerprint attribute MUST be the last attribute in the stream.
    // If it's not, then the code below will return false

    ChkIf(pAttrib==NULL, E_FAIL);
    
    ChkIfA(pAttrib->attributeType != STUN_ATTRIBUTE_FINGERPRINT, E_FAIL);

    ChkIf(pAttrib->size != 4, E_FAIL);
    ChkIf(_state != BodyValidated, E_FAIL);
    Chk(_stream.GetBuffer(&spBuffer));

    size = _stream.GetSize();
    ChkIf(size < STUN_HEADER_SIZE, E_FAIL);

    ptr = spBuffer->GetData();
    ChkIfA(ptr==NULL, E_FAIL);

    crc.process_bytes(ptr, size-8); // -8 because we're assuming the fingerprint attribute is 8 bytes and is the last attribute in the stream
    computedValue = crc.checksum();
    computedValue = computedValue ^ STUN_FINGERPRINT_XOR;

    readValue = *(uint32_t*)(ptr+pAttrib->offset);
    readValue = ntohl(readValue);
    hr = (readValue==computedValue) ? S_OK : E_FAIL;

Cleanup:
    return (SUCCEEDED(hr));
}

bool CStunMessageReader::HasMessageIntegrityAttribute()
{
    return (NULL != _mapAttributes.Lookup(STUN_ATTRIBUTE_MESSAGEINTEGRITY));
}

HRESULT CStunMessageReader::ValidateMessageIntegrity(uint8_t* key, size_t keylength)
{
    HRESULT hr = S_OK;
    
    int lastAttributeIndex = _countAttributes - 1;
    bool fFingerprintAdjustment = false;
    bool fNoOtherAttributesAfterIntegrity = false;
    const size_t c_hmacsize = 20;
    uint8_t hmaccomputed[c_hmacsize] = {}; // zero-init
    unsigned int hmaclength = c_hmacsize;
#ifndef __APPLE__
    HMAC_CTX* ctx = NULL;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    HMAC_CTX ctxData = {};
    ctx = &ctxData;
    HMAC_CTX_init(ctx);
#else
    ctx = HMAC_CTX_new();
#endif
#else
    CCHmacContext* ctx = NULL;
    CCHmacContext ctxData = {};
    ctx = &ctxData;
    
    UNREFERENCED_VARIABLE(hmaclength);
#endif
    uint32_t chunk32;
    uint16_t chunk16;
    size_t len, nChunks;
    CDataStream stream;
    CRefCountedBuffer spBuffer;
    StunAttribute* pAttribIntegrity=NULL;
    
    int cmp = 0;
    bool fContextInit = false;
    
    
    ChkIf(_state != BodyValidated, E_FAIL);
    
    ChkIf(_countAttributes == 0, E_FAIL); // if there's not attributes, there's definitely not a message integrity attribute
    ChkIf(_indexMessageIntegrity == -1, E_FAIL);
    
    // can a key be empty?
    ChkIfA(key==NULL, E_INVALIDARG);
    ChkIfA(keylength==0, E_INVALIDARG);
    
    pAttribIntegrity = _mapAttributes.Lookup(::STUN_ATTRIBUTE_MESSAGEINTEGRITY);
    
    ChkIf(pAttribIntegrity == NULL, E_FAIL);

    ChkIf(pAttribIntegrity->size != c_hmacsize, E_FAIL);
    
    // first, check to make sure that no other attributes (other than fingerprint) follow the message integrity
    fNoOtherAttributesAfterIntegrity = (_indexMessageIntegrity == lastAttributeIndex) || ((_indexMessageIntegrity == (lastAttributeIndex-1)) && (_indexFingerprint == lastAttributeIndex));
    ChkIf(fNoOtherAttributesAfterIntegrity==false, E_FAIL);
    
    fFingerprintAdjustment = (_indexMessageIntegrity == (lastAttributeIndex-1));

    Chk(GetBuffer(&spBuffer));
    stream.Attach(spBuffer, false);
    
    // Here comes the fun part.  If there is a fingerprint attribute, we have to adjust the length header in computing the hash
#ifndef __APPLE__
#if OPENSSL_VERSION_NUMBER < 0x10100000L // could be lower!
    HMAC_Init(ctx, key, keylength, EVP_sha1());
#else
    HMAC_Init_ex(ctx, key, keylength, EVP_sha1(), NULL);
#endif
#else
    CCHmacInit(ctx, kCCHmacAlgSHA1, key, keylength);
#endif
    fContextInit = true;
    
    // message type
    Chk(stream.ReadUint16(&chunk16));
#ifndef __APPLE__
    HMAC_Update(ctx, (unsigned char*)&chunk16, sizeof(chunk16));
#else
    CCHmacUpdate(ctx, &chunk16, sizeof(chunk16));
#endif
    
    // message length
    Chk(stream.ReadUint16(&chunk16));
    if (fFingerprintAdjustment)
    {
        // subtract the length of the fingerprint off the length header
        // fingerprint attribute is 8 bytes long including it's own header
        // and to do this, we have to fix the network byte ordering issue
        uint16_t lengthHeader = ntohs(chunk16);
        uint16_t adjustedlengthHeader = lengthHeader - 8;
        
        
        chunk16 = htons(adjustedlengthHeader);
    }
    
#ifndef __APPLE__
    HMAC_Update(ctx, (unsigned char*)&chunk16, sizeof(chunk16));
#else
    CCHmacUpdate(ctx, &chunk16, sizeof(chunk16));
#endif
    
    // now include everything up to the hash attribute itself.
    len = pAttribIntegrity->offset;
    len -= 4; // subtract the size of the attribute header
    len -= 4; // subtract the size of the message header (not including the transaction id)
    
    // len should be the number of bytes from the start of the transaction ID up through to the start of the integrity attribute header
    // the stun message has to be a multiple of 4 bytes, so we can read in 32 bit chunks
    nChunks = len / 4;
    ASSERT((len % 4) == 0);
    for (size_t count = 0; count < nChunks; count++)
    {
        Chk(stream.ReadUint32(&chunk32));
#ifndef __APPLE__
        HMAC_Update(ctx, (unsigned char*)&chunk32, sizeof(chunk32));
#else
        CCHmacUpdate(ctx, &chunk32, sizeof(chunk32));
#endif
    }
    
#ifndef __APPLE__
    HMAC_Final(ctx, hmaccomputed, &hmaclength);
#else
    CCHmacFinal(ctx, hmaccomputed);
#endif
    
    
    // now compare the bytes
    cmp = memcmp(hmaccomputed, spBuffer->GetData() + pAttribIntegrity->offset, c_hmacsize);
    
    hr = (cmp == 0 ? S_OK : E_FAIL);
    
Cleanup:
    if (fContextInit)
    {
#ifndef __APPLE__
#if OPENSSL_VERSION_NUMBER < 0x10100000L
        HMAC_CTX_cleanup(ctx);
#else
        HMAC_CTX_free(ctx);
#endif
#else
        UNREFERENCED_VARIABLE(fContextInit);
#endif
    }
        
    return hr;
}

HRESULT CStunMessageReader::ValidateMessageIntegrityShort(const char* pszPassword)
{
    return ValidateMessageIntegrity((uint8_t*)pszPassword, strlen(pszPassword));
}

HRESULT CStunMessageReader::ValidateMessageIntegrityLong(const char* pszUser, const char* pszRealm, const char* pszPassword)
{
    HRESULT hr = S_OK;
    const size_t MAX_KEY_SIZE = MAX_STUN_AUTH_STRING_SIZE*3 + 2;
    uint8_t key[MAX_KEY_SIZE + 1]; // long enough for 64-char strings and two semicolons and a null char
    uint8_t* pData = NULL;
    uint8_t* pDst = key;
    size_t totallength = 0;
    
    size_t passwordlength = pszPassword ? strlen(pszPassword) : 0;
    size_t userLength = pszUser ? strlen(pszUser)  : 0;
    size_t realmLength = pszRealm ? strlen(pszRealm) : 0;
    
    uint8_t hash[MD5_DIGEST_LENGTH] = {};
    
    ChkIf(_state != BodyValidated, E_FAIL);
   
    totallength = userLength + realmLength + passwordlength + 2; // +2 for two semi-colons
    
    pData = GetStream().GetDataPointerUnsafe();
    ChkIfA(pData==NULL, E_FAIL);
    
    if (userLength > 0)
    {
        memcpy(pDst, pszUser, userLength);
        pDst += userLength;
    }
    *pDst = ':';
    pDst++;
    
    
    if (realmLength > 0)
    {
        memcpy(pDst, pszRealm, realmLength);
        pDst += realmLength;
    }
    *pDst = ':';
    pDst++;

    if (passwordlength > 0)
    {
        memcpy(pDst, pszPassword, passwordlength);
        pDst += passwordlength;
    }
    *pDst = '0'; // null terminate for debugging (does not get hashed)
    
    ASSERT(key+totallength == pDst);
    
#ifndef __APPLE__
    ChkIfA(NULL == MD5(key, totallength, hash), E_FAIL);
#else
        CC_MD5(key, totallength, hash);
#endif
    
    
    
    Chk(ValidateMessageIntegrity(hash, ARRAYSIZE(hash)));
    
Cleanup:
    return hr;    
}



HRESULT CStunMessageReader::GetAttributeByIndex(int index, StunAttribute* pAttribute)
{
    StunAttribute* pFound = _mapAttributes.LookupValueByIndex((size_t)index);
    
    if (pFound == NULL)
    {
        return E_FAIL;
    }
    
    if (pAttribute)
    {
        *pAttribute = *pFound;
    }
    return S_OK;
}

HRESULT CStunMessageReader::GetAttributeByType(uint16_t attributeType, StunAttribute* pAttribute)
{
    StunAttribute* pFound = _mapAttributes.Lookup(attributeType);
        
    if (pFound == NULL)
    {
        return E_FAIL;
    }
    
    if (pAttribute)
    {
        *pAttribute = *pFound;
    }
    return S_OK;
}


int CStunMessageReader::GetAttributeCount()
{
    return (int)(this->_mapAttributes.Size());
}

HRESULT CStunMessageReader::GetResponsePort(uint16_t* pPort)
{
    StunAttribute* pAttrib = NULL;
    HRESULT hr = S_OK;
    uint16_t portNBO;
    uint8_t *pData = NULL;

    ChkIfA(pPort == NULL, E_INVALIDARG);

    pAttrib = _mapAttributes.Lookup(STUN_ATTRIBUTE_RESPONSE_PORT);
    ChkIf(pAttrib == NULL, E_FAIL);
    
    ChkIf(pAttrib->size != STUN_ATTRIBUTE_RESPONSE_PORT_SIZE, E_UNEXPECTED);

    pData = _stream.GetDataPointerUnsafe();
    ChkIf(pData==NULL, E_UNEXPECTED);

    memcpy(&portNBO, pData + pAttrib->offset, STUN_ATTRIBUTE_RESPONSE_PORT_SIZE);
    *pPort = ntohs(portNBO);
Cleanup:
    return hr;
}

HRESULT CStunMessageReader::GetChangeRequest(StunChangeRequestAttribute* pChangeRequest)
{
    HRESULT hr = S_OK;
    uint8_t *pData = NULL;
    StunAttribute *pAttrib;
    uint32_t value = 0;

    ChkIfA(pChangeRequest == NULL, E_INVALIDARG);
    
    pAttrib = _mapAttributes.Lookup(STUN_ATTRIBUTE_CHANGEREQUEST);
    ChkIf(pAttrib == NULL, E_FAIL);

    ChkIf(pAttrib->size != STUN_ATTRIBUTE_CHANGEREQUEST_SIZE, E_UNEXPECTED);

    pData = _stream.GetDataPointerUnsafe();
    ChkIf(pData==NULL, E_UNEXPECTED);

    memcpy(&value, pData + pAttrib->offset, STUN_ATTRIBUTE_CHANGEREQUEST_SIZE);

    value = ntohl(value);

    pChangeRequest->fChangeIP = !!(value & 0x0004);
    pChangeRequest->fChangePort = !!(value & 0x0002);

Cleanup:
    if (FAILED(hr) && pChangeRequest)
    {
        pChangeRequest->fChangeIP = false;
        pChangeRequest->fChangePort = false;
    }

    return hr;
}


HRESULT CStunMessageReader::GetPaddingAttributeSize(uint16_t* pSizePadding)
{
    HRESULT hr = S_OK;
    StunAttribute *pAttrib;

    ChkIfA(pSizePadding == NULL, E_INVALIDARG);

    *pSizePadding = 0;
    
    pAttrib = _mapAttributes.Lookup(STUN_ATTRIBUTE_PADDING);

    ChkIf(pAttrib == NULL, E_FAIL);

    *pSizePadding = pAttrib->size;

Cleanup:
    return hr;
}

HRESULT CStunMessageReader::GetErrorCode(uint16_t* pErrorNumber)
{
    HRESULT hr = S_OK;
    uint8_t* ptr = NULL;
    uint8_t cl = 0;
    uint8_t num = 0;

    StunAttribute* pAttrib;

    ChkIf(pErrorNumber==NULL, E_INVALIDARG);

    pAttrib = _mapAttributes.Lookup(STUN_ATTRIBUTE_ERRORCODE);
    ChkIf(pAttrib == NULL, E_FAIL);

    // first 21 bits of error-code attribute must be zero.
    // followed by 3 bits of "class" and 8 bits for the error number modulo 100
    ptr = _stream.GetDataPointerUnsafe() + pAttrib->offset + 2;

    cl = *ptr++;
    cl = cl & 0x07;
    num = *ptr;
    *pErrorNumber = cl * 100 + num;

Cleanup:
    return hr;
}


HRESULT CStunMessageReader::GetAddressHelper(uint16_t attribType, CSocketAddress* pAddr)
{
    HRESULT hr = S_OK;
    StunAttribute* pAttrib = _mapAttributes.Lookup(attribType);
    uint8_t *pAddrStart = NULL;

    ChkIf(pAttrib == NULL, E_FAIL);
    
    pAddrStart = _stream.GetDataPointerUnsafe() + pAttrib->offset;
    Chk(::GetMappedAddress(pAddrStart, pAttrib->size, pAddr));

Cleanup:
    return hr;

}

HRESULT CStunMessageReader::GetMappedAddress(CSocketAddress* pAddr)
{
    HRESULT hr = S_OK;
    Chk(GetAddressHelper(STUN_ATTRIBUTE_MAPPEDADDRESS, pAddr));
Cleanup:
    return hr;
}

HRESULT CStunMessageReader::GetOtherAddress(CSocketAddress* pAddr)
{
    HRESULT hr = S_OK;
    
    hr = GetAddressHelper(STUN_ATTRIBUTE_OTHER_ADDRESS, pAddr);
    
    if (FAILED(hr))
    {
        // look for the legacy changed address attribute that a legacy (RFC 3489) server would send
        hr = GetAddressHelper(STUN_ATTRIBUTE_CHANGEDADDRESS, pAddr);
    }
    
    return hr;
}


HRESULT CStunMessageReader::GetXorMappedAddress(CSocketAddress* pAddr)
{
    HRESULT hr = S_OK;
    hr = GetAddressHelper(STUN_ATTRIBUTE_XORMAPPEDADDRESS, pAddr);
    
    if (FAILED(hr))
    {
        // this is the vovida compat address attribute
        hr = GetAddressHelper(STUN_ATTRIBUTE_XORMAPPEDADDRESS_OPTIONAL, pAddr);
    }
    
    if (SUCCEEDED(hr))
    {
        pAddr->ApplyStunXorMap(_transactionid);
    }

    return hr;
}

HRESULT CStunMessageReader::GetResponseOriginAddress(CSocketAddress* pAddr)
{
    HRESULT hr = S_OK;
    
    hr = GetAddressHelper(STUN_ATTRIBUTE_RESPONSE_ORIGIN, pAddr);
    
    if (FAILED(hr))
    {
        // look for the legacy address attribute that a legacy (RFC 3489) server would send
        hr = GetAddressHelper(STUN_ATTRIBUTE_SOURCEADDRESS, pAddr);
    }
    
    return hr;
    
}


HRESULT CStunMessageReader::GetStringAttributeByType(uint16_t attributeType, char* pszValue, /*in-out*/ size_t size)
{
    HRESULT hr = S_OK;
    StunAttribute* pAttrib = _mapAttributes.Lookup(attributeType);
    
    ChkIfA(pszValue == NULL, E_INVALIDARG);
    ChkIf(pAttrib == NULL, E_INVALIDARG);
    
    // size needs to be 1 greater than attrib.size so we can properly copy over a null char at the end
    ChkIf(pAttrib->size >= size, E_INVALIDARG);
    
    memcpy(pszValue, _stream.GetDataPointerUnsafe() + pAttrib->offset, pAttrib->size);
    pszValue[pAttrib->size] = '\0';
    
Cleanup:
    return hr;
}




HRESULT CStunMessageReader::ReadHeader()
{

    HRESULT hr = S_OK;
    bool fHeaderValid = false;
    uint16_t msgType;
    uint16_t msgLength;
    uint32_t cookie;
    StunTransactionId transID;

    Chk(_stream.SeekDirect(0));
    Chk(_stream.ReadUint16(&msgType));
    Chk(_stream.ReadUint16(&msgLength));
    Chk(_stream.Read(&transID.id, sizeof(transID.id)));

    // convert from big endian to native type
    msgType = ntohs(msgType);
    msgLength = ntohs(msgLength);

    memcpy(&cookie, transID.id, 4);
    cookie = ntohl(cookie);

    _fMessageIsLegacyFormat = !(cookie == STUN_COOKIE);

    fHeaderValid = ( (0==(msgType & 0xc000)) && ((msgLength%4)==0) );

    // if we aren't in legacy format (the default), then the cookie field of the transaction id must be the STUN_COOKIE
    fHeaderValid = (fHeaderValid && (_fAllowLegacyFormat || !_fMessageIsLegacyFormat));

    ChkIf(fHeaderValid == false, E_FAIL);

    _msgTypeNormalized = (  (msgType & 0x000f) | ((msgType & 0x00e0)>>1) | ((msgType & 0x3E00)>>2)  );
    _msgLength = msgLength;

    _transactionid = transID;

    ChkIf(_msgLength > MAX_STUN_MESSAGE_SIZE, E_UNEXPECTED);


    if (STUN_IS_REQUEST(msgType))
    {
        _msgClass = StunMsgClassRequest;
    }
    else if (STUN_IS_INDICATION(msgType))
    {
        _msgClass = StunMsgClassIndication;
    }
    else if (STUN_IS_SUCCESS_RESP(msgType))
    {
        _msgClass = StunMsgClassSuccessResponse;
    }
    else if (STUN_IS_ERR_RESP(msgType))
    {
        _msgClass = StunMsgClassFailureResponse;
    }
    else
    {
        // couldn't possibly happen, because msgClass is only two bits
        _msgClass = StunMsgClassInvalidMessageClass;
        ChkA(E_FAIL);
    }



Cleanup:
    return hr;
}

HRESULT CStunMessageReader::ReadBody()
{
    size_t currentSize = _stream.GetSize();
    size_t bytesConsumed = STUN_HEADER_SIZE;
    HRESULT hr = S_OK;

    Chk(_stream.SeekDirect(STUN_HEADER_SIZE));

    while (SUCCEEDED(hr) && (bytesConsumed < currentSize))
    {
        uint16_t attributeType;
        uint16_t attributeLength;
        uint16_t attributeOffset;
        int paddingLength=0;

        hr = _stream.ReadUint16(&attributeType);

        if (SUCCEEDED(hr))
        {
            hr = _stream.ReadUint16(&attributeLength);
        }

        if (SUCCEEDED(hr))
        {
            attributeOffset = _stream.GetPos();
            attributeType = ntohs(attributeType);
            attributeLength = ntohs(attributeLength);

            // todo - if an attribute has no size, it's length is not padded by 4 bytes, right?
            if (attributeLength % 4)
            {
                paddingLength = 4 - attributeLength % 4;
            }

            hr = (attributeLength <= MAX_STUN_ATTRIBUTE_SIZE) ? S_OK : E_FAIL;
        }

        if (SUCCEEDED(hr))
        {
            int result;
            StunAttribute attrib;
            attrib.attributeType = attributeType;
            attrib.size = attributeLength;
            attrib.offset = attributeOffset;

            // if we have already read in more attributes than MAX_NUM_ATTRIBUTES, then Insert call will fail (this is how we gate too many attributes)
            result = _mapAttributes.Insert(attributeType, attrib);
            hr = (result >= 0) ? S_OK : E_FAIL;
        }
        
        if (SUCCEEDED(hr))
        {
            
            if (attributeType == ::STUN_ATTRIBUTE_FINGERPRINT)
            {
                _indexFingerprint = _countAttributes;
            }
            
            if (attributeType == ::STUN_ATTRIBUTE_MESSAGEINTEGRITY)
            {
                _indexMessageIntegrity = _countAttributes;
            }
            
            _countAttributes++;
        }
        

        
        if (SUCCEEDED(hr))
        {
            hr = _stream.SeekRelative(attributeLength);
        }

        // consume the padding
        if (SUCCEEDED(hr))
        {
            if (paddingLength > 0)
            {
                hr = _stream.SeekRelative(paddingLength);
            }
        }

        if (SUCCEEDED(hr))
        {
            bytesConsumed += sizeof(attributeType) + sizeof(attributeLength) + attributeLength + paddingLength;
        }
    }

    // I don't think we could consume more bytes than stream size, but it's a worthy check to still keep here
    hr = (bytesConsumed == currentSize) ? S_OK : E_FAIL;

Cleanup:
    return hr;

}

CStunMessageReader::ReaderParseState CStunMessageReader::AddBytes(const uint8_t* pData, uint32_t size)
{
    HRESULT hr = S_OK;
    size_t currentSize;

    if (_state == ParseError)
    {
        return ParseError;
    }

    if (size == 0)
    {
        return _state;
    }
    
    // seek to the end of the stream
    _stream.SeekDirect(_stream.GetSize());

    if (FAILED(_stream.Write(pData, size)))
    {
        return ParseError;
    }

    currentSize = _stream.GetSize();

    if (_state == HeaderNotRead)
    {
        if (currentSize >= STUN_HEADER_SIZE)
        {
            hr = ReadHeader();

            _state = SUCCEEDED(hr) ? HeaderValidated : ParseError;

            if (SUCCEEDED(hr) && (_msgLength==0))
            {
                _state = BodyValidated;
            }
        }
    }

    if (_state == HeaderValidated)
    {
        if (currentSize >= (_msgLength+STUN_HEADER_SIZE))
        {
            if (currentSize == (_msgLength+STUN_HEADER_SIZE))
            {
                hr = ReadBody();
                _state = SUCCEEDED(hr) ? BodyValidated : ParseError;
            }
            else
            {
                // TOO MANY BYTES FED IN
                _state = ParseError;
            }
        }
    }

    if (_state == BodyValidated)
    {
        // What?  After validating the body, the caller still passes in way too many bytes?
        if (currentSize > (_msgLength+STUN_HEADER_SIZE))
        {
            _state = ParseError;
        }
    }

    return _state;

}

CStunMessageReader::ReaderParseState CStunMessageReader::GetState()
{
    return _state;
}

void CStunMessageReader::GetTransactionId(StunTransactionId* pTrans)
{
    if (pTrans)
    {
        *pTrans = _transactionid;
    }
}


StunMessageClass CStunMessageReader::GetMessageClass()
{
    return _msgClass;
}

uint16_t CStunMessageReader::GetMessageType()
{
    return _msgTypeNormalized;
}

HRESULT CStunMessageReader::GetBuffer(CRefCountedBuffer* pRefBuffer)
{
    HRESULT hr = S_OK;

    ChkIf(pRefBuffer == NULL, E_INVALIDARG);
    Chk(_stream.GetBuffer(pRefBuffer));

Cleanup:
    return hr;
}

CDataStream& CStunMessageReader::GetStream()
{
    return _stream;
}

