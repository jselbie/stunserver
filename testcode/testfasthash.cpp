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



HRESULT CTestFastHash::AddOne(int val)
{
    HRESULT hr = S_OK;
    Item item;
    item.key = val;
    int ret;
    Item* pValue = NULL;

    ret = _hashtable.Insert(val, item);
    ChkIf(ret < 0, E_FAIL);
    
    ChkIf(_hashtable.Exists(val)==false, E_FAIL);
    
    pValue = _hashtable.Lookup(val);
    ChkIf(pValue == NULL, E_FAIL);
    ChkIf(pValue->key != val, E_FAIL);
    
    
    
Cleanup:
    return hr;
}

HRESULT CTestFastHash::RemoveOne(int val)
{
    HRESULT hr = S_OK;
    int ret;
    
    ret = _hashtable.Remove(val);
    ChkIf(ret < 0, E_FAIL);
    
    ChkIf(_hashtable.Exists(val), E_FAIL);
    
    ChkIf(_hashtable.Lookup(val) != NULL, E_FAIL);
    
Cleanup:
    return hr;
    
}


HRESULT CTestFastHash::AddRangeToSet(int first, int last)
{
    HRESULT hr = S_OK;
    
    for (int x = first; x <= last; x++)   
    {
        Chk(AddOne(x));
    }
    
Cleanup:
    return hr;
}

HRESULT CTestFastHash::RemoveRangeFromSet(int first, int last)
{
    HRESULT hr = S_OK;
    
    for (int x = first; x <= last; x++)   
    {
        Chk(RemoveOne(x));
    }
    
Cleanup:
    return hr;
}

HRESULT CTestFastHash::ValidateRangeInSet(int first, int last)
{
    HRESULT hr = S_OK;
    
    for (int x = first; x <= last; x++)   
    {
        Item* pValue = NULL;
        
        ChkIf(_hashtable.Exists(x)==false, E_FAIL);
        
        pValue = _hashtable.Lookup(x);
        ChkIf(pValue == NULL, E_FAIL);
        ChkIf(pValue->key != x, E_FAIL);
    }
    
Cleanup:
    return hr;
}

HRESULT CTestFastHash::ValidateRangeNotInSet(int first, int last)
{
    HRESULT hr = S_OK;
    
    for (int x = first; x <= last; x++)   
    {
        ChkIf(_hashtable.Lookup(x) != NULL, E_FAIL);
        ChkIf(_hashtable.Exists(x), E_FAIL);
    }
    
Cleanup:
    return hr;
}

HRESULT CTestFastHash::ValidateRangeInIndex(int first, int last)
{
    HRESULT hr = S_OK;
    const int length = last - first + 1;
    bool* arr = new bool[length];
    size_t size = _hashtable.Size();
    
    memset(arr, '\0', length);
    
    for (int x = 0; x < (int)size; x++)
    {
        Item* pItem = _hashtable.LookupValueByIndex(x);
        
        if (pItem == NULL)
        {
            continue;
        }
        
        int val = pItem->key;
        
        if ((val >= first) && (val <= last))
        {
            int index = val - first;
            ChkIfA(arr[index] != false, E_FAIL);
            arr[index] = true;
        }
    }
    
    for (int i = 0; i < length; i++)
    {
        ChkIfA(arr[i] == false, E_FAIL);
    }

    
Cleanup:
    delete [] arr;
    return hr;
}



HRESULT CTestFastHash::TestFastHash()
{
    HRESULT hr = S_OK;
    HRESULT hrRet = S_OK;

    _hashtable.Reset();
    
    ChkA(AddRangeToSet(1, c_maxsize));
    
    // now make sure that we can't insert one past the limit
    {
        hrRet = AddOne(c_maxsize+1);
        ChkIfA(SUCCEEDED(hrRet), E_FAIL);
    }
    
    // check that the size is what's expected
    ChkIfA(_hashtable.Size() != c_maxsize, E_FAIL);
    
    // validate that all the items are in the table
    ChkA(ValidateRangeInSet(1, c_maxsize));
    
    // validate items not inserted don't get returned
    ChkA(ValidateRangeNotInSet(c_maxsize+1, c_maxsize*2));
    
    ChkA(ValidateRangeInIndex(1, c_maxsize));
    
    // test a basic remove
    ChkA(RemoveOne(c_maxsize/2));
    
    // revalidate that the index is ok
    ChkA(ValidateRangeInIndex(1, c_maxsize/2-1));    
    ChkA(ValidateRangeInIndex(c_maxsize/2+1, c_maxsize));    
    
    // now add another item
    ChkA(AddOne(c_maxsize+1));
    
    // check that the size is what's expected
    ChkIfA(_hashtable.Size() != c_maxsize, E_FAIL);
    
    
    
    
Cleanup:
    return hr;
    
}

HRESULT CTestFastHash::TestRemove()
{
    HRESULT hr = S_OK;
    int tracking[c_maxsize] = {};
    size_t expected;
    FastHashBase<int, Item>::Item* pItem;
    
    for (size_t x = 0; x < c_maxsize; x++)
    {
        tracking[x] = (int)x;
    }
    
    // shuffle our array - this is the order in which we'll do removes
    srand(99);
    for (size_t x = 0; x < c_maxsize; x++)
    {
        int firstindex = rand() % c_maxsize;
        int secondindex = rand() % c_maxsize;
        
        int val1 = tracking[firstindex];
        int val2 = tracking[secondindex];
        int tmp;
        
        tmp = val1;
        val1 = val2;
        val2 = tmp;
        
        tracking[firstindex] = val1;
        tracking[secondindex] = val2;
    }
        
    
    _hashtable.Reset();
    ChkIfA(_hashtable.Size() != 0, E_FAIL);
    
    ChkA(AddRangeToSet(0, c_maxsize-1));

    // now start removing items randomly 
    for (size_t x = 0; x < (c_maxsize/2); x++)
    {
        ChkA(RemoveOne(tracking[x]));
    }
    
    // now validate that the first half of the list is gone and that the other half of the list is present
    for (size_t x = 0; x < (c_maxsize/2); x++)
    {
        ChkA(ValidateRangeNotInSet(tracking[x], tracking[x]));
    }
    
    for (size_t x = (c_maxsize/2); x < c_maxsize; x++)
    {
        ChkA(ValidateRangeInSet(tracking[x], tracking[x]));
    }
    
    
    
    expected = c_maxsize - c_maxsize/2;
    ChkIfA(_hashtable.Size() != expected, E_FAIL);
    
    // now add them all back
    for (size_t x = 0; x < (c_maxsize/2); x++)
    {
        ChkA(AddOne(tracking[x]));
    }
    
    ChkIfA(_hashtable.Size() != c_maxsize, E_FAIL);
    ChkA(ValidateRangeInSet(0, c_maxsize-1));
    
    ChkA(ValidateRangeInIndex(0, c_maxsize-1));
    
    pItem = _hashtable.LookupByIndex(0);
    ChkA(RemoveOne(pItem->key));
    
    for (size_t x = 0; x < c_maxsize; x++)
    {
        if (x == (size_t)(pItem->key))
            continue;
        
        ChkA(ValidateRangeInIndex(x,x));
    }
    
   
Cleanup:
    return hr;
}



