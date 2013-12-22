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

// FastHashDynamic is similar to FastHash, except it will "new" all the memory in the constructor and "delete" it in the destructor

// Use FastHash when the maximum number of elements in the hash table is known at compile time


size_t FastHash_GetHashTableWidth(unsigned int maxItems);


inline size_t FastHash_Hash(void* ptr)
{
    return (size_t)ptr;
}
inline size_t FastHash_Hash(unsigned int x)
{
    return (size_t)x;
}
inline size_t FastHash_Hash(signed int x)
{
    return (size_t)x;
}


template <typename K, typename V>
class FastHashBase
{
    
public:    
    struct Item
    {
        K key;
        V value;
    };
    
protected:
    
    
    struct ItemNode
    {
        int index;
        ItemNode* pNext;
    };
    
    typedef ItemNode* ItemNodePtr;
    
    size_t _fsize;  // max number of items the table can hold
    size_t _tsize;  // number of table columns (length of lookuptable)
    
    Item* _nodes;            // array of key/value pair instances, size==fsize
    ItemNode* _itemnodes; // array of ItemNode instances, size==fsize
    ItemNode* _freelist;
    ItemNodePtr* _lookuptable; // hash table (array of size tsize of ItemNode*)
    
    // for iterators and fast lookup by index
    int* _indexlist; // array of index values, size==fsize
    bool _fIndexValid;
    size_t _indexStart; // _indexList is a circular array
    
    size_t _size;
    
    // disable copy constructor and bad overloads
    FastHashBase(const FastHashBase&) {;}
    FastHashBase& operator=(const FastHashBase&) {return *this;}
    bool operator==(const FastHashBase&) {return false;}    
    
    
    
    ItemNode* Find(const K& key, size_t* pHashIndex=NULL, ItemNode** ppPrev=NULL)
    {
        size_t hashindex = ((size_t)(FastHash_Hash(key))) % _tsize;
        ItemNode* pPrev = NULL;
        ItemNode* pProbe = _lookuptable[hashindex];
        while (pProbe)
        {
            if (_nodes[pProbe->index].key == key)
            {
                break;
            }
            pPrev = pProbe;
            pProbe = pProbe->pNext;
        }
        
        if (pHashIndex)
        {
            *pHashIndex = hashindex;
        }
        
        if (ppPrev)
        {
            *ppPrev = pPrev;
        }
        
        return pProbe;
    }
    
   
    void ReIndex()
    {
        int index = 0;
        
        if ((_indexlist == NULL) || (_size == 0))
        {
            return;
        }
        
        for (size_t t = 0; t < _tsize; t++)
        {
            ItemNode* pNode = _lookuptable[t];
            while (pNode)
            {
                _indexlist[index] = pNode->index;
                index++;
                pNode = pNode->pNext;
            }
        }
        
        _fIndexValid = true;
        _indexStart = 0;
    }
    
    void UpdateIndexWithAdd(ItemNode* pNode)
    {
        // this method is called before _size is incremented
        // ASSERT( _size < _fsize)
       
        
        if (_fIndexValid && (_size < _fsize) && _indexlist)
        {
            size_t pos = (_size + _indexStart) % _fsize;
            
            _indexlist[pos] = pNode->index;
        }
    }
    
    void UpdateIndexWithRemove(ItemNode* pNode)
    {
        // this method is called before size is decremented
        // ASSERT(_size > 0)
        
        // if size is 0, then that's an error
        // if there is no indexlist, then just bail
        if ( (_size == 0) ||
             (_indexlist == NULL) ||
             ((_size > 1) && (_fIndexValid==false)))
        {
            return;
        }
        
        // the list is always valid again when the table goes empty
        if (_size == 1)
        {
            _fIndexValid = true;
            _indexStart = 0;
            return;
        }

        // If the item being removed is from the front or end of the index, then nothing to do
        if (pNode->index == _indexlist[_indexStart])
        {
            _indexStart = (_indexStart + 1) % _fsize;
            return;
        }
        
        size_t indexlast = (_indexStart + (_size-1)) % _fsize;
        if (pNode->index == _indexlist[indexlast])
        {
            return;
        }
        
        // otherwise, we're removing an item from the middle - the index is now invalid
        // I suppose we could do a memmove, but then that creates other perf issues
        _fIndexValid = false;
    }
    
    
    
public:
    
