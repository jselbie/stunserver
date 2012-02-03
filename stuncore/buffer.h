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




#ifndef CBUFFER_H
#define CBUFFER_H





class CBuffer
{
private:
    uint8_t* _data;
    size_t _size;
    size_t _allocatedSize;
    boost::scoped_array<uint8_t> _spAllocation;

    // disallow copy and assignment.
    CBuffer(const CBuffer&);
    void operator=(const CBuffer& other);


public:
    CBuffer(); // deliberately makes the buffer null
    void Reset(); // releases current pointer


    CBuffer(size_t nSize);
    HRESULT InitWithAllocation(size_t size);


    CBuffer(uint8_t* pByteArray, size_t nByteArraySize, bool fCopy);

    HRESULT InitWithAllocAndCopy(uint8_t* pByteArray, size_t nByteArraySize);
    HRESULT InitNoAlloc(uint8_t* pByteArray, size_t nByteArraySize);

    inline size_t GetSize() {return _size;}
    inline size_t GetAllocatedSize() {return _allocatedSize;}

    HRESULT SetSize(size_t size);


    inline uint8_t* GetData() {return _data;}

    bool IsValid();
};

typedef boost::shared_ptr<CBuffer> CRefCountedBuffer;


#endif
