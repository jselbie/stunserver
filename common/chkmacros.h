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


#ifndef CHKMACROS_H
#define CHKMACROS_H

#include "hresult.h"



template <typename T>
inline HRESULT CheckCore(const T t)
{
    t.What_You_Passed_To_Chk_Is_Not_HRESULT();
    return E_FAIL;
}

template <typename T>
inline bool CheckIfCore(const T t)
{
    t.What_You_Passed_To_ChkIf_Is_Not_bool();
    return false;
}


template <>
inline HRESULT CheckCore<HRESULT>(const HRESULT t)
{
    return t;
}

template <>
inline bool CheckIfCore<bool>(const bool t)
{
    return t;
}


#define Chk(expr) \
{ \
    ((void)hr); \
    hr = CheckCore((expr)); \
    if (FAILED(hr)) \
    { \
        goto Cleanup; \
    } \
}

#define ChkIf(expr, hrerror) \
{ \
    ((void)hr); \
    if (CheckIfCore((expr))) \
    {  \
        hr = (hrerror); \
        goto Cleanup; \
    } \
}

#define ChkA(expr) \
{ \
    ((void)hr); \
    hr = CheckCore((expr)); \
    if (FAILED(hr)) \
    { \
        ASSERT(false); \
        goto Cleanup; \
    } \
}

#define ChkIfA(expr, hrerror) \
{ \
    ((void)hr); \
    if (CheckIfCore((expr))) \
    {  \
        ASSERT(false); \
        hr = (hrerror); \
        goto Cleanup; \
    } \
}

#endif