    FastHashBase()
    {
        Init(0, 0, NULL, NULL, NULL, NULL);
    }
    
    
    FastHashBase(size_t fsize, size_t tsize, Item* nodelist, ItemNode* itemnodelist, ItemNode** table, int* indexlist)
    {
        Init(fsize, tsize, nodelist, itemnodelist, table, indexlist);
    }
    
    void Init(size_t fsize, size_t tsize, Item* nodelist, ItemNode* itemnodelist, ItemNode** table, int* indexlist)
    {
        
        _fsize = fsize;
        _tsize = tsize;
        
        _nodes = nodelist;
        _itemnodes = itemnodelist;
        _freelist = NULL;
        _lookuptable = table;
        _indexlist = indexlist;
        _fIndexValid = (_indexlist != NULL);
        _indexStart = 0;
        
        Reset();
        
    }
    
    void Reset()
    {
        if (_lookuptable != NULL)
        {
            memset(_lookuptable, '\0', sizeof(ItemNodePtr)*_tsize);
        }
        
        if ((_fsize > 0) && (_itemnodes != NULL))
        {
            for (size_t x = 0; x < _fsize; x++)
            {
                _itemnodes[x].pNext = &_itemnodes[x+1];
                _itemnodes[x].index = x;
            }
        
            _itemnodes[_fsize-1].pNext = NULL;
        }
        
        _freelist = _itemnodes;
        _size = 0;
        
        _fIndexValid = (_indexlist != NULL); // index is valid when we are empty
        _indexStart = 0;
    }
    
    bool IsValid()
    {
        return ((_tsize > 0) && (_fsize > 0) && (_itemnodes != NULL) && (_lookuptable != NULL) && (_nodes != NULL));
    }
    
    size_t Size()
    {
        return _size;
    }
    
    size_t GetMaxCapacity()
    {
        return _fsize;
    }
    
    size_t GetTableWidth()
    {
        return _tsize;
    }
    
    int Insert(const K& key, V& value)
    {
        size_t hashindex = FastHash_Hash(key) % _tsize;
        ItemNode* pInsert = NULL;
        ItemNode* pHead = _lookuptable[hashindex];
        Item* pItem = NULL;
        
        if (_freelist == NULL)
        {
            return -1;
        }
        
        pInsert = _freelist;
        _freelist = _freelist->pNext;
        
        pItem = &_nodes[pInsert->index];
        
        pItem->key = key;
        pItem->value = value;
        
        pInsert->pNext = pHead;
        
        _lookuptable[hashindex]= pInsert;
        
        UpdateIndexWithAdd(pInsert);
        
        _size++;
        
                
        return 1;
    }
    
    int Remove(const K& key)
    {
        size_t hashindex;
        
        ItemNode* pPrev = NULL;
        ItemNode* pNode = Find(key, &hashindex, &pPrev);
        ItemNode* pNext = NULL;
        
        if (pNode == NULL)
        {
            return -1;
        }
        
        pNext = pNode->pNext;
        
        if (pPrev == NULL)
        {
            _lookuptable[hashindex] = pNext;
        }
        
        if (pPrev)
        {
            pPrev->pNext = pNext;
        }
        
        UpdateIndexWithRemove(pNode);
        
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
            Item* pItem = &(_nodes[pNode->index]);
            pValue = &(pItem->value);
        }
        return pValue;
    }
    
    bool Exists(const K& key)
    {
        return (Find(key) != NULL);
    }
    
    Item* LookupByIndex(size_t index)
    {
        int itemindex;
        int indexadjusted;
        
        if ((index >= _size) || (_indexlist == NULL))
        {
            return NULL;
        }
        
        if (_fIndexValid == false)
        {
            ReIndex();
            if (_fIndexValid == false)
            {
                return NULL;
            }
        }
        
        indexadjusted = (_indexStart + index) % _fsize;
        itemindex = _indexlist[indexadjusted];
        return &(_nodes[itemindex]);
    }
    
    V* LookupValueByIndex(size_t index)
    {
        Item* pItem = LookupByIndex(index);
        return pItem ? &pItem->value : NULL;
    }
    
};


