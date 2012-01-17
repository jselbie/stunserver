#include "commonincludes.h"
#include "polling.h"
#include "fasthash.h"

#ifdef __GNUC__
    #ifdef HAS_EPOLL
    #pragma message "polling.cpp: EPOLL is available"
    #else
    #pragma message "polling.cpp: no kernel polling api available!"
    #endif
#endif



// --------------------------------------------------------------------------

#ifdef HAS_EPOLL

class CEpoll : 
    public CBasicRefCount,
    public CObjectFactory<CEpoll>,
    public IPolling
{
private:
    int _epollfd;
    
    uint32_t ToNativeFlags(uint32_t eventflags);
    uint32_t FromNativeFlags(uint32_t eventflags);
    
public:
    virtual HRESULT Initialize();
    virtual HRESULT Close();
    virtual HRESULT Add(int fd, uint32_t eventflags);
    virtual HRESULT Remove(int fd);
    virtual HRESULT ChangeEventSet(int fd, uint32_t eventflags);
    virtual HRESULT WaitForNextEvent(PollEvent* pPollEvent, int timeoutMilliseconds);
    
    CEpoll();
    ~CEpoll();
    
    ADDREF_AND_RELEASE_IMPL();
};



CEpoll::CEpoll() :
_epollfd(-1)
{
    
}

CEpoll::~CEpoll()
{
    Close();
}

uint32_t CEpoll::ToNativeFlags(uint32_t eventflags)
{
    uint32_t result = 0;
    
    if (eventflags & IPOLLING_READ)        result |= EPOLLIN;
    if (eventflags & IPOLLING_WRITE)       result |= EPOLLOUT;
    if (eventflags & IPOLLING_EDGETRIGGER) result |= EPOLLET;
    if (eventflags & IPOLLING_RDHUP)       result |= EPOLLRDHUP;
    if (eventflags & IPOLLING_HUP)         result |= EPOLLHUP;
    if (eventflags & IPOLLING_PRI)         result |= EPOLLPRI;
    if (eventflags & IPOLLING_ERROR)       result |= EPOLLERR;
    
    return result;
}


uint32_t CEpoll::FromNativeFlags(uint32_t eventflags)
{
    uint32_t result = 0;
    
    if (eventflags & EPOLLIN)    result |= IPOLLING_READ;
    if (eventflags & EPOLLOUT)   result |= IPOLLING_WRITE;
    if (eventflags & EPOLLET)    result |= IPOLLING_EDGETRIGGER;
    if (eventflags & EPOLLRDHUP) result |= IPOLLING_RDHUP;
    if (eventflags & EPOLLHUP)   result |= IPOLLING_HUP;
    if (eventflags & EPOLLPRI)   result |= IPOLLING_PRI;
    if (eventflags & EPOLLERR)   result |= IPOLLING_ERROR;
    
    return result;
}


HRESULT CEpoll::Initialize()
{
    ASSERT(_epollfd == -1);
    
    Close();
    
    _epollfd = epoll_create(1000);
    if (_epollfd == -1)
    {
        return ERRNOHR;
    }
    return S_OK;
}

HRESULT CEpoll::Close()
{
    if (_epollfd != -1)   
    {
        close(_epollfd);

    }
    return S_OK;
}


HRESULT CEpoll::Add(int fd, uint32_t eventflags)
{
    epoll_event ev = {};
    HRESULT hr = S_OK;
    
    ChkIfA(fd == -1, E_INVALIDARG);
    ChkIfA(_epollfd==-1, E_UNEXPECTED);    

    
    ev.data.fd = fd;
    ev.events = ToNativeFlags(eventflags);
    
    ChkIfA(epoll_ctl(_epollfd, EPOLL_CTL_ADD, fd, &ev) == -1, ERRNOHR);
Cleanup:
    return hr;
}

HRESULT CEpoll::Remove(int fd)
{
    HRESULT hr = S_OK;
    epoll_event ev={}; // pass empty ev, because some implementations of epoll_ctl can't handle a NULL event struct

    ChkIfA(fd == -1, E_INVALIDARG);
    ChkIfA(_epollfd==-1, E_UNEXPECTED);    
    
    ChkIfA(epoll_ctl(_epollfd, EPOLL_CTL_DEL, fd, &ev) == -1, ERRNOHR);
Cleanup:
    return hr;    
}

HRESULT CEpoll::ChangeEventSet(int fd, uint32_t eventflags)
{
    HRESULT hr = S_OK;
    epoll_event ev = {};
    
    ChkIfA(fd == -1, E_INVALIDARG);
    ChkIfA(_epollfd==-1, E_UNEXPECTED);    
    
    ev.data.fd = fd;
    ev.events = ToNativeFlags(eventflags);
    
    ChkIfA(epoll_ctl(_epollfd, EPOLL_CTL_MOD, fd, &ev) == -1, ERRNOHR);
    
Cleanup:
    return hr;
}

