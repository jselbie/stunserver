/*
   Copyright 2013 John Selbie

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


#ifndef ATOMICHELPERS_H
#define	ATOMICHELPERS_H

typedef unsigned int (*xaddFunctionType)(volatile void*, unsigned int);


#if defined(i386) || defined(__i386__) || defined(__i386)
#define ATOMICS_ARE_DEFINED


extern "C" unsigned int xadd_4(volatile void* pVal, unsigned int inc);
extern "C" unsigned int __sync_add_and_fetch_4(volatile void* pVal, unsigned int inc);
extern "C" unsigned int __sync_sub_and_fetch_4(volatile void* pVal, unsigned int inc);
extern "C" unsigned int __sync_fetch_and_add_4(volatile void* pVal, unsigned int inc);
extern "C" unsigned int __sync_fetch_and_sub_4(volatile void* pVal, unsigned int inc);
#endif


int AtomicIncrement(int* pInt);
int AtomicDecrement(int* pInt);


#endif	/* ATOMICHELPERS_H */

