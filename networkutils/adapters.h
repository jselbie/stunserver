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



#ifndef ADAPTERS_H
#define	ADAPTERS_H

#include "socketaddress.h"


bool HasAtLeastTwoAdapters(int family);
HRESULT GetBestAddressForSocketBind(bool fPrimary, int family, uint16_t port, CSocketAddress* pSocketAddr);
HRESULT GetSocketAddressForAdapter(int family, const char* pszAdapterName, uint16_t port, CSocketAddress* pSocketAddr);



#endif	/* ADAPTERS_H */

