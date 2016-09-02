#include "commonincludes.hpp"
#include "stuncore.h"
#include "stunconnection.h"

CConnectionPool::CConnectionPool() :
_freelist(NULL)
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
        StunConnection* pConn = NULL;
        pConn = new StunConnection();
        
        if (pConn == NULL)
        {
            return E_OUTOFMEMORY;
        }
        
        pConn->_spOutputBuffer = CRefCountedBuffer(new CBuffer(MAX_STUN_MESSAGE_SIZE));
        pConn->_spReaderBuffer = CRefCountedBuffer(new CBuffer(MAX_STUN_MESSAGE_SIZE));
        
        if ((pConn->_spOutputBuffer == NULL) || (pConn->_spReaderBuffer == NULL))
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
    StunConnection* pConn = NULL;
    
    if (_freelist == NULL)
    {
        Grow();
        if (_freelist == NULL)
        {
            return NULL; // out of memory ?
        }
    }
    
    pConn = _freelist;
    _freelist = pConn->pNext;
    pConn->pNext = NULL;
    
    // prep this connection for usage
    pConn->_reader.Reset();
    pConn->_reader.GetStream().Attach(pConn->_spReaderBuffer, true);
    pConn->_state = ConnectionState_Receiving;
    pConn->_stunsocket.Attach(sock);
    pConn->_stunsocket.SetRole(role);
    pConn->_txCount = 0;
    pConn->_timeStart = time(NULL);
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

