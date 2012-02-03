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

#ifndef TEST_FAST_HASH_H
#define TEST_FAST_HASH_H

#include "fasthash.h"
#include "unittest.h"




class CTestFastHash : public IUnitTest
{
private:

    struct Item
    {
        int key;
    };
    
    static const size_t c_maxsize = 500;
    static const size_t c_tablesize = 91;
    
    FastHash<int, Item, c_maxsize, c_tablesize> _hashtable;
    
    HRESULT AddRangeToSet(int first, int last);
    HRESULT RemoveRangeFromSet(int first, int last);
    HRESULT ValidateRangeInSet(int first, int last);
    HRESULT ValidateRangeNotInSet(int first, int last);
    HRESULT AddOne(int val);
    HRESULT RemoveOne(int val);
    HRESULT ValidateRangeInIndex(int first, int last);
    
    
    
    HRESULT TestFastHash();
    HRESULT TestRemove();
    
    HRESULT TestIndexing();
    

public:
    virtual HRESULT Run();
    UT_DECLARE_TEST_NAME("CTestFastHash");
};

#endif
