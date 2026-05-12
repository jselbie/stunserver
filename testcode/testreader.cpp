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

#include "testreader.h"


// the following request block is from RFC 5769, section 2.1
// static
const unsigned char c_requestbytes[] =
 "\x00\x01\x00\x58"
 "\x21\x12\xa4\x42"
 "\xb7\xe7\xa7\x01\xbc\x34\xd6\x86\xfa\x87\xdf\xae"
 "\x80\x22\x00\x10"
   "STUN test client"
 "\x00\x24\x00\x04"
   "\x6e\x00\x01\xff"
 "\x80\x29\x00\x08"
   "\x93\x2f\xf9\xb1\x51\x26\x3b\x36"
 "\x00\x06\x00\x09"
   "\x65\x76\x74\x6a\x3a\x68\x36\x76\x59\x20\x20\x20"
 "\x00\x08\x00\x14"
   "\x9a\xea\xa7\x0c\xbf\xd8\xcb\x56\x78\x1e\xf2\xb5"
   "\xb2\xd3\xf2\x49\xc1\xb5\x71\xa2"
 "\x80\x28\x00\x04"
   "\xe5\x7a\x3b\xcf";

const char c_password[] = "VOkJxbRl1RmTxUk/WvJxBt";
const char c_username[] = "evtj:h6vY";
const char c_software[] = "STUN test client";


HRESULT CTestReader::Run()
{
    HRESULT hr = S_OK;
    Chk(Test1());
    Chk(Test2());
    Chk(TestErrorCodeMissing());
    Chk(TestErrorCodeValid());
    Chk(TestErrorCodeWithReason());
    Chk(TestErrorCodeTruncatedValue());
    Chk(TestErrorCodeNullOut());
Cleanup:
    return hr;
}


HRESULT CTestReader::Test1()
{
    HRESULT hr = S_OK;

    StunAttribute attrib;
    CRefCountedBuffer spBuffer;
    char szStringValue[100];
    
    const unsigned char *req = c_requestbytes;
    size_t requestsize = sizeof(c_requestbytes)-1; // -1 to get rid of the trailing null

    CStunMessageReader reader;
    CStunMessageReader::ReaderParseState state;

    // reader is expecting at least enough bytes to fill the header
    ChkIfA(reader.AddBytes(NULL, 0) != CStunMessageReader::HeaderNotRead, E_FAIL);
    ChkIfA(reader.HowManyBytesNeeded() != STUN_HEADER_SIZE, E_FAIL);

    state = reader.AddBytes(req, requestsize);
    ChkIfA(state != CStunMessageReader::BodyValidated, E_FAIL);

    ChkIfA(reader.HowManyBytesNeeded() != 0, E_FAIL);

    ChkA(reader.GetBuffer(&spBuffer));

    ChkIfA(reader.GetMessageClass() != StunMsgClassRequest, E_FAIL);

    ChkIfA(reader.GetMessageType() != StunMsgTypeBinding, E_FAIL);

    ChkA(reader.GetAttributeByType(STUN_ATTRIBUTE_SOFTWARE, &attrib));

    ChkIfA(attrib.attributeType != STUN_ATTRIBUTE_SOFTWARE, E_FAIL);

    ChkIfA(0 != ::strncmp(c_software, (const char*)(spBuffer->GetData() + attrib.offset), attrib.size), E_FAIL);

    ChkA(reader.GetAttributeByType(STUN_ATTRIBUTE_USERNAME, &attrib));

    ChkIfA(attrib.attributeType != STUN_ATTRIBUTE_USERNAME, E_FAIL);

    ChkIfA(0 != ::strncmp(c_username, (const char*)(spBuffer->GetData() + attrib.offset), attrib.size), E_FAIL);
    
    
    ChkA(reader.GetStringAttributeByType(STUN_ATTRIBUTE_SOFTWARE, szStringValue, ARRAYSIZE(szStringValue)));
    ChkIfA(0 != ::strcmp(c_software, szStringValue), E_FAIL);

    ChkIfA(reader.HasFingerprintAttribute() == false, E_FAIL);

    ChkIfA(reader.IsFingerprintAttributeValid() == false, E_FAIL);
    
    ChkIfA(reader.HasMessageIntegrityAttribute() == false, E_FAIL);
    
    ChkA(reader.ValidateMessageIntegrityShort(c_password));

Cleanup:
    return hr;
 }

