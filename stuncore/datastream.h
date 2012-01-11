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



#ifndef DATASTREAM_H
#define DATASTREAM_H

#include "buffer.h"

class CDataStream
{
    CRefCountedBuffer _spBuffer;
    CBuffer* _pBuffer; // direct pointer for better performance
    size_t _pos;
    bool _fNoGrow;
    
    HRESULT Grow(size_t newsize);


public:
    CDataStream();
    CDataStream(CRefCountedBuffer& buffer);

    HRESULT SetSizeHint(size_t size);

    void SetNoGrow(bool fDisableGrow);

    void Reset();
    void Attach(CRefCountedBuffer& buffer, bool fForWriting);
    
    HRESULT Write(const void* data, size_t size);
    HRESULT Read(void* data, size_t size);

    HRESULT WriteUint8(uint8_t val)   {return Write(&val, sizeof(val));}
    HRESULT WriteUint16(uint16_t val) {return Write(&val, sizeof(val));}
    HRESULT WriteUint32(uint32_t val) {return Write(&val, sizeof(val));}
    HRESULT WriteUint64(uint64_t val) {return Write(&val, sizeof(val));}

    HRESULT WriteInt8(int8_t val)   {return Write(&val, sizeof(val));}
    HRESULT WriteInt16(int16_t val) {return Write(&val, sizeof(val));}
    HRESULT WriteInt32(int32_t val) {return Write(&val, sizeof(val));}
    HRESULT WriteInt64(int64_t val) {return Write(&val, sizeof(val));}

    HRESULT ReadUint8(uint8_t* pVal)   {return Read(pVal, sizeof(*pVal));}
    HRESULT ReadUint16(uint16_t* pVal) {return Read(pVal, sizeof(*pVal));}
    HRESULT ReadUint32(uint32_t* pVal) {return Read(pVal, sizeof(*pVal));}
    HRESULT ReadUint64(uint64_t* pVal) {return Read(pVal, sizeof(*pVal));}

    HRESULT ReadInt8(int8_t* pVal)   {return Read(pVal, sizeof(*pVal));}
    HRESULT ReadInt16(int16_t* pVal) {return Read(pVal, sizeof(*pVal));}
    HRESULT ReadInt32(int32_t* pVal) {return Read(pVal, sizeof(*pVal));}
    HRESULT ReadInt64(int64_t* pVal) {return Read(pVal, sizeof(*pVal));}


    HRESULT GetBuffer(CRefCountedBuffer* pRefCountedBuffer);
    uint8_t* GetDataPointerUnsafe();

    HRESULT SeekDirect(size_t pos);
    HRESULT SeekRelative(int nOffset);

    size_t GetPos();
    size_t GetSize();


    bool IsEOF();
};





#endif
