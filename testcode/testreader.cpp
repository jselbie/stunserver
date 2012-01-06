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



#include "commonincludes.h"
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
    return Test1();
}


HRESULT CTestReader::Test1()
{
    HRESULT hr = S_OK;

    StunAttribute attrib;
    const char* pszExpectedSoftwareAttribute = "STUN test client";
    const char* pszExpectedUserName = "evtj:h6vY";
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

    ChkIfA(0 != ::strncmp(pszExpectedSoftwareAttribute, (const char*)(spBuffer->GetData() + attrib.offset), attrib.size), E_FAIL);

    ChkA(reader.GetAttributeByType(STUN_ATTRIBUTE_USERNAME, &attrib));

    ChkIfA(attrib.attributeType != STUN_ATTRIBUTE_USERNAME, E_FAIL);

    ChkIfA(0 != ::strncmp(pszExpectedUserName, (const char*)(spBuffer->GetData() + attrib.offset), attrib.size), E_FAIL);
    
    
    ChkA(reader.GetStringAttributeByType(STUN_ATTRIBUTE_SOFTWARE, szStringValue, ARRAYSIZE(szStringValue)));
    ChkIfA(0 != ::strcmp(pszExpectedSoftwareAttribute, szStringValue), E_FAIL);

    ChkIfA(reader.HasFingerprintAttribute() == false, E_FAIL);

    ChkIfA(reader.IsFingerprintAttributeValid() == false, E_FAIL);
    
    ChkIfA(reader.HasMessageIntegrityAttribute() == false, E_FAIL);
    
    ChkA(reader.ValidateMessageIntegrityShort(c_password));

Cleanup:
    return hr;
 }

