#include "commonincludes.hpp"
#include "stuncore.h"
#include "stunconnection.h"

CConnectionPool::CConnectionPool() :
_freelist(nullptr)
{
    
}

CConnectionPool::~CConnectionPool()
{
    Reset();
}

void CConnectionPool::Reset()
{
    StunConnection* pConn;
    while (_freelist)
    {
        pConn = _freelist;
        _freelist = _freelist->pNext;
        delete pConn;
    }
}

HRESULT CConnectionPool::Grow()
{
    const size_t c_growrate = 50;
    
    for (size_t i = 0; i < c_growrate; i++)
    {
        StunConnection* pConn = nullptr;
        pConn = new StunConnection();
        
        if (pConn == nullptr)
        {
            return E_OUTOFMEMORY;
        }
        
        pConn->_spOutputBuffer = CRefCountedBuffer(new CBuffer(MAX_STUN_MESSAGE_SIZE));
        pConn->_spReaderBuffer = CRefCountedBuffer(new CBuffer(MAX_STUN_MESSAGE_SIZE));
        
        if ((pConn->_spOutputBuffer == nullptr) || (pConn->_spReaderBuffer == nullptr))
        {
            delete pConn;
            return E_OUTOFMEMORY;
        }
        
        pConn->pNext = _freelist;
        _freelist = pConn;
    }
    
    return S_OK;
}

StunConnection* CConnectionPool::GetConnection(int sock, SocketRole role)
{
    StunConnection* pConn = nullptr;
    
    if (_freelist == nullptr)
    {
        Grow();
        if (_freelist == nullptr)
        {
            return nullptr; // out of memory ?
        }
    }
    
    pConn = _freelist;
    _freelist = pConn->pNext;
    pConn->pNext = nullptr;
    
    // prep this connection for usage
    pConn->_reader.Reset();
    pConn->_reader.GetStream().Attach(pConn->_spReaderBuffer, true);
    pConn->_state = ConnectionState_Receiving;
    pConn->_stunsocket.Attach(sock);
    pConn->_stunsocket.SetRole(role);
    pConn->_txCount = 0;
    pConn->_timeStart = time(nullptr);
    pConn->_idHashTable = -1;    
    
    return pConn;
}

void CConnectionPool::ReleaseConnection(StunConnection* pConn)
{
    ASSERT(pConn->_stunsocket.IsValid() == false); // not the pool's job to close a socket!
    
    pConn->pNext = _freelist;
    _freelist = pConn;
}

void CConnectionPool::ResetConnection(StunConnection* pConn)
{
    pConn->_reader.Reset();
    pConn->_reader.GetStream().Attach(pConn->_spReaderBuffer, true);
    pConn->_state = ConnectionState_Receiving;
    pConn->_txCount = 0;
}

