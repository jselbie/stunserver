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
//        Made for simple types and structs of simple types - as items are pre-allocated (no regards to constructors or destructors)
//        Duplicate key insertions will not remove the previous item
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


// fast hash supports basic insert and remove
template <class K, class V, size_t FSIZE=100, size_t TSIZE=37>
class FastHash
{
protected:
    struct ItemNode
    {
        K key;
        int index; // index into _nodes where value exists
        ItemNode* pNext;
        ItemNode* pPrev;
    };
    
    
    int _insertindex;
    
    V _nodes[FSIZE];
    ItemNode _itemnodes[FSIZE];
    ItemNode* _freelist;
    ItemNode* _lookuptable[TSIZE];
    
    size_t _size;
    
    ItemNode* Find(const K& key)
    {
        size_t hashindex = FastHash_Hash(key) % TSIZE;
        ItemNode* pProbe = _lookuptable[hashindex];
        while (pProbe)
        {
            if (pProbe->key == key)
            {
                break;
            }
            pProbe = pProbe->pNext;
        }
        return pProbe;
    }
public:
    FastHash()
    {
#ifdef DEBUG
        char compiletimeassert1[(FSIZE > 0)?1:-1];
        char compiletimeassert2[(TSIZE > 0)?1:-1];
        compiletimeassert1[0] = 'x';
        compiletimeassert2[0] = 'x';
#endif
        
        Reset();
    }
    
    void Reset()
    {
        memset(_lookuptable, '\0', sizeof(_lookuptable));
        for (size_t x = 0; x < FSIZE; x++)
        {
            _itemnodes[x].pNext = &_itemnodes[x+1];
            _itemnodes[x].pPrev = NULL;
            _itemnodes[x].index = x;
        }
        _itemnodes[FSIZE-1].pNext = NULL;
        _freelist = _itemnodes;
        _size = 0;
    }
    
    size_t Size()
    {
        return _size;
    }
    
    int Insert(const K& key, V& value)
    {
        size_t hashindex = FastHash_Hash(key) % TSIZE;
        ItemNode* pInsert = NULL;
        ItemNode* pHead = _lookuptable[hashindex];
        
        if (_freelist == NULL)
        {
            return -1;
        }
        
        pInsert = _freelist;
        _freelist = _freelist->pNext;
        
        _nodes[pInsert->index] = value;
        
        pInsert->key = key;
        pInsert->pPrev = NULL;
        pInsert->pNext = pHead;
        if (pHead)
        {
            pHead->pPrev = pInsert;
        }
        
        
        _lookuptable[hashindex]= pInsert;
        _size++;
                
        return 1;
    }
    int Remove(const K& key)
    {
        ItemNode* pNode = Find(key);
        ItemNode* pPrev = NULL;
        ItemNode* pNext = NULL;
        if (pNode == NULL)
        {
            return -1;
        }
        pPrev = pNode->pPrev;
        pNext = pNode->pNext;
        if (pPrev == NULL)
        {
            size_t hashindex = FastHash_Hash(key) % TSIZE;
            _lookuptable[hashindex] = pNext;
        }
        if (pPrev)
        {
            pPrev->pNext = pNext;
        }
        if (pNext)
        {
            pNext->pPrev = pPrev;
        }
        
        pNode->pPrev = NULL;
        pNode->pNext = _freelist;
        _freelist = pNode;
        
        _size--;
        
        return 1;
    }
    V* Lookup(const K& key)
    {
        V* pValue = NULL;
        ItemNode* pNode = Find(key);
        if (pNode)
        {
            pValue = &_nodes[pNode->index];
        }
        return pValue;
    }
    bool Exists(const K& key)
    {
        return (Find(key) != NULL);
    }
    

};




#endif