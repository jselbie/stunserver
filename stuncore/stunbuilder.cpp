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

#include "stringhelper.h"
#include "atomichelpers.h"

#include "stunbuilder.h"
#include <boost/crc.hpp>

#ifndef __APPLE__
#include <openssl/md5.h>
#include <openssl/hmac.h>
#else
#define COMMON_DIGEST_FOR_OPENSSL
#include <CommonCrypto/CommonCrypto.h>
#endif


#include "stunauth.h"


static int g_sequence_number = 0xaaaaaaaa;


CStunMessageBuilder::CStunMessageBuilder() :
_transactionid(),
_fLegacyMode(false)
{
    ;
}

void CStunMessageBuilder::SetLegacyMode(bool fLegacyMode)
{
    _fLegacyMode = fLegacyMode;
}


HRESULT CStunMessageBuilder::AddHeader(StunMessageType msgType, StunMessageClass msgClass)
{
    uint16_t msgTypeField=0;
    HRESULT hr = S_OK;

    ChkA(_stream.SetSizeHint(200));

    // merge the _msgType and _msgClass, and the leading zero bits into a 16-bit field
    msgTypeField =  (msgType & 0x0f80) << 2;
    msgTypeField |= (msgType & 0x0070) << 1;
    msgTypeField |= (msgType & 0x000f);
    msgTypeField |= (msgClass & 0x02) << 7;
    msgTypeField |= (msgClass & 0x01) << 4;

    ChkA(_stream.WriteUint16(htons(msgTypeField))); // htons for big-endian
    ChkA(_stream.WriteUint16(0)); // place holder for length

Cleanup:
    return hr;
}

HRESULT CStunMessageBuilder::AddBindingRequestHeader()
{
    return AddHeader(StunMsgTypeBinding, StunMsgClassRequest);
}

HRESULT CStunMessageBuilder::AddBindingResponseHeader(bool fSuccess)
{
    return AddHeader(StunMsgTypeBinding, fSuccess ? StunMsgClassSuccessResponse : StunMsgClassFailureResponse);
}

HRESULT CStunMessageBuilder::AddTransactionId(const StunTransactionId& transid)
{
    _transactionid = transid;

    return _stream.Write(transid.id, sizeof(transid.id));
}

HRESULT CStunMessageBuilder::AddRandomTransactionId(StunTransactionId* pTransId)
{
    StunTransactionId transid;
    uint32_t stun_cookie_nbo = htonl(STUN_COOKIE);

    uint32_t entropy=0;


    // on x86, the rdtsc instruction is about as good as it gets for a random sequence number
    // on linux, there's /dev/urandom


#ifdef _WIN32
    // on windows, there's lots of simple stuff we can get at to give us a random number
    // the rdtsc instruction is about as good as it gets
    uint64_t clock = __rdtsc();
    entropy ^= (uint32_t)(clock);
#else
    // on linux, /dev/urandom should be sufficient
    {
    	int randomfile = ::open("/dev/urandom", O_RDONLY);
    	if (randomfile >= 0)
    	{
    	    int readret = read(randomfile, &entropy, sizeof(entropy));
            UNREFERENCED_VARIABLE(readret);
    	    ASSERT(readret > 0);
    		close(randomfile);
    	}
    }


    if (entropy == 0)
    {
        entropy ^= getpid();
        entropy ^= reinterpret_cast<uintptr_t>(this);
        entropy ^= time(NULL);
        entropy ^= AtomicIncrement(&g_sequence_number);
    }

#endif


    srand(entropy);


    // the first four bytes of the transaction id is always the magic cookie
    // followed by 12 bytes of the real transaction id
    memcpy(transid.id, &stun_cookie_nbo, sizeof(stun_cookie_nbo));
    for (int x = 4; x < (STUN_TRANSACTION_ID_LENGTH-4); x++)
    {
        transid.id[x] = (uint8_t)(rand() % 256);
    }

    if (pTransId)
    {
        *pTransId = transid;
    }

    return AddTransactionId(transid);
}

HRESULT CStunMessageBuilder::AddAttributeHeader(uint16_t attribType, uint16_t size)
{
    HRESULT hr = S_OK;

    Chk(_stream.WriteUint16(htons(attribType)));
    Chk(_stream.WriteUint16(htons(size)));

Cleanup:
    return hr;
}

