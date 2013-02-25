
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

#define IS_DIVISIBLE_BY(x, y)  ((x % y)==0)

static unsigned int FindPrime(unsigned int val)
{
    unsigned int result = val;
    bool fPrime = false;
    
    if (val <= 2)
    {
        return 2;
    }
    
    if (val == 3)
    {
        return 3;
    }
    
    if ((result % 2)==0)
    {
        result++;
    }
    
    while (fPrime == false)
    {
        unsigned int stop = (unsigned int)(sqrt(result)+1);
        fPrime = true;
        // test to see if result is prime, if it is, return it
        for (unsigned int x = 3; x <= stop; x++)
        {
            if (IS_DIVISIBLE_BY(result, x))            
            {
                fPrime = false;
                break;
            }
        }
        if (fPrime==false)
        {
            result += 2;
        }
    }
    
    return result;
}

size_t FastHash_GetHashTableWidth(unsigned int maxConnections)
{
    // find highest prime, greater than or equal to maxConnections
    return FindPrime(maxConnections);
}

