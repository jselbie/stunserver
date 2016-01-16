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

