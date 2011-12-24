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
    HRESULT hr = S_OK;
    
    ChkA(TestFastHash());
    ChkA(TestRemove());
    
Cleanup:
    return hr;
}

HRESULT CTestFastHash::TestFastHash()
{
    HRESULT hr = S_OK;
    
    const size_t c_maxsize = 500;
    const size_t c_tablesize = 91;
    FastHash<int, Item, c_maxsize, c_tablesize> hash;
    int result;
    size_t testindex;
    
    for (int index = 0; index < (int)c_maxsize; index++)
    {
        Item item;
        item.key = index;
        
        result = hash.Insert(index, item);
        ChkIfA(result < 0,E_FAIL);
    }
    
    // now make sure that we can't insert one past the limit
    {
        Item item;
        item.key = c_maxsize;
        result = hash.Insert(item.key, item);
        ChkIfA(result >= 0, E_FAIL);
    }
    
    // check that the size is what's expected
    ChkIfA(hash.Size() != c_maxsize, E_FAIL);
    
    // validate that all the items are in the table
    for (int index = 0; index < (int)c_maxsize; index++)
    {
        Item* pItem = NULL;
        
        ChkIfA(hash.Exists(index)==false, E_FAIL);
        
        pItem = hash.Lookup(index);
        
        ChkIfA(pItem == NULL, E_FAIL);
        ChkIfA(pItem->key != index, E_FAIL);
    }
    
    // validate that items aren't in the table don't get returned
    for (int index = c_maxsize; index < (int)(c_maxsize*2); index++)
    {
        ChkIfA(hash.Exists(index), E_FAIL);
        ChkIfA(hash.Lookup(index)!=NULL, E_FAIL);
    }
    
    // test a basic remove
    testindex = c_maxsize/2;
    result = hash.Remove(testindex);
    ChkIfA(result < 0, E_FAIL);
    
    // now add another item
    {
        Item item;
        item.key = c_maxsize;
        result = hash.Insert(item.key, item);
        ChkIfA(result < 0, E_FAIL);
    }
    
Cleanup:
    return hr;
    
}

HRESULT CTestFastHash::TestRemove()
{
    HRESULT hr = S_OK;
    int result;
    const size_t c_maxsize = 500;
    const size_t c_tablesize = 91;
    FastHash<int, Item, c_maxsize, c_tablesize> hash;

    // add 500 items
    for (int index = 0; index < (int)c_maxsize; index++)
    {
        Item item;
        item.key = index;
        
        result = hash.Insert(index, item);
        ChkIfA(result < 0,E_FAIL);
    }
    
    // now remove them all
    for (int index = 0; index < (int)c_maxsize; index++)
    {
        result = hash.Remove(index);
        ChkIfA(result < 0,E_FAIL);
    }
    
    ChkIfA(hash.Size() != 0, E_FAIL);
    
    // Now add all the items back
    for (int index = 0; index < (int)c_maxsize; index++)
    {
        Item item;
        item.key = index;
        
        result = hash.Insert(index, item);
        ChkIfA(result < 0,E_FAIL);
    }
    
    ChkIfA(hash.Size() != c_maxsize, E_FAIL);
    
    
    
   
Cleanup:
    return hr;

}