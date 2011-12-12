/* 
 * File:   main.cpp
 * Author: jselbie
 *
 * Created on December 3, 2011, 11:18 PM
 */

#ifndef FASTHASH_H
#define FASTHASH_H

// FastHash is a cheap and dirty hash table template class
//    It is "fast" because it never allocates memory beyond the static arrays inside each instance
//    Hence, it can be used off the stack or in cases where memory allocations impact performance
//    Limitations:
//        Fixed number of insertions (specified by FSIZE)
//        Does not support removals
//        Made for simple types and structs of simple types - as items are pre-allocated (no regards to constructors or destructors)
//        Duplicate key insertions will not remove the previous item
//    Additional:
//        FastHash keeps a static array of items inserted (in insertion order)
//        Then a hash table of <K,int> to map keys back to index values
//        This allows calling code to be able to iterate over the table in insertion order
//    Template parameters
//        K = key type
//        V = value type
//        FSIZE = max number of items in the hash table (default is 100)
//        TSIZE = hash table width (higher value reduces collisions, but with extra memory overhead - default is 37).  Usually a prime number.

inline size_t FastHash_Hash(unsigned int x)
{
    return (size_t)x;
}

inline size_t FastHash_Hash(signed int x)
{
    return (size_t)x;
}

const size_t FAST_HASH_DEFAULT_CAPACITY = 100;
const size_t FASH_HASH_DEFAULT_TABLE_SIZE = 37;

template <class K, class V, size_t FSIZE=FAST_HASH_DEFAULT_CAPACITY, size_t TSIZE=FASH_HASH_DEFAULT_TABLE_SIZE>
class FastHash
{
private:
    struct ItemNode
    {
        K key;
        int index;    // index into _list where this item is stored
        ItemNode* pNext;
    };
    
    
    V _list[FSIZE];   // list of items
    size_t _count;    // number of items inserted so far
    
    ItemNode _tablenodes[FSIZE];
    ItemNode* _table[TSIZE];
    
public:
    
    FastHash()
    {
        Reset();
    }
    
    void Reset()
    {
        _count = 0;
        memset(_table, '\0', sizeof(_table));
    }
    
    size_t Size()
    {
        return _count;
    }
    
    
    HRESULT Insert(K key, const V& val)
    {
        size_t tableindex = FastHash_Hash(key) % TSIZE;
        
        if (_count >= FSIZE)
        {
            return false;
        }
        
        _list[_count] = val;
        
        _tablenodes[_count].index = _count;
        _tablenodes[_count].key = key;
        _tablenodes[_count].pNext = _table[tableindex];
        _table[tableindex] = &_tablenodes[_count];
        
        _count++;
        
        return true;
    }
    
    V* Lookup(K key, int* pIndex=NULL)
    {
        size_t tableindex = FastHash_Hash(key) % TSIZE;
        
        V* pFoundItem = NULL;
        
        ItemNode* pHead = _table[tableindex];
        
        if (pIndex)
        {
            *pIndex = -1;
        }
        
        while (pHead)
        {
            if (pHead->key == key)
            {
                pFoundItem = &_list[pHead->index];
                
                if (pIndex)
                {
                    *pIndex = pHead->index;
                }
                
                break;
            }
            pHead = pHead->pNext;
        }
        
        return pFoundItem;
    }
    
    bool Exists(K key)
    {
        V* pItem = Lookup(key);
        return (pItem != NULL);
    }
    
    V* GetItemByIndex(int index)
    {
        if ((index < 0) || (((size_t)index) >= _count))
        {
            return NULL;
        }
        
        return &_list[index];
    }
};


#endif