template <typename K, typename V, size_t FSIZE=100, size_t TSIZE=37>
class FastHash : public FastHashBase<K, V>
{
public:
    typedef typename FastHashBase<K,V>::Item Item;
    
protected:
    // disable copy constructor and bad overloads
    FastHash(const FastHash&) {;}
    FastHash& operator=(const FastHash&) {return *this;}
    bool operator==(const FastHash&) {return false;}
    
    typedef typename FastHashBase<K,V>::ItemNode ItemNode;
    typedef typename FastHashBase<K,V>::ItemNodePtr ItemNodePtr;
    
    Item _nodesarray[FSIZE];
    ItemNodePtr _lookuptablearray[TSIZE];
    ItemNode _itemnodesarray[FSIZE];
    int _indexarray[FSIZE];
    
public:
    FastHash() :
    FastHashBase<K,V>(FSIZE, TSIZE, _nodesarray, _itemnodesarray, _lookuptablearray, _indexarray)
    {
        COMPILE_TIME_ASSERT(FSIZE > 0);
        COMPILE_TIME_ASSERT(TSIZE > 0);
        
    }
};

template <class K, class V>
class FastHashDynamic : public FastHashBase<K,V>
{
public:
    typedef typename FastHashBase<K,V>::Item Item;
    
protected:
    Item* _nodesarray;            // array of ItemNode instances, size==fsize
    
    typedef typename FastHashBase<K,V>::ItemNode ItemNode;
    typedef typename FastHashBase<K,V>::ItemNodePtr ItemNodePtr;
    
    ItemNode* _itemnodesarray; // array of ItemNode instances, size==fsize
    ItemNodePtr* _lookuptablearray; // hash table (array of size tsize of ItemNode*)
    int* _indexarray;
    
public:
    
    FastHashDynamic() :
    _nodesarray(NULL),
    _itemnodesarray(NULL),
    _lookuptablearray(NULL),
    _indexarray(NULL)
    {
    }
    
    FastHashDynamic(size_t fsize, size_t tsize) :
    _nodesarray(NULL),
    _itemnodesarray(NULL),
    _lookuptablearray(NULL),
    _indexarray(NULL)
    {
        InitTable(fsize, tsize);
    }
    
    ~FastHashDynamic()
    {
        ResetTable();
    }
    
    int InitTable(size_t fsize, size_t tsize)
    {
        
        if (fsize <= 0)
        {
            return -1;
        }
        
        if (tsize == 0)
        {
            tsize = FastHash_GetHashTableWidth(fsize);
        }
        
        ResetTable();
        
        _nodesarray = new Item[fsize];
        _itemnodesarray = new ItemNode[fsize];
        _lookuptablearray = new ItemNodePtr[tsize];
        _indexarray = new int[fsize];
        
        if ((_nodesarray == NULL) || (_itemnodesarray == NULL) || (_lookuptablearray == NULL) || (_indexarray==NULL))
        {
            ResetTable();
            return -1;
        }
        
        this->Init(fsize, tsize, _nodesarray, _itemnodesarray, _lookuptablearray, _indexarray);
        return 1;
    }
    
    void ResetTable()
    {
        delete [] _nodesarray;
        _nodesarray = NULL;
        
        delete [] _itemnodesarray;
        _itemnodesarray = NULL;
        
        delete [] _lookuptablearray;
        _lookuptablearray = NULL;
        
        delete [] _indexarray;
        _indexarray = NULL;
        
        this->Init(0,0, NULL, NULL, NULL, NULL);
    }
    
    
};




#endif