HRESULT CEpoll::WaitForNextEvent(PollEvent* pPollEvent, int timeoutMilliseconds)
{
    HRESULT hr = S_OK;
    epoll_event ev = {};
    int ret;
    
    ChkIfA(_epollfd==-1, E_UNEXPECTED);    
    
    ret = ::epoll_wait(_epollfd, &ev, 1, timeoutMilliseconds);
    ChkIf(ret == -1, ERRNOHR);
    
    ChkIf(ret == 0, S_FALSE);
    
    pPollEvent->fd = ev.data.fd;
    pPollEvent->eventflags = FromNativeFlags(ev.events);
    
Cleanup:
    return hr;
}

#endif // HAS_EPOLL

// ------------------------------------------------------------------------------

class CPoll : 
    public CBasicRefCount,
    public CObjectFactory<CPoll>,
    public IPolling
{
private:
    std::vector<pollfd> _fds;
    uint32_t _rotation;
    uint32_t _unreadcount;
    bool _fInitialized;
    
    FastHash<int, size_t, 100, 101> _hashtable; // maps socket to position in fds
    
    void Reindex();
    
    uint32_t ToNativeFlags(uint32_t eventflags);
    uint32_t FromNativeFlags(uint32_t eventflags);
    
    bool FindNextEvent(PollEvent* pEvent);
    
public:
    virtual HRESULT Initialize();
    virtual HRESULT Close();
    virtual HRESULT Add(int fd, uint32_t eventflags);
    virtual HRESULT Remove(int fd);
    virtual HRESULT ChangeEventSet(int fd, uint32_t eventflags);
    virtual HRESULT WaitForNextEvent(PollEvent* pPollEvent, int timeoutMilliseconds);
    
    CPoll();
    ~CPoll();
    
    ADDREF_AND_RELEASE_IMPL();
};

CPoll::CPoll() :
_rotation (0),
_unreadcount(0),
_fInitialized(false)
{

}

CPoll::~CPoll()
{
    Close();
}

uint32_t CPoll::ToNativeFlags(uint32_t eventflags)
{
    uint32_t result = 0;
    
    if (eventflags & IPOLLING_READ)        result |= POLLIN;
    if (eventflags & IPOLLING_WRITE)       result |= POLLOUT;
    if (eventflags & IPOLLING_RDHUP)       result |= POLLRDHUP;
    if (eventflags & IPOLLING_HUP)         result |= POLLHUP;
    if (eventflags & IPOLLING_PRI)         result |= POLLPRI;
    if (eventflags & IPOLLING_ERROR)       result |= POLLERR;
    
    return result;
}


uint32_t CPoll::FromNativeFlags(uint32_t eventflags)
{
    uint32_t result = 0;
    
    if (eventflags & POLLIN)    result |= IPOLLING_READ;
    if (eventflags & POLLOUT)   result |= IPOLLING_WRITE;
    if (eventflags & POLLRDHUP) result |= IPOLLING_RDHUP;
    if (eventflags & POLLHUP)   result |= IPOLLING_HUP;
    if (eventflags & POLLPRI)   result |= IPOLLING_PRI;
    if (eventflags & POLLERR)   result |= IPOLLING_ERROR;
    
    return result;
}


HRESULT CPoll::Initialize()
{
    pollfd pfd = {};
    pfd.fd = -1;
    
    
    _fds.reserve(1000);
    _rotation = 0;
    _unreadcount = 0;
    _fInitialized = true;

    return S_OK;
}

HRESULT CPoll::Close()
{
   _fds.clear();
   _fInitialized = false;

   return S_OK;
}

HRESULT CPoll::Add(int fd, uint32_t eventflags)
{
    HRESULT hr = S_OK;
    size_t pos = _fds.size();
    pollfd pfd;
    
    ChkIfA(_fInitialized == false, E_FAIL);
    
    ChkIfA(_hashtable.Lookup(fd)!=NULL, E_UNEXPECTED);
    
    pfd.events = ToNativeFlags(eventflags);
    pfd.fd = fd;
    pfd.revents = 0;
    _fds.push_back(pfd);
    _hashtable.Insert(fd, pos);
Cleanup:    
    return hr;
}

