#include "commonincludes.h"

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

