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
#include "refcountobject.h"

int AtomicIncrement(int* pInt)
{
    // InterlockedIncrement
    return __sync_add_and_fetch(pInt, 1);
}

int AtomicDecrement(int* pInt)
{
    // InterlockedDecrement
    return __sync_sub_and_fetch(pInt, 1);
}


CBasicRefCount::CBasicRefCount()
{
    m_nRefs = 0;
}

CBasicRefCount::~CBasicRefCount()
{
    ;
}

int CBasicRefCount::InternalAddRef()
{
    return AtomicIncrement(&m_nRefs);
}

int CBasicRefCount::InternalRelease()
{
    int refcount = AtomicDecrement(&m_nRefs);
    if (refcount == 0)
    {
        OnFinalRelease();
    }
    return refcount;
}

void CBasicRefCount::OnFinalRelease()
{
    delete this;
}



