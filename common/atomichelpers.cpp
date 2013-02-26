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



#ifdef ATOMICS_ARE_DEFINED

extern "C" unsigned int xadd_4(volatile void* pVal, unsigned int inc)
{
    unsigned int result;
    unsigned int* pValInt = (unsigned int*)pVal;

    asm volatile( 
        "lock; xaddl %%eax, %2;"
        :"=a" (result) 
        : "a" (inc), "m" (*pValInt) 
        :"memory" );

    return (result);

}

extern "C" unsigned int __sync_add_and_fetch_4(volatile void* pVal, unsigned int inc)
{
    return (xadd_4(pVal, inc) + inc);
}

extern "C" unsigned int __sync_sub_and_fetch_4(volatile void* pVal, unsigned int inc)
{
    return (xadd_4(pVal, -inc) - inc);
}

extern "C" unsigned int __sync_fetch_and_add_4(volatile void* pVal, unsigned int inc)
{
    return xadd_4(pVal, inc);
}

extern "C" unsigned int __sync_fetch_and_sub_4(volatile void* pVal, unsigned int inc)
{
    return xadd_4(pVal, -inc);
}

#ifdef __GNUC__
    #pragma message "atomichelpers.cpp: Defining sync_add_and_fetch helpers for i386 compile"
#endif

#endif



int AtomicIncrement(int* pInt)
{
    COMPILE_TIME_ASSERT(sizeof(int)==4);   
    // InterlockedIncrement
    return __sync_add_and_fetch(pInt, 1);
}

int AtomicDecrement(int* pInt)
{
    // InterlockedDecrement
    return __sync_sub_and_fetch(pInt, 1);
}

