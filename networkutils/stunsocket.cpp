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

CStunSocket::CStunSocket() :
_sock(-1),
_role(RolePP)
{
    
}

CStunSocket::~CStunSocket()
{
    Close();
}

void CStunSocket::Reset()
{
    _sock = -1;
    _addrlocal = CSocketAddress(0,0);
    _addrremote = CSocketAddress(0,0);
    _role = RolePP;
}

void CStunSocket::Close()
{
    if (_sock != -1)
    {
        close(_sock);
        _sock = -1;
    }
    Reset();
}

bool CStunSocket::IsValid()
{
    return (_sock != -1);
}

HRESULT CStunSocket::Attach(int sock)
{
    if (sock == -1)
    {
        ASSERT(false);
        return E_INVALIDARG;
    }
    
    if (sock != _sock)
    {
        // close any existing socket
        Close(); // this will also call "Reset"
        _sock = sock;
    }
    
    UpdateAddresses();
    return S_OK;
}

int CStunSocket::Detach()
{
    int sock = _sock;
    Reset();
    return sock;
}

int CStunSocket::GetSocketHandle() const
{
    return _sock;
}

const CSocketAddress& CStunSocket::GetLocalAddress() const
{
    return _addrlocal;
}

const CSocketAddress& CStunSocket::GetRemoteAddress() const
{
    return _addrremote;
}


SocketRole CStunSocket::GetRole() const
{
    ASSERT(_sock != -1);
    return _role;
}

void CStunSocket::SetRole(SocketRole role)
{
    _role = role;
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

HRESULT CStunSocket::SetNonBlocking(bool fEnable)
{
    HRESULT hr = S_OK;
    int result;
    int flags;
    
    flags = ::fcntl(_sock, F_GETFL, 0);
    
    ChkIf(flags == -1, ERRNOHR);
    
    flags |= O_NONBLOCK;
    
    result = fcntl(_sock , F_SETFL , flags);
    
    ChkIf(result == -1, ERRNOHR);
    
Cleanup:
    return hr;
}

void CStunSocket::UpdateAddresses()
{
    sockaddr_storage addrLocal = {};
    sockaddr_storage addrRemote = {};
    socklen_t len;
    int ret;
    
    ASSERT(_sock != -1);
    if (_sock == -1)
    {
        return;
    }
    
    
    len = sizeof(addrLocal);
    ret = ::getsockname(_sock, (sockaddr*)&addrLocal, &len);
    if (ret != -1)
    {
        _addrlocal = addrLocal;
    }
    
    len = sizeof(addrRemote);
    ret = ::getpeername(_sock, (sockaddr*)&addrRemote, &len);
    if (ret != -1)
    {
        _addrremote = addrRemote;
    }
}



HRESULT CStunSocket::InitCommon(int socktype, const CSocketAddress& addrlocal, SocketRole role)
{
    int sock = -1;
    int ret;
    HRESULT hr = S_OK;
    
    ASSERT((socktype == SOCK_DGRAM)||(socktype==SOCK_STREAM));
    
    sock = socket(addrlocal.GetFamily(), socktype, 0);
    ChkIf(sock < 0, ERRNOHR);
    
    
    
    ret = bind(sock, addrlocal.GetSockAddr(), addrlocal.GetSockAddrLength());
    ChkIf(ret < 0, ERRNOHR);
    
    Attach(sock);
    sock = -1;
    
    SetRole(role);
    
Cleanup:
    if (sock != -1)
    {
        close(sock);
        sock = -1;
    }
    return hr;
}



HRESULT CStunSocket::UDPInit(const CSocketAddress& local, SocketRole role)
{
    return InitCommon(SOCK_DGRAM, local, role);
}

HRESULT CStunSocket::TCPInit(const CSocketAddress& local, SocketRole role)
{
    return InitCommon(SOCK_STREAM, local, role);
}


