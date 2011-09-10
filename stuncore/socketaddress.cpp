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
#include "socketaddress.h"


CSocketAddress::CSocketAddress() :
_address() // zero-init
{
    _address.addr4.sin_family = AF_INET;
}

CSocketAddress::CSocketAddress(const sockaddr& addr)
{
    CommonConstructor(addr);
}

CSocketAddress::CSocketAddress(const sockaddr_storage& addr)
{
    CommonConstructor(*(sockaddr*)&addr);
}


void CSocketAddress::CommonConstructor(const sockaddr& addr)
{
    ASSERT((addr.sa_family == AF_INET) || (addr.sa_family==AF_INET6));

    if (addr.sa_family == AF_INET6)
    {
        _address.addr6 = *(sockaddr_in6*)&addr;
    }
    else if (addr.sa_family == AF_INET)
    {
        _address.addr4 = *(sockaddr_in*)&addr;
    }
    else
    {
        _address.addr = addr;
    }
}

CSocketAddress::CSocketAddress(const sockaddr_in6& addr6)
{
    ASSERT(addr6.sin6_family==AF_INET6);
    _address.addr6 = addr6;
}

CSocketAddress::CSocketAddress(const sockaddr_in& addr4)
{
    ASSERT(addr4.sin_family == AF_INET);
    _address.addr4 = addr4;
}

CSocketAddress::CSocketAddress(uint32_t ip, uint16_t port)
{
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(ip);

    _address.addr4 = addr;
}

uint16_t CSocketAddress::GetPort() const
{
    return ntohs(GetPort_NBO());
}

void CSocketAddress::SetPort(uint16_t port)
{
    if (_address.addr.sa_family == AF_INET)
    {
        _address.addr4.sin_port = htons(port);
    }
    else
    {
        _address.addr6.sin6_port = htons(port);
    }
}

uint16_t CSocketAddress::GetPort_NBO() const
{
    if (_address.addr.sa_family == AF_INET)
    {
        return _address.addr4.sin_port;
    }
    else
    {
        return _address.addr6.sin6_port;
    }
}

uint16_t CSocketAddress::GetIPLength() const
{
    uint16_t length;

    if (_address.addr.sa_family == AF_INET)
    {
        length = sizeof(_address.addr4.sin_addr.s_addr);
    }
    else
    {
        length  = sizeof(_address.addr6.sin6_addr.s6_addr);
    }

    ASSERT((length == STUN_IPV4_LENGTH) || (length == STUN_IPV6_LENGTH));
    return length;
}


size_t CSocketAddress::GetIPImpl(void* pAddr, size_t length, bool fNBO) const
{
    HRESULT hr = S_OK;
    size_t bytescopied = 0;

    ChkIfA(pAddr == NULL, E_INVALIDARG);
    ChkIfA(length <= 0, E_INVALIDARG);
    ChkIfA(length < GetIPLength(), E_INVALIDARG);

    ASSERT((_address.addr.sa_family == AF_INET)||(_address.addr.sa_family == AF_INET6));

    if (_address.addr.sa_family == AF_INET)
    {
        uint32_t ip = _address.addr4.sin_addr.s_addr;
        if (fNBO==false)
        {
            ip = htonl(ip);
        }
        memcpy(pAddr, &ip, sizeof(ip));
        bytescopied = sizeof(ip);
        ASSERT(sizeof(ip) == STUN_IPV4_LENGTH);
    }
    else
    {
        // ipv6 addresses are the same in either byte ordering - just an array of 16 bytes
        ASSERT(sizeof(_address.addr6.sin6_addr.s6_addr) == STUN_IPV6_LENGTH);

        memcpy(pAddr, &_address.addr6.sin6_addr.s6_addr, sizeof(_address.addr6.sin6_addr.s6_addr));
        bytescopied = STUN_IPV6_LENGTH;
    }

Cleanup:
    return bytescopied;

}


size_t CSocketAddress::GetIP(void* pAddr, size_t length) const // returns the number of bytes copied, or Zero on error
{
    return GetIPImpl(pAddr, length, false);
}


size_t CSocketAddress::GetIP_NBO(void* pAddr, size_t length) const // returns the number of bytes copied, or Zero on error
{
    return GetIPImpl(pAddr, length, true);
}

uint16_t CSocketAddress::GetFamily() const
{
    return _address.addr.sa_family;
}