HRESULT CStunMessageBuilder::AddAttribute(uint16_t attribType, const void* data, uint16_t size)
{
    uint8_t padBytes[4] = {0};
    size_t padding = 0;
    HRESULT hr = S_OK;
    uint16_t sizeheader = size;

    if (data == NULL)
    {
        size = 0;
    }
    
    // attributes always start on a 4-byte boundary
    padding = (size % 4) ? (4 - (size % 4)) : 0;
    
    if (_fLegacyMode)
    {
        // in legacy mode (RFC 3489), the header size of the attribute includes the padding
        // in RFC 5389, the attribute header is the exact size of the data, and extra padding bytes are implicitly assumed
        sizeheader += padding;
    }
    
    // I suppose you can have zero length attributes as an indicator of something
    Chk(AddAttributeHeader(attribType, sizeheader));

    if (size > 0)
    {
        Chk(_stream.Write(data, size));
    }

    // pad with zeros to get the 4-byte alignment
    if (padding > 0)
    {
        Chk(_stream.Write(padBytes, padding));
    }

Cleanup:
    return hr;
}

HRESULT CStunMessageBuilder::AddStringAttribute(uint16_t attribType, const char* pstr)
{
    HRESULT hr = S_OK;
    // I can't think of a single string attribute that could be legitimately empty
    // AddNonce, AddRealm, AddUserName below depend on this check.  So if this check gets removed, add it back to everywhere else
    ChkIfA(StringHelper::IsNullOrEmpty(pstr), E_INVALIDARG);
    
    // AddAttribute allows empty attribute values, so if someone needs to add empty attribute values, do it with that call
    hr = AddAttribute(attribType, pstr, pstr?strlen(pstr):0);
    
Cleanup:
    return hr;
}

HRESULT CStunMessageBuilder::AddErrorCode(uint16_t errorNumber, const char* pszReason)
{
    HRESULT hr = S_OK;
    uint8_t padBytes[4] = {0};
    size_t strsize = (pszReason==NULL) ? 0 : strlen(pszReason);
    size_t size = strsize + 4;
    size_t sizeheader = size;
    size_t padding = 0;
    uint8_t cl = 0;
    uint8_t ernum = 0;

    ChkIf(strsize >= 128, E_INVALIDARG);
    ChkIf(errorNumber < 300, E_INVALIDARG);
    ChkIf(errorNumber > 600, E_INVALIDARG);
    
    padding = (size%4) ? (4-size%4) : 0;

    // fix for RFC 3489 clients - explicitly do the 4-byte padding alignment on the string with spaces instead of
    // padding the message with zeros. Adjust the length field to always be a multiple of 4.
    if ((size % 4) && _fLegacyMode)
    {
        padding = 4 - (size % 4);
    }
    
    if (_fLegacyMode)
    {
        sizeheader += padding;
    }

    Chk(AddAttributeHeader(STUN_ATTRIBUTE_ERRORCODE, sizeheader));

    Chk(_stream.WriteInt16(0));

    cl = (uint8_t)(errorNumber / 100);
    ernum = (uint8_t)(errorNumber % 100);

    Chk(_stream.WriteUint8(cl));
    Chk(_stream.WriteUint8(ernum));

    if (strsize > 0)
    {
        _stream.Write(pszReason, strsize);
    }
    
    if (padding > 0)
    {
        Chk(_stream.Write(padBytes, padding));
    }

Cleanup:
    return hr;
}

