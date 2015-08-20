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
#include "socketaddress.h"
#include "stringhelper.h"


HRESULT ResolveHostName(const char* pszHostName, int family, bool fNumericOnly, CSocketAddress* pAddr)
{

    int ret;
    HRESULT hr = S_OK;
    addrinfo* pResultList = NULL;
    
    addrinfo hints = {};
    
    std::string strHostName(pszHostName);
    StringHelper::Trim(strHostName);
    
    ChkIf(strHostName.length() == 0, E_INVALIDARG);
    ChkIf(pAddr==NULL, E_INVALIDARG);
    
    hints.ai_family = family;
    if (fNumericOnly)
    {
        hints.ai_flags = AI_NUMERICHOST;
    }
    
    // without a socktype hint, getaddrinfo will return 3x the number of addresses (SOCK_STREAM, SOCK_DGRAM, and SOCK_RAW)
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo(strHostName.c_str(), NULL, &hints, &pResultList);
    
    ChkIf(ret != 0, ERRNO_TO_HRESULT(ret));
    ChkIf(pResultList==NULL, E_FAIL)
    
    // just pick the first one found
    *pAddr = CSocketAddress(*(pResultList->ai_addr));
    
Cleanup:

    if (pResultList != NULL) // android will crash if passed NULL
    {
        ::freeaddrinfo(pResultList);
    }

    return hr;

}

HRESULT NumericIPToAddress(int family, const char* pszIP, CSocketAddress* pAddr)
{
    HRESULT hr = S_OK;
    
    ChkIf((family != AF_INET) && (family != AF_INET6), E_INVALIDARG);

    if (family == AF_INET)
    {
        sockaddr_in addr4 = {};
        ChkIf(0 == ::inet_pton(family, pszIP, &addr4.sin_addr), E_FAIL);
        addr4.sin_family = family;
        *pAddr = CSocketAddress(addr4);
    }
    else
    {
        sockaddr_in6 addr6 = {};
        ChkIf(0 == ::inet_pton(family, pszIP, &addr6.sin6_addr), E_FAIL);
        addr6.sin6_family = family;
        *pAddr = CSocketAddress(addr6);
    }
    
Cleanup:

    return hr;
    
}




