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



#ifndef STUN_SOCK_ADDR_H
#define STUN_SOCK_ADDR_H

#include "stuntypes.h"

union simple_sockaddr
{
    sockaddr addr;
    sockaddr_in addr4;
    sockaddr_in6 addr6;
};


class CSocketAddress
{
    simple_sockaddr _address;

    size_t GetIPImpl(void* pAddr, size_t length, bool fNetworkByteOrder) const; // returns the number of bytes copied, or Zero on error


public:
    CSocketAddress();

    CSocketAddress(const sockaddr& addr);
    CSocketAddress(const sockaddr_storage& addr);
    void CommonConstructor(const sockaddr& addr);

    CSocketAddress(const sockaddr_in& addr);
    CSocketAddress(const sockaddr_in6& addr6);
    
    // both IP and PORT are passed in HOST BYTE ORDER 
    CSocketAddress(uint32_t ipHostByteOrder, uint16_t port);


    uint16_t GetPort() const;
    uint16_t GetPort_NBO() const; // network byte order
    void SetPort(uint16_t);

    uint16_t GetIPLength() const; // either returns 4 or 16 for IPV4 and IPV6 respectively

    // not sure if IPv6 has a logical "network byte order" that's different from it's normal nomenclature
    size_t GetIP(void* pAddr, size_t length) const; // returns the number of bytes copied, or Zero on error
    size_t GetIP_NBO(void* pAddr, size_t length) const; // returns the number of bytes copied, or Zero on error

    uint16_t GetFamily() const;

    void ApplyStunXorMap(const StunTransactionId& id);

    const sockaddr* GetSockAddr() const;
    socklen_t GetSockAddrLength() const;

    bool IsIPAddressZero() const;


    bool IsSameIP(const CSocketAddress& other) const;
    bool IsSameIP_and_Port(const CSocketAddress& other) const;

    void ToString(std::string* pStr) const;
    HRESULT ToStringBuffer(char* pszAddrBytes, size_t length) const;
    
    static HRESULT GetLocalHost(uint16_t family, CSocketAddress* pAddr);


};



#endif
