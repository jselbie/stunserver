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

#include "commonincludes.h"
#include "tcpserver.h"
#include "server.h"
#include "stunsocket.h"


class CStunConnectionBufferPool
{
protected:
    
    size_t _maxCount;  // the max number of buffers that can be instantiated at once (aka, "max number of connections")
    std::list<CRefCountedBuffer> _listBuffers;
    std::list<CRefCountedBuffer> _listFree;
public:
    CStunConnectionBufferPool(size_t initialCount, size_t maxCount);
    HRESULT Grow(size_t newcount);
    void ReturnToPool(CRefCountedBuffer& spBuffer);
    HRESULT GetBuffer(CRefCountedBuffer* pspBuffer);
};

CStunConnectionBufferPool::CStunConnectionBufferPool(size_t initialCount, size_t maxCount)
{
    if (initialCount > maxCount)
    {
        maxCount = initialCount;
    }
    
    _maxCount = maxCount;
    Grow(initialCount);
}

HRESULT CStunConnectionBufferPool::Grow(size_t newcount)
{
    size_t total = _listBuffers.size();
    size_t inc = 0;
    
    if (newcount <= total)
    {
        return S_OK;
    }
    
    while (total < newcount)
    {
        if (total >= _maxCount)
        {
            break;
        }
        
        CBuffer* pBuffer = new CBuffer(1500);
        if (pBuffer == NULL)
        {
            return E_OUTOFMEMORY;
        }
        
        CRefCountedBuffer spBuffer(pBuffer);
        
        _listBuffers.push_back(spBuffer);
        _listFree.push_back(spBuffer);
        inc++;
    }
    
    if (inc == 0)
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

HRESULT CStunConnectionBufferPool::GetBuffer(CRefCountedBuffer* pspBuffer)
{
    CRefCountedBuffer spBuffer;
    size_t total = _listBuffers.size();
    
    if (_listFree.size() == 0)
    {
        Grow(total*2 + 1);
    }
    
    if (_listFree.size() == 0)
    {
        return E_OUTOFMEMORY;
    }
    
    spBuffer = _listFree.pop_back();
    
    *pspBuffer = spBuffer;
    return S_OK;
}

void CStunConnectionBufferPool::ReturnToPool(CRefCountedBuffer& spBuffer)
{
    ASSERT(spBuffer.get() != NULL);
    ASSERT(spBuffer->GetData() != NULL);
    
    _listFree.push_back(spBuffer);
}

enum StunConnectionState
{
    ConnectionState_Idle,
    ConnectionState_Receiving,
    ConnectionState_Transmitting,
};


struct StunConnection
{
    StunConnectionState _state;
    
    time_t _expireTime;
    
    CRefCountedStunSocket spSocket;
    
    CStunMessageReader reader;
    CRefCountedBuffer spReaderBuffer;
    
    CRefCountedBuffer spOutputBuffer;
    size_t txCount;  // number of bytes transmitted thus far
    
    void ResetToIdle(CStunConnectionBufferPool* pPool);
};

void StunConnection::ResetToIdle(CStunConnectionBufferPool* pPool)
{
    pPool->ReturnToPool(spReaderBuffer);
    pPool->ReturnToPool(spOutputBuffer);
    spReaderBuffer.reset();
    spOutputBuffer.reset();
    
    spSocket
}



class CTCPStunServer
{
private:
    CRefCountedStunSocket _spListenSocket;
    
    
    
public:
    HRESULT Initialize(const CStunServerConfig& config);
    HRESULT Shutdown();

    HRESULT Start();
    HRESULT Stop();
};



#endif	/* SERVER_H */