HRESULT CTestReader::Test2()
{
    HRESULT hr = S_OK;

   // this test is to validate an extreme case for TCP scenarios.
   // what if the bytes only arrived "one at a time"? 
   // or if the byte chunks straddled across logical parse segments (i.e. the header and the body)
   // Can CStunMessageReader::AddBytes handle and parse out the correct result

    for (size_t chunksize = 1; chunksize <= 30; chunksize++)
    {
        Chk(TestFixedReadSizes(chunksize));
    }

    srand(888);
    for (size_t i = 0; i < 200; i++)
    {
        Chk(TestFixedReadSizes(0));
    }
Cleanup:
    return hr;
}

// ---------------------------------------------------------------------------
// GetErrorCode tests
// ---------------------------------------------------------------------------

// No ERROR-CODE attribute present: GetErrorCode must return failure.
HRESULT CTestReader::TestErrorCodeMissing()
{
    HRESULT hr = S_OK;
    CStunMessageBuilder builder;
    CRefCountedBuffer spBuffer;
    CStunMessageReader reader;
    uint16_t errorCode = 0;

    ChkA(builder.AddBindingRequestHeader());
    ChkA(builder.AddRandomTransactionId(NULL));
    ChkA(builder.FixLengthField());
    ChkA(builder.GetResult(&spBuffer));

    ChkIfA(CStunMessageReader::BodyValidated != reader.AddBytes(spBuffer->GetData(), spBuffer->GetSize()), E_FAIL);
    ChkIfA(SUCCEEDED(reader.GetErrorCode(&errorCode)), E_FAIL);

Cleanup:
    return hr;
}

// Well-formed ERROR-CODE attributes (size >= 4, no reason phrase): verify all
// standard error codes round-trip correctly.
HRESULT CTestReader::TestErrorCodeValid()
{
    static const uint16_t c_codes[] = {
        STUN_ERROR_TRYALTERNATE,
        STUN_ERROR_BADREQUEST,
        STUN_ERROR_UNAUTHORIZED,
        STUN_ERROR_UNKNOWNATTRIB,
        STUN_ERROR_STALENONCE,
        STUN_ERROR_SERVERERROR,
    };

    HRESULT hr = S_OK;
    for (size_t i = 0; i < ARRAYSIZE(c_codes); i++)
    {
        CStunMessageBuilder builder;
        CRefCountedBuffer spBuffer;
        CStunMessageReader reader;
        uint16_t errorCode = 0;

        ChkA(builder.AddBindingResponseHeader(false));
        ChkA(builder.AddRandomTransactionId(NULL));
        ChkA(builder.AddErrorCode(c_codes[i], NULL));
        ChkA(builder.FixLengthField());
        ChkA(builder.GetResult(&spBuffer));

        ChkIfA(CStunMessageReader::BodyValidated != reader.AddBytes(spBuffer->GetData(), spBuffer->GetSize()), E_FAIL);
        ChkA(reader.GetErrorCode(&errorCode));
        ChkIfA(errorCode != c_codes[i], E_FAIL);
    }

Cleanup:
    return hr;
}

// ERROR-CODE attribute with a reason phrase: class and number must still decode
// correctly regardless of how long the trailing UTF-8 reason phrase is.
HRESULT CTestReader::TestErrorCodeWithReason()
{
    HRESULT hr = S_OK;
    CStunMessageBuilder builder;
    CRefCountedBuffer spBuffer;
    CStunMessageReader reader;
    uint16_t errorCode = 0;

    ChkA(builder.AddBindingResponseHeader(false));
    ChkA(builder.AddRandomTransactionId(NULL));
    ChkA(builder.AddErrorCode(STUN_ERROR_BADREQUEST, "Bad Request"));
    ChkA(builder.FixLengthField());
    ChkA(builder.GetResult(&spBuffer));

    ChkIfA(CStunMessageReader::BodyValidated != reader.AddBytes(spBuffer->GetData(), spBuffer->GetSize()), E_FAIL);
    ChkA(reader.GetErrorCode(&errorCode));
    ChkIfA(errorCode != STUN_ERROR_BADREQUEST, E_FAIL);

Cleanup:
    return hr;
}

