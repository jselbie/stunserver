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

#ifndef RESOLVEHOSTNAME_H
#define	RESOLVEHOSTNAME_H

#include "socketaddress.h"

/**
 * Converts a string contains a numeric address or FQDN into a socket address structure
 * @param pszHostName is the string of the remote host to resolve
 * @param family either AF_INET or AF_INET6
 * @param fNumericOnly Prevents DNS and just resolves numeric IP addresses (e.g. "192.168.1.2"
 * @param pAddr - OUT param.  On success is filled with a valid socket addresss information.
 * @return S_OK on success.  Error code otherwise
 */
HRESULT ResolveHostName(const char* pszHostName, int family, bool fNumericOnly, CSocketAddress* pAddr);

HRESULT NumericIPToAddress(int family, const char* pszIP, CSocketAddress* pAddr);



#endif	/* RESOLVEHOSTNAME_H */

