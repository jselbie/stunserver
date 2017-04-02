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



// About the "packet info option"
// What we are trying to do is enable the socket to be able to provide the "destination address"
// for packets we receive.  However, Linux, BSD, and MacOS all differ in what the
// socket option is. And it differs even differently between IPV4 and IPV6 across these operating systems.
// So we have the "try one or the other" implementation based on what's DEFINED
// On some operating systems, there's only one option defined. Other's have both, but only one works!
// So we have to try them both

HRESULT CStunSocket::EnablePktInfoImpl(int level, int option1, int option2, bool fEnable)
{
    HRESULT hr = S_OK;
    int enable = fEnable?1:0;
    int ret = -1;
    
    
    ChkIfA((option1 == -1) && (option2 == -1), E_FAIL);
    
    if (option1 != -1)
    {
        ret = setsockopt(_sock, level, option1, &enable, sizeof(enable));
    }
    
    if ((ret < 0) && (option2 != -1))
    {
        enable = fEnable?1:0;
        ret = setsockopt(_sock, level, option2, &enable, sizeof(enable));
    }
    
    ChkIfA(ret < 0, ERRNOHR);
    
Cleanup:
    return hr;
}

HRESULT CStunSocket::EnablePktInfo_IPV4(bool fEnable)
{
    int level = IPPROTO_IP;
    int option1 = -1;
    int option2 = -1;
    
#ifdef IP_PKTINFO
    option1 = IP_PKTINFO;
#endif
    
#ifdef IP_RECVDSTADDR
    option2 = IP_RECVDSTADDR;
#endif
    
    return EnablePktInfoImpl(level, option1, option2, fEnable);
}

HRESULT CStunSocket::EnablePktInfo_IPV6(bool fEnable)
{
    int level = IPPROTO_IPV6;
    int option1 = -1;
    int option2 = -1;
    
#ifdef IPV6_RECVPKTINFO
    option1 = IPV6_RECVPKTINFO;
#endif
    
#ifdef IPV6_PKTINFO
    option2 = IPV6_PKTINFO;
#endif
    
    return EnablePktInfoImpl(level, option1, option2, fEnable);
}

HRESULT CStunSocket::EnablePktInfoOption(bool fEnable)
{
    int family = _addrlocal.GetFamily();
    HRESULT hr;
    
    if (family == AF_INET)
    {
        hr = EnablePktInfo_IPV4(fEnable);
    }
    else
    {
        hr = EnablePktInfo_IPV6(fEnable);
    }
    
    return hr;
}

HRESULT CStunSocket::SetV6Only(int sock)
{
    int optname = -1;
    int result = 0;
    HRESULT hr = S_OK;
    int enabled = 1;
    
#ifdef IPV6_BINDV6ONLY
    optname = IPV6_BINDV6ONLY;
#elif IPV6_V6ONLY
    optname = IPV6_V6ONLY;
#else
    return E_NOTIMPL;
#endif
    
    result = setsockopt(sock, IPPROTO_IPV6, optname, (char *)&enabled, sizeof(enabled));
    hr = (result == 0) ? S_OK : ERRNOHR ;
         
    return hr;
}

HRESULT CStunSocket::SetNonBlocking(bool fEnable)
{
    HRESULT hr = S_OK;
    int result;
    int flags;
    
    flags = ::fcntl(_sock, F_GETFL, 0);
    
    ChkIf(flags == -1, ERRNOHR);

    if (fEnable)
    {    
        flags |= O_NONBLOCK;
    }
    else
    {
        flags &= ~(O_NONBLOCK);
    }
    
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



HRESULT CStunSocket::InitCommon(int socktype, const CSocketAddress& addrlocal, SocketRole role, bool fSetReuseFlag)
{
    int sock = -1;
    int ret;
    HRESULT hr = S_OK;
    
    ASSERT((socktype == SOCK_DGRAM)||(socktype==SOCK_STREAM));
    
    sock = socket(addrlocal.GetFamily(), socktype, 0);
    ChkIf(sock < 0, ERRNOHR);
    
    if (addrlocal.GetFamily() == AF_INET6)
    {
        // Don't allow IPv6 socket to receive binding request from IPv4 client
        // Because if we don't then an IPv4 client will get an IPv6 mapped address in the binding response
        // I'm pretty sure you have to call this before bind()
        // Intentionally ignoring result
        (void)SetV6Only(sock);
    }
    
    if (fSetReuseFlag)
    {
        int fAllow = 1;
        ret = ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &fAllow, sizeof(fAllow));
        ChkIf(ret == -1, ERRNOHR);
    }
    
    ret = bind(sock, addrlocal.GetSockAddr(), addrlocal.GetSockAddrLength());
    ChkIf(ret == -1, ERRNOHR);
    
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



HRESULT CStunSocket::UDPInit(const CSocketAddress& local, SocketRole role, bool fSetReuseFlag)
{
    return InitCommon(SOCK_DGRAM, local, role, fSetReuseFlag);
}

HRESULT CStunSocket::TCPInit(const CSocketAddress& local, SocketRole role, bool fSetReuseFlag)
{
    return InitCommon(SOCK_STREAM, local, role, fSetReuseFlag);
}


