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

#include "commonincludes.h"
#include "testfasthash.h"
#include "fasthash.h"


HRESULT CTestFastHash::Run()
{
    return TestFastHash();
}

HRESULT CTestFastHash::TestFastHash()
{
    HRESULT hr = S_OK;
    
    const size_t c_maxsize = 500;
    FastHash<int, Item, c_maxsize> hash;
    
    for (size_t index = 0; index < c_maxsize; index++)
    {
        Item item;
        item.key = (int)index;
        ChkA(hash.Insert((int)index, item));
    }
    
    // validate that all the items are in the table
    for (size_t index = 0; index < c_maxsize; index++)
    {
        Item* pItem = NULL;
        Item* pItemDirect = NULL;
        int insertindex = -1;
        
        ChkIfA(hash.Exists(index)==false, E_FAIL);
        
        pItem = hash.Lookup((int)index, &insertindex);
        
        ChkIfA(pItem == NULL, E_FAIL);
        ChkIfA(pItem->key != (int)index, E_FAIL);
        ChkIfA((int)index != insertindex, E_FAIL);
        
        pItemDirect = hash.GetItemByIndex((int)index);
        ChkIfA(pItemDirect != pItem, E_FAIL);
    }
    
    // validate that items aren't in the table don't get returned
    for (size_t index = c_maxsize; index < (c_maxsize*2); index++)
    {
        ChkIfA(hash.Exists((int)index), E_FAIL);
        ChkIfA(hash.Lookup((int)index)!=NULL, E_FAIL);
        ChkIfA(hash.GetItemByIndex((int)index)!=NULL, E_FAIL);
    }
    
Cleanup:
    return hr;
    
}
