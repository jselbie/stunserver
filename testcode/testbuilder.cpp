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

#include "testbuilder.h"

HRESULT CTestBuilder::Run()
{
    HRESULT hr = S_OK;
    Chk(Test1())
    Chk(Test2());
Cleanup:
    return hr;
}

// The goal of this test is to just validate that we can create a message from CStunMessageBuilder and have it's output parsed correctly by CStunMessageReader
// Also helps validate CSocketAddress
HRESULT CTestBuilder::Test1()
{
    HRESULT hr = S_OK;
    CStunMessageBuilder builder;
    CStunMessageReader reader;
    StunAttribute attrib;
    CRefCountedBuffer spBuffer;
    CRefCountedBuffer spBufferReader;
    CSocketAddress addrValidate(0,0);
    StunTransactionId transid = {};
    uint32_t ipvalidate = 0;


    CSocketAddress addr(0x7f000001, 9999);
    CSocketAddress addrOrigin(0xAABBCCDD, 8888);
    CSocketAddress addrOther(0x11223344, 7777);
    

    ChkA(builder.AddBindingRequestHeader());
    ChkA(builder.AddRandomTransactionId(&transid));
    ChkA(builder.AddStringAttribute(STUN_ATTRIBUTE_SOFTWARE, "FOOBAR"));
    ChkA(builder.AddMappedAddress(addr));
    ChkA(builder.AddXorMappedAddress(addr));
    ChkA(builder.AddOtherAddress(addrOther));
    ChkA(builder.AddResponseOriginAddress(addrOrigin));
    ChkA(builder.AddFingerprintAttribute());
    ChkA(builder.GetResult(&spBuffer));

    ChkIfA(CStunMessageReader::BodyValidated != reader.AddBytes(spBuffer->GetData(), spBuffer->GetSize()), E_FAIL);

    ChkIfA(reader.HasFingerprintAttribute() == false, E_FAIL);

    ChkIfA(reader.IsFingerprintAttributeValid() == false, E_FAIL);

    ChkIfA(reader.GetMessageClass() != StunMsgClassRequest, E_FAIL);

    ChkIfA(reader.GetMessageType() != StunMsgTypeBinding, E_FAIL);

    ChkA(reader.GetBuffer(&spBufferReader));

    ChkA(reader.GetAttributeByType(STUN_ATTRIBUTE_SOFTWARE, &attrib));

    ChkIfA(attrib.attributeType != STUN_ATTRIBUTE_SOFTWARE, E_FAIL);

    ChkIfA(0 != ::strncmp("FOOBAR", (const char*)(spBufferReader->GetData() + attrib.offset), attrib.size), E_FAIL);

    ChkA(reader.GetXorMappedAddress(&addrValidate));
    ChkIf(addrValidate.IsSameIP_and_Port(addr) == false, E_FAIL);
    ChkIfA(addrValidate.GetIPLength() != 4, E_FAIL);

    addrValidate = CSocketAddress(0,0);
    ChkA(reader.GetMappedAddress(&addrValidate));
    ChkIfA(addrValidate.GetPort() != 9999, E_FAIL);
    ChkIfA(addrValidate.GetIPLength() != 4, E_FAIL);
    ChkIfA(4 != addrValidate.GetIP(&ipvalidate, 4), E_FAIL);
    ChkIfA(ipvalidate != 0x7f000001, E_FAIL);

    addrValidate = CSocketAddress(0,0);
    ipvalidate = 0;
    reader.GetOtherAddress(&addrValidate);
    ChkIfA(addrValidate.GetPort() != 7777, E_FAIL);
    ChkIfA(addrValidate.GetIPLength() != 4, E_FAIL);
    ChkIfA(4 != addrValidate.GetIP(&ipvalidate, 4), E_FAIL);
    ChkIf(ipvalidate != 0x11223344, E_FAIL);


Cleanup:
   return hr;
}


// this test validates that IPV6 addresses work fine with CStunMessageBuilder and CStunMessageReader
HRESULT CTestBuilder::Test2()
{
    HRESULT hr = S_OK;
    CSocketAddress addr(0,0);
    CSocketAddress addrValidate(0,0);
    const char* ip6addr = "ABCDEFGHIJKLMNOP";
    sockaddr_in6 addr6 = {};
    CStunMessageReader reader;
    StunTransactionId transid;
    CStunMessageBuilder builder;
    CRefCountedBuffer spBuffer;

    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = htons(9999);
    memcpy(addr6.sin6_addr.s6_addr, ip6addr, 16);
    addr = CSocketAddress(addr6);

    ChkA(builder.AddHeader(StunMsgTypeBinding, StunMsgClassRequest));
    ChkA(builder.AddRandomTransactionId(&transid));
    ChkA(builder.AddMappedAddress(addr));
    ChkA(builder.AddXorMappedAddress(addr));
    ChkA(builder.GetResult(&spBuffer));

    ChkIfA(CStunMessageReader::BodyValidated != reader.AddBytes(spBuffer->GetData(), spBuffer->GetSize()), E_FAIL);

    ChkA(reader.GetXorMappedAddress(&addrValidate));
    
    ChkIf(addrValidate.IsSameIP_and_Port(addr) == false, E_FAIL);

Cleanup:
    return hr;
}


