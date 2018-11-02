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
#include "buffer.h"



CBuffer::CBuffer() :
_data(nullptr),
_size(0),
_allocatedSize(0)
{
    ;
}

void CBuffer::Reset()
{
    _spAllocation.clear();
    _data = nullptr;
    _size = 0;
    _allocatedSize = 0;

}


CBuffer::CBuffer(size_t nSize)
{
    InitWithAllocation(nSize);
}


HRESULT CBuffer::InitWithAllocation(size_t size)
{

    Reset();

    // deliberately not checking for 0.  Ok to allocate a 0 byte array
    std::vector<uint8_t> spAlloc(size+2); // add two bytes for null termination (makes debugging ascii and unicode strings easier), but these two bytes are invisible to the caller (not included in _allocatedSize)

    _spAllocation.swap(spAlloc);

    spAlloc.clear();

    _data = _spAllocation.data();

    if (_data)
    {
        _data[size] = 0;
        _data[size+1] = 0;
    }

    _size = (_data != nullptr) ? size : 0;
    _allocatedSize = _size;

    return (_data != nullptr) ? S_OK : E_FAIL;
}

HRESULT CBuffer::InitNoAlloc(uint8_t* pByteArray, size_t size)
{
    Reset();

    if (pByteArray == nullptr) 
    {
        size = 0;
    }

    _data = pByteArray;
    _size = size;
    _allocatedSize = size;
    return S_OK;
}

HRESULT CBuffer::InitWithAllocAndCopy(uint8_t* pByteArray, size_t size)
{
    HRESULT hr = S_OK;
    Reset();

    if (pByteArray == nullptr)
    {
        size = 0;
    }

    hr = InitWithAllocation(size);

    if (SUCCEEDED(hr))
    {
        memcpy(_data, pByteArray, _size);
    }

    return hr;
}


CBuffer::CBuffer(uint8_t* pByteArray, size_t nByteArraySize, bool fCopy)
{

    if (fCopy == false)
    {
        InitNoAlloc(pByteArray, nByteArraySize);
    }
    else
    {
        InitWithAllocAndCopy(pByteArray, nByteArraySize);
    }
}




HRESULT CBuffer::SetSize(size_t size)
{
    HRESULT hr = S_OK;

    hr = (size <= _allocatedSize);

    if (SUCCEEDED(hr))
    {
        _size = size;
    }

    return hr;
}




bool CBuffer::IsValid()
{
    return (_data != nullptr);
}



