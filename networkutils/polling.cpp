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
#include "polling.h"
#include "fasthash.h"


// --------------------------------------------------------------------------

#ifdef HAS_EPOLL

class CEpoll : 
    public CBasicRefCount,
    public CObjectFactory<CEpoll>,
    public IPolling
{
private:
    int _epollfd;
    
    epoll_event* _events;
    size_t _sizeEvents;        // total allocated size for events
    size_t _pendingCount;      // number of valid events in _events
    size_t _currentEventIndex; // which one to process next
    
    
    
    uint32_t ToNativeFlags(uint32_t eventflags);
    uint32_t FromNativeFlags(uint32_t eventflags);
    
public:
    virtual HRESULT Initialize(size_t maxSockets);
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
_epollfd(-1),
_events(NULL),
_sizeEvents(0),
_pendingCount(0),
_currentEventIndex(0)
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
#ifdef EPOLLRDHUP
    if (eventflags & IPOLLING_RDHUP)       result |= EPOLLRDHUP;
#endif
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
#ifdef EPOLLRDHUP
    if (eventflags & EPOLLRDHUP) result |= IPOLLING_RDHUP;
#endif
    if (eventflags & EPOLLHUP)   result |= IPOLLING_HUP;
    if (eventflags & EPOLLPRI)   result |= IPOLLING_PRI;
    if (eventflags & EPOLLERR)   result |= IPOLLING_ERROR;
    
    return result;
}


HRESULT CEpoll::Initialize(size_t maxSockets)
{
    HRESULT hr = S_OK;
    
    ASSERT(_epollfd == -1);
    
    Close();
    
    _epollfd = epoll_create(maxSockets); // maxSockets is likely ignored by epoll_create
    ChkIf(_epollfd == -1, ERRNOHR);
    

    _sizeEvents = maxSockets;
    _events = new epoll_event[maxSockets];
    ChkIf(_events == NULL, E_OUTOFMEMORY);
    
    
    _pendingCount = 0;
    _currentEventIndex = 0;
Cleanup:
    if (FAILED(hr))
    {
        Close();
    }
    return S_OK;
}

HRESULT CEpoll::Close()
{
    if (_epollfd != -1)   
    {
        close(_epollfd);
        _epollfd = -1;
    }
    
    delete [] _events;
    _events = NULL;
    _sizeEvents = 0;
    _pendingCount = 0;
    _currentEventIndex = 0;
    
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
    // Remove doesn't bother to check to see if the socket is within any
    // unprocessed event within _events.  A more robust polling and eventing
    // library might want to check this.  For the stun server, "Remove" gets
    // called immediately after WaitForNextEvent in most cases. That is, the socket
    // we just got notified for isn't going to be within the _events array
    
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
    epoll_event *pEvent = NULL;
    int ret = 0;
    
    ChkIfA(_epollfd==-1, E_UNEXPECTED);   
    
    if (_currentEventIndex >= _pendingCount)
    {
        _currentEventIndex = 0;
        _pendingCount = 0;
        
        ret = ::epoll_wait(_epollfd, _events, _sizeEvents, timeoutMilliseconds);
        ChkIf(ret <= -1, ERRNOHR);
        ChkIf(ret == 0, S_FALSE);
        
        _pendingCount = (size_t)ret;
    }
    
    
    pEvent = &_events[_currentEventIndex];
    _currentEventIndex++;
    
    
    pPollEvent->fd = pEvent->data.fd;
    pPollEvent->eventflags = FromNativeFlags(pEvent->events);
    
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
    
    FastHashDynamic<int, size_t> _hashtable; // maps socket to position in fds
    
    void Reindex();
    
    uint32_t ToNativeFlags(uint32_t eventflags);
    uint32_t FromNativeFlags(uint32_t eventflags);
    
    bool FindNextEvent(PollEvent* pEvent);
    
public:
    virtual HRESULT Initialize(size_t maxSockets);
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
#ifdef POLLRDHUP
    if (eventflags & IPOLLING_RDHUP)       result |= POLLRDHUP;
#endif
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
#ifdef POLLRDHUP    
    if (eventflags & POLLRDHUP) result |= IPOLLING_RDHUP;
#endif
    if (eventflags & POLLHUP)   result |= IPOLLING_HUP;
    if (eventflags & POLLPRI)   result |= IPOLLING_PRI;
    if (eventflags & POLLERR)   result |= IPOLLING_ERROR;
    
    return result;
}


HRESULT CPoll::Initialize(size_t maxSockets)
{
    _fds.reserve(maxSockets);
    _rotation = 0;
    _unreadcount = 0;
    
    _hashtable.InitTable(maxSockets, 0);
    
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
    
    // See notes below why pPos is declared volatile.  Gets around a compiler bug
    volatile size_t* pPos = NULL;
    
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
        
        ASSERT(pPos != NULL);
       
        // If the volatile declaration above was not made, this block of code
        // gets over-optimized on older GCC compilers (g++ 4.2.1 on BSD) with with -O2
        // The following line would essentially not get executed.
        // There are multiple workarounds, but "volatile" seems to work.
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
    
    ChkIf(size == 0, S_FALSE);

    // check first to see if there is a pending event from the last poll() call
    fFound = FindNextEvent(pPollEvent);
    
    if (fFound == false)
    {
        ASSERT(_unreadcount == 0);
        
        _unreadcount = 0;
        
        list = &_fds.front();
    
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
    pollfd* list = &_fds.front();
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
            _rotation++;
            _unreadcount--;
            break;
        }
    }
    
    // don't increment _rotation if we didn't find anything
    
    return fFound;
}



HRESULT CreatePollingInstance(uint32_t type, size_t maxSockets, IPolling** ppPolling)
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
        ChkA(CEpoll::CreateInstance(maxSockets, ppPolling));
#endif
    }
    else if (type == IPOLLING_TYPE_POLL)
    {
        ChkA(CPoll::CreateInstance(maxSockets, ppPolling));
    }
    else
    {
        ChkA(E_FAIL); // unknown type
    }
    
Cleanup:
    return hr;
}

