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



#ifndef STUN_UTILS_H
#define STUN_UTILS_H


bool IsTransactionIdValid(StunTransactionId& transid);
HRESULT GetXorMappedAddress(uint8_t* pData, size_t size, StunTransactionId &transid, CSocketAddress* pAddr);
HRESULT GetMappedAddress(uint8_t* pData, size_t size, CSocketAddress* pAddr);

std::string NatBehaviorToString(NatBehavior behavior);
std::string NatFilteringToString(NatFiltering filtering);

#endif