HRESULT CStunMessageBuilder::AddUnknownAttributes(const uint16_t* arr, size_t count)
{
    HRESULT hr = S_OK;
    uint16_t size = count * sizeof(uint16_t);
    uint16_t unpaddedsize = size;
    bool fPad = false;
    
    ChkIfA(arr == NULL, E_INVALIDARG);
    ChkIfA(count <= 0, E_INVALIDARG);
    
    // fix for RFC 3489. Since legacy clients can't understand implicit padding rules
    // of rfc 5389, then we do what rfc 3489 suggests.  If there are an odd number of attributes
    // that would make the length of the attribute not a multiple of 4, then repeat one
    // attribute.
    
    fPad = _fLegacyMode && (!!(count % 2));
    
    if (fPad)
    {
        size += sizeof(uint16_t);
    }
    
    Chk(AddAttributeHeader(STUN_ATTRIBUTE_UNKNOWNATTRIBUTES, size));
    
    Chk(_stream.Write(arr, unpaddedsize));
    
    if (fPad)
    {
        // repeat the last attribute in the array to get an even alignment of 4 bytes
        _stream.Write(&arr[count-1], sizeof(arr[0]));
    }
    
    
Cleanup:
    return hr;
}

HRESULT CStunMessageBuilder::AddXorMappedAddress(const CSocketAddress& addr)
{
    CSocketAddress addrxor(addr);
    uint16_t attributeID = _fLegacyMode ? STUN_ATTRIBUTE_XORMAPPEDADDRESS_OPTIONAL : STUN_ATTRIBUTE_XORMAPPEDADDRESS;

    addrxor.ApplyStunXorMap(_transactionid);

    return AddMappedAddressImpl(attributeID, addrxor);
}

HRESULT CStunMessageBuilder::AddMappedAddress(const CSocketAddress& addr)
{
    return AddMappedAddressImpl(STUN_ATTRIBUTE_MAPPEDADDRESS, addr);
}

HRESULT CStunMessageBuilder::AddResponseOriginAddress(const CSocketAddress& addr)
{
    uint16_t attribid = _fLegacyMode ? STUN_ATTRIBUTE_SOURCEADDRESS : STUN_ATTRIBUTE_RESPONSE_ORIGIN;
    
    return AddMappedAddressImpl(attribid, addr);
}

HRESULT CStunMessageBuilder::AddOtherAddress(const CSocketAddress& addr)
{
    uint16_t attribid = _fLegacyMode ? STUN_ATTRIBUTE_CHANGEDADDRESS : STUN_ATTRIBUTE_OTHER_ADDRESS;
    return AddMappedAddressImpl(attribid, addr);
}

HRESULT CStunMessageBuilder::AddResponsePort(uint16_t port)
{

    // convert to network byte order
    port = htons(port);
    return AddAttribute(STUN_ATTRIBUTE_RESPONSE_PORT, &port, sizeof(port));
}

HRESULT CStunMessageBuilder::AddPaddingAttribute(uint16_t paddingSize)
{
    HRESULT hr = S_OK;
    const uint16_t PADDING_BUFFER_SIZE = 128;
    static char padding_bytes[PADDING_BUFFER_SIZE] = {};

    // round up so we're a multiple of 4
    if (paddingSize % 4)
    {
        paddingSize = paddingSize + 4 - (paddingSize % 4);
    }


    Chk(AddAttributeHeader(STUN_ATTRIBUTE_PADDING, paddingSize));

    while (paddingSize > 0)
    {
        uint16_t blocksize = (paddingSize >= PADDING_BUFFER_SIZE) ? PADDING_BUFFER_SIZE : paddingSize;
        Chk(_stream.Write(padding_bytes, blocksize));
        paddingSize -= blocksize;
    }

Cleanup:
    return hr;

}


HRESULT CStunMessageBuilder::AddMappedAddressImpl(uint16_t attribute, const CSocketAddress& addr)
{
    uint16_t port;
    size_t length;
    uint8_t ip[STUN_IPV6_LENGTH];
    HRESULT hr = S_OK;
    uint8_t family = (addr.GetFamily()==AF_INET) ? STUN_ATTRIBUTE_FIELD_IPV4 :STUN_ATTRIBUTE_FIELD_IPV6;
    size_t attributeSize = (family == STUN_ATTRIBUTE_FIELD_IPV4) ? STUN_ATTRIBUTE_MAPPEDADDRESS_SIZE_IPV4 : STUN_ATTRIBUTE_MAPPEDADDRESS_SIZE_IPV6;

    Chk(AddAttributeHeader(attribute, attributeSize));

    port = addr.GetPort_NBO();
    length = addr.GetIP_NBO(ip, sizeof(ip));
    // if we ever had a length that was not a multiple of 4, we'd need to add padding
    ASSERT((length == STUN_IPV4_LENGTH) || (length == STUN_IPV6_LENGTH));


    Chk(_stream.WriteUint8(0));
    Chk(_stream.WriteUint8(family));
    Chk(_stream.WriteUint16(port));
    Chk(_stream.Write(ip, length));


Cleanup:
    return hr;
}

