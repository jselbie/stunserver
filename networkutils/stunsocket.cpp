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
#include "stunsocket.h"

CStunSocket::~CStunSocket()
{
    Close();
}

void CStunSocket::Close()
{
    if (_sock != -1)
    {
        close(_sock);
        _addrlocal = CSocketAddress(0,0);
    }
}

int CStunSocket::GetSocketHandle() const
{
    return _sock;
}

const CSocketAddress& CStunSocket::GetLocalAddress() const
{
    return _addrlocal;
}

SocketRole CStunSocket::GetRole() const
{
    ASSERT(_sock != -1);
    
    return _role;
}

HRESULT CStunSocket::EnablePktInfoOption(bool fEnable)
{
    int enable = fEnable?1:0;
    int ret;
    
    int family = _addrlocal.GetFamily();
    int level =  (family==AF_INET) ? IPPROTO_IP : IPPROTO_IPV6;

// if you change the ifdef's below, make sure you it's matched with the same logic in recvfromex.cpp
#ifdef IP_PKTINFO
    int option = (family==AF_INET) ? IP_PKTINFO : IPV6_RECVPKTINFO;
#elif defined(IP_RECVDSTADDR)
    int option = (family==AF_INET) ? IP_RECVDSTADDR : IPV6_PKTINFO;
#else
    int fail[-1]; // set a compile time assert
#endif

    ret = ::setsockopt(_sock, level, option, &enable, sizeof(enable));
    
    // Linux documentation (man ipv6) says you are supposed to set IPV6_PKTINFO as the option
    // Yet, it's really IPV6_RECVPKTINFO.  Other operating systems might expect IPV6_PKTINFO.
    // We'll cross that bridge, when we get to it.
    // todo - we should write a unit test that tests the packet info behavior
    ASSERT(ret == 0);
    
    return (ret == 0) ? S_OK : ERRNOHR;
}

//static
HRESULT CStunSocket::Create(const CSocketAddress& addrlocal, SocketRole role, boost::shared_ptr<CStunSocket>* pStunSocketShared)
{
    int sock = -1;
    int ret;
    CStunSocket* pSocket = NULL;
    sockaddr_storage addrBind = {};
    socklen_t sizeaddrBind;
    HRESULT hr = S_OK;
    
    ChkIfA(pStunSocketShared == NULL, E_INVALIDARG);
    
    sock = socket(addrlocal.GetFamily(), SOCK_DGRAM, 0);
    ChkIf(sock < 0, ERRNOHR);
    
    ret = bind(sock, addrlocal.GetSockAddr(), addrlocal.GetSockAddrLength());
    ChkIf(ret < 0, ERRNOHR);
    
    // call get sockname to find out what port we just binded to.  (Useful for when addrLocal.port is 0)
    sizeaddrBind = sizeof(addrBind);
    ret = ::getsockname(sock, (sockaddr*)&addrBind, &sizeaddrBind);
    ChkIf(ret < 0, ERRNOHR);
    
    pSocket = new CStunSocket();
    pSocket->_sock = sock;
    pSocket->_addrlocal = CSocketAddress(*(sockaddr*)&addrBind);
    pSocket->_role = role;
    sock = -1;
    
    {
        boost::shared_ptr<CStunSocket> spTmp(pSocket);
        pStunSocketShared->swap(spTmp);
    }
    
    
Cleanup:

    if (sock != -1)
    {
        close(sock);
        sock = -1;
    }

    return hr;
}