// ERROR-CODE attribute value shorter than the required 4 bytes: GetErrorCode must
// return failure for sizes 0, 1, 2, and 3.  Size 0 was the original out-of-bounds
// read; all four are now rejected by the size < 4 guard.
HRESULT CTestReader::TestErrorCodeTruncatedValue()
{
    HRESULT hr = S_OK;
    // Four bytes that look like a valid error-code body (class=4, num=0 → 400).
    // We truncate the declared length to expose undersized attributes.
    static const uint8_t c_validBody[4] = {0x00, 0x00, 0x04, 0x00};

    for (uint16_t badSize = 0; badSize <= 3; badSize++)
    {
        CStunMessageBuilder builder;
        CRefCountedBuffer spBuffer;
        CStunMessageReader reader;
        uint16_t errorCode = 0xffff;

        ChkA(builder.AddBindingResponseHeader(false));
        ChkA(builder.AddRandomTransactionId(NULL));
        // AddAttribute bypasses AddErrorCode's own minimum-size check, letting us
        // craft an attribute with a declared length smaller than 4.
        ChkA(builder.AddAttribute(STUN_ATTRIBUTE_ERRORCODE,
                                  (badSize > 0) ? c_validBody : NULL,
                                  badSize));
        ChkA(builder.FixLengthField());
        ChkA(builder.GetResult(&spBuffer));

        // The parser must accept the message (it only rejects oversized attributes).
        ChkIfA(CStunMessageReader::BodyValidated != reader.AddBytes(spBuffer->GetData(), spBuffer->GetSize()), E_FAIL);

        // GetErrorCode must reject the undersized attribute cleanly, not crash.
        ChkIfA(SUCCEEDED(reader.GetErrorCode(&errorCode)), E_FAIL);
    }

Cleanup:
    return hr;
}

// NULL output pointer: GetErrorCode must return E_INVALIDARG without touching
// the attribute data.
HRESULT CTestReader::TestErrorCodeNullOut()
{
    HRESULT hr = S_OK;
    CStunMessageBuilder builder;
    CRefCountedBuffer spBuffer;
    CStunMessageReader reader;

    ChkA(builder.AddBindingResponseHeader(false));
    ChkA(builder.AddRandomTransactionId(NULL));
    ChkA(builder.AddErrorCode(STUN_ERROR_BADREQUEST, NULL));
    ChkA(builder.FixLengthField());
    ChkA(builder.GetResult(&spBuffer));

    ChkIfA(CStunMessageReader::BodyValidated != reader.AddBytes(spBuffer->GetData(), spBuffer->GetSize()), E_FAIL);
    ChkIfA(reader.GetErrorCode(NULL) != E_INVALIDARG, E_FAIL);

Cleanup:
    return hr;
}

// ---------------------------------------------------------------------------

HRESULT CTestReader::TestFixedReadSizes(size_t chunksize)
{

    HRESULT hr = S_OK;
    CStunMessageReader reader;
    CStunMessageReader::ReaderParseState prevState, state;
    size_t bytesread = 0;
    bool fRandomChunkSizing = (chunksize==0);
    
    
    prevState = CStunMessageReader::HeaderNotRead;
    state = prevState;
    size_t msgSize = sizeof(c_requestbytes)-1; // c_requestbytes is a string, hence the -1
    while (bytesread < msgSize)
    {
        size_t remaining, toread;
        
        if (fRandomChunkSizing)
        {
            chunksize = (rand() % 17) + 1;
        }
        
        remaining = msgSize - bytesread;
        toread = (remaining > chunksize) ? chunksize : remaining;
        
        state = reader.AddBytes(&c_requestbytes[bytesread], toread);
        bytesread += toread;
        
        ChkIfA(state == CStunMessageReader::ParseError, E_UNEXPECTED);
        
        if ((state == CStunMessageReader::HeaderValidated) && (prevState != CStunMessageReader::HeaderValidated))
        {
            ChkIfA(bytesread < STUN_HEADER_SIZE, E_UNEXPECTED);
        }
        
        if ((state == CStunMessageReader::BodyValidated) && (prevState != CStunMessageReader::BodyValidated))
        {
            ChkIfA(prevState != CStunMessageReader::HeaderValidated, E_UNEXPECTED);
            ChkIfA(bytesread != msgSize, E_UNEXPECTED);
        }
        
        prevState = state;
    }
    
    ChkIfA(reader.GetState() != CStunMessageReader::BodyValidated, E_UNEXPECTED);
    
    // just validate the integrity and fingerprint, that should cover all the attributes
    ChkA(reader.ValidateMessageIntegrityShort(c_password));
    ChkIfA(reader.IsFingerprintAttributeValid() == false, E_FAIL);
    
Cleanup:
    return hr;
}