HRESULT CStunMessageBuilder::AddChangeRequest(const StunChangeRequestAttribute& changeAttrib)
{
    uint32_t changeData = 0;

    if (changeAttrib.fChangeIP)
    {
        changeData |= 0x04;
    }

    if (changeAttrib.fChangePort)
    {
        changeData |= 0x02;
    }

    changeData = htonl(changeData);
    return AddAttribute(STUN_ATTRIBUTE_CHANGEREQUEST, &changeData, sizeof(changeData));
}



HRESULT CStunMessageBuilder::AddFingerprintAttribute()
{
    boost::crc_32_type result;
    uint32_t value;
    CRefCountedBuffer spBuffer;
    void* pData = NULL;
    size_t length = 0;
    int offset;

    HRESULT hr = S_OK;

    Chk(_stream.WriteUint16(htons(STUN_ATTRIBUTE_FINGERPRINT)));
    Chk(_stream.WriteUint16(htons(sizeof(uint32_t)))); // field length is 4 bytes
    Chk(_stream.WriteUint32(0)); // dummy value for start

    Chk(FixLengthField());

    // now do a CRC-32 on everything but the last 8 bytes

    ChkA(_stream.GetBuffer(&spBuffer));
    pData = spBuffer->GetData();
    length = spBuffer->GetSize();

    ASSERT(length > 8);
    length = length-8;
    result.process_bytes(pData, length);

    value = result.checksum();
    value = value ^ STUN_FINGERPRINT_XOR;

    offset = -(int)(sizeof(value));

    Chk(_stream.SeekRelative(offset));

    Chk(_stream.WriteUint32(htonl(value)));

Cleanup:
    return hr;
}

HRESULT CStunMessageBuilder::AddUserName(const char* pszUserName)
{
    return AddStringAttribute(STUN_ATTRIBUTE_USERNAME, pszUserName);
}

HRESULT CStunMessageBuilder::AddNonce(const char* pszNonce)
{
    return AddStringAttribute(STUN_ATTRIBUTE_NONCE, pszNonce);
}

HRESULT CStunMessageBuilder::AddRealm(const char* pszRealm)
{
    return AddStringAttribute(STUN_ATTRIBUTE_REALM, pszRealm);
}


HRESULT CStunMessageBuilder::AddMessageIntegrityImpl(uint8_t* key, size_t keysize)
{
    HRESULT hr = S_OK;
    const size_t c_hmacsize = 20;
    uint8_t hmacvaluedummy[c_hmacsize] = {}; // zero-init
    unsigned int resultlength = c_hmacsize;
    uint8_t* pDstBuf = NULL;
    
    CRefCountedBuffer spBuffer;
    void* pData = NULL;
    size_t length = 0;
    unsigned char* pHashResult = NULL;
    UNREFERENCED_VARIABLE(pHashResult);
    
    ChkIfA(key==NULL || keysize <= 0, E_INVALIDARG);
    
    // add in a "zero-init" HMAC value.  This adds 24 bytes to the length
    Chk(AddAttribute(STUN_ATTRIBUTE_MESSAGEINTEGRITY, hmacvaluedummy, ARRAYSIZE(hmacvaluedummy)));

    Chk(FixLengthField());
    // now do a SHA1 on everything but the last 24 bytes (4 bytes of the attribute header and 20 bytes for the dummy content)

    ChkA(_stream.GetBuffer(&spBuffer));
    pData = spBuffer->GetData();
    length = spBuffer->GetSize();

    ASSERT(length > 24);
    length = length-24;
    
    
    // now do a little pointer math so that HMAC can write exactly to where the hash bytes will appear
    pDstBuf = ((uint8_t*)pData) + length + 4;
    
#ifndef __APPLE__
    pHashResult = HMAC(EVP_sha1(), key, keysize, (uint8_t*)pData, length, pDstBuf, &resultlength);
    ASSERT(resultlength == 20);
    ASSERT(pHashResult != NULL);
#else
    CCHmac(kCCHmacAlgSHA1, key, keysize,(uint8_t*)pData, length, pDstBuf);
    UNREFERENCED_VARIABLE(resultlength);
#endif
    
Cleanup:
    return hr;
}