HRESULT CPoll::Remove(int fd)
{
    size_t* pPos = NULL;
    size_t size = _fds.size();
    size_t pos;
    HRESULT hr = S_OK;

    ChkIfA(_fInitialized == false, E_FAIL);

    ASSERT(_hashtable.Size() == size);
    
    ChkIf(size == 0, E_FAIL);
    
    pPos = _hashtable.Lookup(fd);
    
    ChkIfA(pPos == NULL, E_FAIL);
    
    pos = *pPos;
    
    ChkIfA(pos >= size, E_FAIL);
    
    ChkIfA(_fds[pos].fd != fd, E_FAIL);
    
    if (pos != (size-1))
    {
        _fds[pos] = _fds[size-1];
        pPos = _hashtable.Lookup(_fds[pos].fd);
        ASSERT(pPos);
        *pPos = pos;
    }
    
    _hashtable.Remove(fd);
    _fds.pop_back();
    
Cleanup:
    return hr;
}

HRESULT CPoll::ChangeEventSet(int fd, uint32_t eventflags)
{
    size_t* pPos = NULL;
    size_t pos;
    HRESULT hr = S_OK;
    size_t size = _fds.size();
    
    ChkIfA(_fInitialized == false, E_FAIL);    
    
    ASSERT(_hashtable.Size() == size);
    ChkIf(size == 0, E_FAIL);
    
    pPos = _hashtable.Lookup(fd);
    ChkIfA(pPos == NULL, E_FAIL);
    pos = *pPos;
    ChkIfA(pos >= size, E_FAIL);
    ChkIfA(_fds[pos].fd != fd, E_FAIL);
    _fds[pos].events = ToNativeFlags(eventflags);
    
Cleanup:
    return hr;
}

HRESULT CPoll::WaitForNextEvent(PollEvent* pPollEvent, int timeoutMilliseconds)
{
    HRESULT hr = S_OK;
    int ret;
    size_t size = _fds.size();
    pollfd* list = NULL;
    bool fFound = false;
    
    ChkIfA(_fInitialized == false, E_FAIL);    
    
    ChkIfA(pPollEvent == NULL, E_INVALIDARG);
    pPollEvent->eventflags = 0;
    
    ChkIfA(size == 0, S_FALSE);

    // check first to see if there is a pending event from the last poll() call
    fFound = FindNextEvent(pPollEvent);
    
    if (fFound == false)
    {
        ASSERT(_unreadcount == 0);
        
        list = _fds.data();
    
        ret = poll(list, size, timeoutMilliseconds);
    
        ChkIfA(ret < 0, ERRNOHR); // error
        ChkIf(ret == 0, S_FALSE); // no data, we timed out
        
        _unreadcount = (uint32_t)ret;
        
        fFound = FindNextEvent(pPollEvent);
        ASSERT(fFound); // poll returned a positive value, but we didn't find anything?
    }
    
    hr = fFound ? S_OK : S_FALSE;
    
Cleanup:    
    return hr;
}

bool CPoll::FindNextEvent(PollEvent* pEvent)
{
    size_t size = _fds.size();
    ASSERT(size > 0);
    pollfd* list = _fds.data();
    bool fFound = false;
    
    if (_unreadcount == 0)
    {
        return false;
    }
    
    if (_rotation >= size)
    {
        _rotation = 0;
    }
    
    for (size_t index = 0; index < size; index++)
    {
        size_t slotindex = (index + _rotation) % size;
        
        if (list[slotindex].revents)
        {
            pEvent->fd = list[slotindex].fd;
            pEvent->eventflags = FromNativeFlags(list[slotindex].revents);
            list[slotindex].revents = 0;
            fFound = true;
            break;
        }
    }
    
    if (fFound)
    {
        _rotation++;        
    }
    else
    {
        _unreadcount = _unreadcount - 1;
        // don't increment rotation if we didn't find anything
    }
    
    return fFound;
}



HRESULT CreatePollingInstance(uint32_t type, IPolling** ppPolling)
{
    HRESULT hr = S_OK;
    
    ChkIfA(ppPolling == NULL, E_INVALIDARG);
    
#ifdef HAS_EPOLL
    if (type == IPOLLING_TYPE_BEST)
    {
        type = IPOLLING_TYPE_EPOLL;
    }
#else
    if (type == IPOLLING_TYPE_BEST)
    {
        type = IPOLLING_TYPE_POLL;
    }
#endif
    
    
    if (type == IPOLLING_TYPE_EPOLL)
    {
#ifndef HAS_EPOLL
        ChkA(E_FAIL);
#else
        ChkA(CEpoll::CreateInstance(ppPolling));
#endif
    }
    else if (type == IPOLLING_TYPE_POLL)
    {
        ChkA(CPoll::CreateInstance(ppPolling));
    }
    else
    {
        ChkA(E_FAIL); // unknown type
    }
    
Cleanup:
    return hr;
}

