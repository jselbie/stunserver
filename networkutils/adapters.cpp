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

// TODO - FIX THIS SUCH THAT CSocketAddress is in it's own "base" library independent of networkutils and stuncore
#include "socketaddress.h"


void GetDefaultAdapters(int family, ifaddrs* pList, ifaddrs** ppAddrPrimary, ifaddrs** ppAddrAlternate)
{
    ifaddrs* pAdapter = NULL;

    *ppAddrPrimary = NULL;
    *ppAddrAlternate = NULL;

    pAdapter = pList;
    while (pAdapter)
    {
        if ( (pAdapter->ifa_addr != NULL) && (pAdapter->ifa_addr->sa_family == family) && (pAdapter->ifa_flags & IFF_UP) && !(pAdapter->ifa_flags & IFF_LOOPBACK))
        {
            if (*ppAddrPrimary == NULL)
            {
                *ppAddrPrimary = pAdapter;
            }
            else
            {
                *ppAddrAlternate = pAdapter;
                break;
            }
        }
        pAdapter = pAdapter->ifa_next;
    }
}

/**
 * Returns true if there are two or more host interfaces(adapters) for the specified family of IP addresses that are both "up" and not loopback adapters
 * @param family either AF_INET or AF_INET6
 */
bool HasAtLeastTwoAdapters(int family)
{
    HRESULT hr = S_OK;
    
    ifaddrs* pList = NULL;
    ifaddrs* pAdapter1 = NULL;
    ifaddrs* pAdapter2 = NULL;
    bool fRet = false;

    ChkIf(getifaddrs(&pList) < 0, ERRNOHR);

    GetDefaultAdapters(family, pList, &pAdapter1, &pAdapter2);

    fRet = (pAdapter1 && pAdapter2);


Cleanup:
    freeifaddrs(pList);
    return fRet;
}


/**
 * Suggests a default adapter for a given stun server socket
 * @param fPrimary - true if the returned adapter is to be used for the primary socket in a stun server. Typically passing "true" means "return the first adapter enumerated", otherwise return the second adapter enumerated"
 * @param family - Either AF_INET or AF_INET6
 * @param pSocketAddr - OUT param.  On success, contains the address to bind to
 * @return S_OK on success.  Error code otherwise.
 */
HRESULT GetBestAddressForSocketBind(bool fPrimary, int family, uint16_t port, CSocketAddress* pSocketAddr)
{
    HRESULT hr = S_OK;
    ifaddrs* pList = NULL;
    ifaddrs* pAdapter = NULL;
    ifaddrs* pAdapter1 = NULL;
    ifaddrs* pAdapter2 = NULL;

    ChkIfA(pSocketAddr == NULL, E_INVALIDARG);

    ChkIf(getifaddrs(&pList) < 0, ERRNOHR);

    GetDefaultAdapters(family, pList, &pAdapter1, &pAdapter2);

    pAdapter = fPrimary ? pAdapter1 : pAdapter2;

    ChkIf(pAdapter==NULL, E_FAIL);
    ChkIfA(pAdapter->ifa_addr==NULL, E_UNEXPECTED);

    *pSocketAddr = CSocketAddress(*pAdapter->ifa_addr);
    pSocketAddr->SetPort(port);

Cleanup:
    freeifaddrs(pList);
    return hr;
}


HRESULT GetSocketAddressForAdapter(int family, const char* pszAdapterName, uint16_t port, CSocketAddress* pSocketAddr)
{
    HRESULT hr = S_OK;
    ifaddrs* pList = NULL;
    ifaddrs* pAdapter = NULL;
    ifaddrs* pAdapterFound = NULL;


    ChkIfA(pszAdapterName == NULL, E_INVALIDARG);
    ChkIfA(pszAdapterName[0] == '\0', E_INVALIDARG);
    ChkIfA(pSocketAddr == NULL, E_INVALIDARG);



    // what if the socket address is available, but not "up".  Well, just let this call succeed.  If the server errors out, it will get cleaned up then
    ChkIf(getifaddrs(&pList) < 0, ERRNOHR);
    pAdapter = pList;
    while (pAdapter)
    {
        if ((pAdapter->ifa_addr != NULL) && (pAdapter->ifa_name != NULL) && (family == pAdapter->ifa_addr->sa_family))
        {
            if (strcmp(pAdapter->ifa_name, pszAdapterName) == 0)
            {
                pAdapterFound = pAdapter;
                break;
            }
        }
        pAdapter = pAdapter->ifa_next;
    }


    // If pszAdapterName is an IP address, convert it into a sockaddr and compare the address field with that of the adapter
    // Note: an alternative approach would be to convert pAdapter->ifa_addr to a string and then do a string compare.
    // But then it would be difficult to match "::1" with "0:0:0:0:0:0:0:1" and other formats of IPV6 strings
    if ((pAdapterFound == NULL) && ((family == AF_INET) || (family == AF_INET6)) )
    {
        uint8_t addrbytes[sizeof(in6_addr)] = {};
        int comparesize = (family == AF_INET) ? sizeof(in_addr) : sizeof(in6_addr);
        void* pCmp = NULL;


        if (inet_pton(family, pszAdapterName, addrbytes) == 1)
        {
             pAdapter = pList;
             while (pAdapter)
             {
                 if ((pAdapter->ifa_addr != NULL) && (family == pAdapter->ifa_addr->sa_family))
                 {
                     // offsetof(sockaddr_in, sin_addr) != offsetof(sockaddr_in6, sin6_addr)
                     // so you really can't do too many casting tricks like you can with sockaddr and sockaddr_in

                     if (family == AF_INET)
                     {
                         sockaddr_in *pAddr4 = (sockaddr_in*)(pAdapter->ifa_addr);
                         pCmp = &(pAddr4->sin_addr);
                     }
                     else
                     {
                         sockaddr_in6 *pAddr6 = (sockaddr_in6*)(pAdapter->ifa_addr);
                         pCmp = &(pAddr6->sin6_addr);
                     }

                     if (memcmp(pCmp, addrbytes, comparesize) == 0)
                     {
                         // match on ip address string found
                         pAdapterFound = pAdapter;
                         break;
                     }
                 }
                 pAdapter = pAdapter->ifa_next;
             }
        }
    }

    ChkIf(pAdapterFound == NULL, E_FAIL);

    {
        *pSocketAddr = CSocketAddress(*(pAdapterFound->ifa_addr));
        pSocketAddr->SetPort(port);
    }

Cleanup:

    freeifaddrs(pList);
    return hr;

}


