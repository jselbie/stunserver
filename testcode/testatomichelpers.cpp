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

#include "commonincludes.hpp"
#include "testatomichelpers.h"
#include "atomichelpers.h"


HRESULT CTestAtomicHelpers::Run()
{
    HRESULT hr = S_OK;
    
    int value = -2000;
    int nextexpected = -2000;
    int result = 0;
    
    while (value < 2000)
    {
        nextexpected++;
        result = AtomicIncrement(&value);
        ChkIf(result != nextexpected, E_UNEXPECTED);
        ChkIf(result != value, E_UNEXPECTED);
    }
    
    value = 2000;
    nextexpected = 2000;
    while (value > -2000)
    {
        nextexpected--;
        result = AtomicDecrement(&value);
        ChkIf(result != nextexpected, E_UNEXPECTED);
        ChkIf(result != value, E_UNEXPECTED);
    }

Cleanup:
    return hr;
}