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
#include "testintegrity.h"


// This test validates that the construction and parsing of the message integrity attribute in a stun message works as expected
// The test also validates both short term and long term credential modes with or without the presence of a fingerprint attribute
HRESULT CTestIntegrity::TestMessageIntegrity(bool fWithFingerprint, bool fLongCredentials)
{
    HRESULT hr = S_OK;
    
    const char* pszUserName = "username";
    const char* pszRealm = "stunrealm";
    const char* pszPassword = "ThePassword";
    
    CStunMessageBuilder builder;
    CStunMessageReader reader;
    uint8_t *pMsg = NULL;
    size_t sizeMsg = 0;
    CStunMessageReader::ReaderParseState state;
    CRefCountedBuffer spBuffer;
    
    builder.AddBindingRequestHeader();
    builder.AddRandomTransactionId(NULL);
    builder.AddUserName(pszUserName);
    builder.AddRealm(pszRealm);
    
    
    if (fLongCredentials == false)
    {
        Chk(builder.AddMessageIntegrityShortTerm(pszPassword));
    }
    else
    {
        Chk(builder.AddMessageIntegrityLongTerm(pszUserName, pszRealm, pszPassword));
    }
    
    if (fWithFingerprint)
    {
        builder.AddFingerprintAttribute();
    }
    
    Chk(builder.GetResult(&spBuffer));
    
    pMsg = spBuffer->GetData();
    sizeMsg = spBuffer->GetSize();
    
    state = reader.AddBytes(pMsg, sizeMsg);
    
    ChkIfA(state != CStunMessageReader::BodyValidated, E_FAIL);
    
    ChkIfA(reader.HasMessageIntegrityAttribute()==false, E_FAIL);
    
    if (fLongCredentials == false)
    {
        ChkA(reader.ValidateMessageIntegrityShort(pszPassword));
    }
    else
    {
        ChkA(reader.ValidateMessageIntegrityLong(pszUserName, pszRealm, pszPassword));
    }
    
Cleanup:
    return hr;
}


HRESULT CTestIntegrity::Test2()
{
    HRESULT hr = S_OK;
    
    // CTestReader contains a test that will the fingerprint and integrity
    // of the message in RFC 5769 section 2.1 (short-term auth)
    
    // This test is a validation of section 2.4 (long term auth with integrity and fingerprint)
    
    const unsigned char c_requestbytes[] = 
    "\x00\x01\x00\x60"    //   Request type and message length  
    "\x21\x12\xa4\x42"    //   Magic cookie
    "\x78\xad\x34\x33"    // }
    "\xc6\xad\x72\xc0"    // } TransactionID
    "\x29\xda\x41\x2e"    // }
    "\x00\x06\x00\x12"    //   USERNAME ATTRIBUTE HEADER
    "\xe3\x83\x9e\xe3"    // }
    "\x83\x88\xe3\x83"    // }
    "\xaa\xe3\x83\x83"    // } Username value (18 bytes) and padding (2 bytes)
    "\xe3\x82\xaf\xe3"    // }
    "\x82\xb9\x00\x00"    // }
    "\x00\x15\x00\x1c"    //   NONCE ATTRIBUTE HEADER
    "\x66\x2f\x2f\x34"    // }    
    "\x39\x39\x6b\x39"    // }
    "\x35\x34\x64\x36"    // }
    "\x4f\x4c\x33\x34"    // } Nonce value
    "\x6f\x4c\x39\x46"    // }
    "\x53\x54\x76\x79"    // }
    "\x36\x34\x73\x41"    // }
    "\x00\x14\x00\x0b"    //   REALM attribute header
    "\x65\x78\x61\x6d"    // }
    "\x70\x6c\x65\x2e"    // }   Realm value (11 bytes) and padding (1 byte)
    "\x6f\x72\x67\x00"    // }
    "\x00\x08\x00\x14"    //   MESSAGE INTEGRITY attribute HEADER
    "\xf6\x70\x24\x65"    // }
    "\x6d\xd6\x4a\x3e"    // }
    "\x02\xb8\xe0\x71"    // } HMAC-SHA1 fingerprint
    "\x2e\x85\xc9\xa2"    // }
    "\x8c\xa8\x96\x66";   // }

    const char c_username[] = "\xe3\x83\x9e\xe3\x83\x88\xe3\x83\xaa\xe3\x83\x83\xe3\x82\xaf\xe3\x82\xb9";
    const char c_password[] = "TheMatrIX";
    // const char c_nonce[] = "f//499k954d6OL34oL9FSTvy64sA";
    const char c_realm[] = "example.org";


    CStunMessageReader reader;
    
    reader.AddBytes(c_requestbytes, sizeof(c_requestbytes)-1); // -1 to get rid of the trailing null
    ChkIfA(reader.GetState() != CStunMessageReader::BodyValidated, E_FAIL);

    ChkIfA(reader.HasMessageIntegrityAttribute() == false, E_FAIL);
    
    ChkA(reader.ValidateMessageIntegrityLong(c_username, c_realm, c_password));
    
    
Cleanup:
    return hr;
}




HRESULT CTestIntegrity::Run()
{
    HRESULT hr = S_OK;
    
    Chk(TestMessageIntegrity(false, false));
    ChkA(TestMessageIntegrity(true, false));
    
    Chk(TestMessageIntegrity(false, true));
    ChkA(TestMessageIntegrity(true, true));
    
    ChkA(Test2());
    
    
Cleanup:
    return hr;
}