HRESULT CStunMessageBuilder::AddMessageIntegrityShortTerm(const char* pszPassword)
{
    return AddMessageIntegrityImpl((uint8_t*)pszPassword, strlen(pszPassword)); // if password is null/empty, AddMessageIntegrityImpl will ChkIfA on it
}

HRESULT CStunMessageBuilder::AddMessageIntegrityLongTerm(const char* pszUserName, const char* pszRealm, const char* pszPassword)
{
    HRESULT hr = S_OK;
    const size_t MAX_KEY_SIZE = MAX_STUN_AUTH_STRING_SIZE*3 + 2;
    uint8_t key[MAX_KEY_SIZE + 1]; // long enough for 64-char strings and two semicolons and a null char for debugging
    
    uint8_t hash[MD5_DIGEST_LENGTH] = {};
    uint8_t* pResult = NULL;
    uint8_t* pDst = key;
    
    size_t lenUserName = pszUserName ? strlen(pszUserName) : 0;
    size_t lenRealm = pszRealm ? strlen(pszRealm) : 0;
    size_t lenPassword = pszPassword ? strlen(pszPassword) : 0;
    size_t lenTotal = lenUserName + lenRealm + lenPassword + 2; // +2 for the two colons

    UNREFERENCED_VARIABLE(pResult);
    
    ChkIfA(lenTotal > MAX_KEY_SIZE, E_INVALIDARG); // if we ever hit this limit, just increase MAX_STUN_AUTH_STRING_SIZE
    
    // too bad CDatastream really only works on refcounted buffers.  Otherwise, we wouldn't have to do all this messed up pointer math
    
    // We could create a refcounted buffer in this function, but that would mean a call to "new and delete", and we're trying to avoid memory allocations in 
    // critical code paths because they are a proven perf hit
    
    // TODO - Fix CDataStream and CBuffer so that "ref counted buffers" are no longer needed
    
    pDst = key;
    
    memcpy(pDst, pszUserName, lenUserName);
    pDst += lenUserName;
    
    *pDst = ':';
    pDst++;
    
    memcpy(pDst, pszRealm, lenRealm);
    pDst += lenRealm;
    
    *pDst = ':';
    pDst++;

    memcpy(pDst, pszPassword, lenPassword);
    pDst += lenPassword;
    *pDst ='\0'; // null terminate for debugging (this char doesn not get hashed
    
    ASSERT(key+lenTotal == pDst);

#ifndef __APPLE__
    pResult = MD5(key, lenTotal, hash);
#else
    pResult = CC_MD5(key, lenTotal, hash);
#endif
    
    ASSERT(pResult != NULL);
    hr= AddMessageIntegrityImpl(hash, MD5_DIGEST_LENGTH);
    
Cleanup:
    return hr;
}

        

HRESULT CStunMessageBuilder::FixLengthField()
{
    size_t size = _stream.GetSize();
    size_t currentPos = _stream.GetPos();
    HRESULT hr = S_OK;

    ChkIfA(size < STUN_HEADER_SIZE, E_UNEXPECTED);

    if (size < STUN_HEADER_SIZE)
    {
        size = 0;
    }
    else
    {
        size = size - STUN_HEADER_SIZE;
    }

    ChkA(_stream.SeekDirect(2)); // 2 bytes into the stream is the length
    ChkA(_stream.WriteUint16(ntohs(size)));
    ChkA(_stream.SeekDirect(currentPos));

Cleanup:
    return hr;
}

HRESULT CStunMessageBuilder::GetResult(CRefCountedBuffer* pspBuffer)
{
    HRESULT hr;

    hr = FixLengthField();
    if (SUCCEEDED(hr))
    {
        hr = _stream.GetBuffer(pspBuffer);
    }
    return hr;
}

CDataStream& CStunMessageBuilder::GetStream()
{
    return _stream;
}
