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
#include "stuntypes.h"
#include "socketaddress.h"
#include "buffer.h"
#include "datastream.h"


bool IsTransactionIdValid(StunTransactionId& transid)
{
    uint8_t zerobytes[sizeof(transid.id)] = {}; // zero-init
    return (0 != memcmp(&transid.id, zerobytes, sizeof(zerobytes)));
}


HRESULT GetMappedAddress(uint8_t* pData, size_t size, CSocketAddress* pAddr)
{
    uint16_t port;
    HRESULT hr = S_OK;
    uint8_t attributeid;
    uint8_t ip6[STUN_IPV6_LENGTH];
    uint32_t ip4;


    CRefCountedBuffer spBuffer(new CBuffer(pData, size, false));
    CDataStream stream(spBuffer);

    ChkIfA(pAddr==nullptr, E_INVALIDARG);

    Chk(stream.SeekDirect(1)); // skip over the zero byte

    Chk(stream.ReadUint8(&attributeid));
    Chk(stream.ReadUint16(&port));
    port = ntohs(port);

    if (attributeid == STUN_ATTRIBUTE_FIELD_IPV4)
    {
        Chk(stream.ReadUint32(&ip4));
        ip4 = ntohl(ip4);
        *pAddr = CSocketAddress(ip4, port);
    }
    else
    {
        sockaddr_in6 addr6={};
        Chk(stream.Read(ip6, STUN_IPV6_LENGTH));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(port);
        memcpy(&addr6.sin6_addr, ip6, STUN_IPV6_LENGTH);
        *pAddr = CSocketAddress(addr6);
    }

Cleanup:
    return hr;
}

HRESULT GetXorMappedAddress(uint8_t* pData, size_t size, StunTransactionId &transid, CSocketAddress* pAddr)
{

    HRESULT hr = S_OK;

    Chk(GetMappedAddress(pData, size, pAddr));

    pAddr->ApplyStunXorMap(transid);

Cleanup:
    return hr;


}