void CSocketAddress::ApplyStunXorMap(const StunTransactionId& transid)
{


    // XOR Mapped address is only understood by clients written for RFC 5389 compliance
    // If we're attempting to map a xor address to an RFC 3489 client, it's transaction id
    // won't start with the stun cookie

    ASSERT(transid.id[0] == STUN_COOKIE_B1);
    ASSERT(transid.id[1] == STUN_COOKIE_B2);
    ASSERT(transid.id[2] == STUN_COOKIE_B3);
    ASSERT(transid.id[3] == STUN_COOKIE_B4);

    if (_address.addr.sa_family == AF_INET)
    {
        _address.addr4.sin_port = _address.addr4.sin_port ^ htons(STUN_XOR_PORT_COOKIE);
        _address.addr4.sin_addr.s_addr = _address.addr4.sin_addr.s_addr ^ htonl(STUN_COOKIE);
    }
    else
    {
        _address.addr6.sin6_port = _address.addr6.sin6_port ^ htons(STUN_XOR_PORT_COOKIE);

        uint8_t* ip6 = (uint8_t*)&(_address.addr6.sin6_addr);

        for (int x = 0; x < STUN_IPV6_LENGTH; x++)
        {
            ip6[x] = ip6[x] ^ transid.id[x];
        }
    }
}


const sockaddr* CSocketAddress::GetSockAddr() const
{
    return &_address.addr;
}

socklen_t CSocketAddress::GetSockAddrLength() const
{
    if (_address.addr.sa_family == AF_INET)
    {
        return sizeof(_address.addr4);
    }
    else
    {
        return sizeof(_address.addr6);
    }
}

bool CSocketAddress::IsSameIP(const CSocketAddress& other) const
{
    bool fRet = false;



    if (_address.addr.sa_family == other._address.addr.sa_family)
    {
        if (_address.addr.sa_family == AF_INET)
        {
            fRet = !memcmp(&_address.addr4.sin_addr, &other._address.addr4.sin_addr, sizeof(_address.addr4.sin_addr));
        }
        else if (_address.addr.sa_family == AF_INET6)
        {
            fRet = !memcmp(&_address.addr6.sin6_addr, &other._address.addr6.sin6_addr, sizeof(_address.addr6.sin6_addr));
        }
        else
        {
            ASSERT(false); // comparing an address that is neither IPV4 or IPV6?
            fRet = !memcmp(&_address.addr.sa_data, &other._address.addr.sa_data, sizeof(_address.addr.sa_data));
        }
    }

    return fRet;
}

bool CSocketAddress::IsSameIP_and_Port(const CSocketAddress& other) const
{
    return (IsSameIP(other) && (GetPort() == other.GetPort()) );
}


bool CSocketAddress::IsIPAddressZero() const
{
    const static uint8_t ZERO_ARRAY[16] = {}; // zero init
    bool fRet = false;

    if (_address.addr.sa_family == AF_INET)
    {
        fRet = !memcmp(&_address.addr4.sin_addr, ZERO_ARRAY, sizeof(_address.addr4.sin_addr));
    }
    else if (_address.addr.sa_family == AF_INET6)
    {
        fRet = !memcmp(&_address.addr6.sin6_addr, ZERO_ARRAY, sizeof(_address.addr6.sin6_addr));
    }
    else
    {
        ASSERT(false); // comparing an address that is neither IPV4 or IPV6?
        fRet = !memcmp(&_address.addr.sa_data, ZERO_ARRAY, sizeof(_address.addr.sa_data));
    }

    return fRet;
}


void CSocketAddress::ToString(std::string* pStr) const
{
    char sz[INET6_ADDRSTRLEN + 6];
    ToStringBuffer(sz, ARRAYSIZE(sz));
    *pStr = sz;
}
HRESULT CSocketAddress::ToStringBuffer(char* pszAddrBytes, size_t length) const
{
    HRESULT hr = S_OK;
    int family = GetFamily();
    const void *pAddrBytes = NULL;
    const char* pszResult = NULL;
    const size_t portLength = 6; // colon plus 5 digit string e.g. ":55555"
    char szPort[portLength+1];


    ChkIfA(pszAddrBytes == NULL, E_INVALIDARG);
    ChkIf(length <= 0, E_INVALIDARG);
    pszAddrBytes[0] = 0;

    if (family == AF_INET)
    {
        pAddrBytes = &(_address.addr4.sin_addr);
        ChkIf(length < (INET_ADDRSTRLEN+portLength), E_INVALIDARG);
    }
    else if (family == AF_INET6)
    {
        pAddrBytes = &(_address.addr6.sin6_addr);
        ChkIf(length < (INET6_ADDRSTRLEN+portLength), E_INVALIDARG);
    }
    else
    {
        ChkA(E_FAIL);
    }

    pszResult = ::inet_ntop(family, pAddrBytes, pszAddrBytes, length);

    ChkIf(pszResult == NULL, ERRNOHR);

    sprintf(szPort, ":%d", GetPort());
#if DEBUG
    ChkIfA(strlen(szPort) > portLength, E_FAIL);
#endif

    strcat(pszAddrBytes, szPort);

Cleanup:
    return hr;
}



