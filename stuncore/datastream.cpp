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
#include "datastream.h"



CDataStream::CDataStream() :
_pBuffer(NULL),
_pos(0),
_fNoGrow(false)
{
    // _spBuffer is null
}


CDataStream::CDataStream(CRefCountedBuffer& spBuffer) :
_spBuffer(spBuffer),
_pos(0),
_fNoGrow(false)
{
    _pBuffer = spBuffer.get();
}

HRESULT CDataStream::SetSizeHint(size_t size)
{
    return Grow(size);
}


void CDataStream::Reset()
{
    _spBuffer.reset();
    _pBuffer = NULL;
    _pos = 0;
    _fNoGrow = false;
}

void CDataStream::Attach(CRefCountedBuffer& buf, bool fForWriting)
{
    Reset();
    _spBuffer = buf;
    _pBuffer = _spBuffer.get();

    if (_spBuffer && fForWriting)
    {
        _spBuffer->SetSize(0);
    }
}

HRESULT CDataStream::Read(void* data, size_t size)
{
    size_t newpos = size + _pos;
    size_t currentSize = GetSize();

    ASSERT(newpos <= currentSize);

    if (newpos > currentSize)
    {
        return E_INVALIDARG;
    }

    memcpy(data, _pBuffer->GetData() + _pos, size);
    _pos = newpos;
    return S_OK;
}

HRESULT CDataStream::Grow(size_t size)
{
    size_t currentAllocated = (_pBuffer ? _pBuffer->GetAllocatedSize() : 0);
    size_t currentSize = GetSize();
    size_t newallocationsize=0;

    if (size <= currentAllocated)
    {
        return S_OK;
    }

    if (_fNoGrow)
    {
        return E_FAIL;
    }
    
    if (size > (currentAllocated*2))
    {
        newallocationsize = size;
    }
    else
    {
        newallocationsize = currentAllocated*2;
    }

    ASSERT(newallocationsize > 0);

    CRefCountedBuffer spNewBuffer(new CBuffer(newallocationsize));


    if (spNewBuffer->IsValid() == false)
    {
        return E_OUTOFMEMORY;
    }

    // Grow only increases allocated size.  It doesn't influence the actual data stream size
    spNewBuffer->SetSize(currentSize);

    if (_pBuffer && (currentSize > 0))
    {
        memcpy(spNewBuffer->GetData(), _pBuffer->GetData(), currentSize);
    }

    _spBuffer = spNewBuffer;
    _pBuffer = _spBuffer.get();

    return S_OK;
}

void CDataStream::SetNoGrow(bool fNoGrow)
{
    _fNoGrow = fNoGrow;
}

HRESULT CDataStream::Write(const void* data, size_t size)
{
    
    size_t newposition = size + _pos;
    size_t currentSize = GetSize();
    HRESULT hr = S_OK;

    if ((size == 0) || (data == NULL))
    {
        return E_FAIL;
    }

    hr = Grow(newposition); // make sure we have enough buffer to write into

    if (FAILED(hr))
    {
        return hr;
    }

    memcpy(_pBuffer->GetData()+_pos, data, size);
    _pos = newposition;

    if (newposition > currentSize)
    {
        hr = _pBuffer->SetSize(newposition);
        ASSERT(SUCCEEDED(hr));
    }

    return hr;
}



bool CDataStream::IsEOF()
{
    size_t currentSize = GetSize();
    return (_pos >= currentSize);
}

size_t CDataStream::GetPos()
{
    return _pos;
}

size_t CDataStream::GetSize()
{
    return (_pBuffer ? _pBuffer->GetSize() : 0);
}

HRESULT CDataStream::SeekDirect(size_t pos)
{
    HRESULT hr = S_OK;
    size_t currentSize = (_pBuffer ? _pBuffer->GetSize() : 0);

    // seeking is allowed anywhere between 0 and stream size

    if (pos <= currentSize)
    {
        _pos = pos;
    }
    else
    {
        ASSERT(false); // likely a programmer error if we seek out of the stream
        hr = E_FAIL;
    }

    return hr;
}

HRESULT CDataStream::SeekRelative(int offset)
{
    // todo: there might be some math overflow to check here.
    return SeekDirect(offset + _pos);
}

HRESULT CDataStream::GetBuffer(CRefCountedBuffer* pBuffer)
{
    HRESULT hr;

    if (pBuffer)
    {
        *pBuffer = _spBuffer;
        hr = S_OK;
    }
    else
    {
        hr = E_FAIL;
    }

    return hr;
}

// unsafe because the pointer is not guaranteed to be valid after subsequent writes.  Could also be NULL if nothing has been written
uint8_t* CDataStream::GetDataPointerUnsafe()
{
    uint8_t* pRet = NULL;

    if (_pBuffer)
    {
        pRet = _pBuffer->GetData();
    }

    return pRet;
}






