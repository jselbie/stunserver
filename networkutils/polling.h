/* 
 * File:   polling.h
 * Author: jselbie
 *
 * Created on January 12, 2012, 5:58 PM
 */

#ifndef POLLING_H
#define	POLLING_H


struct PollEvent
{
    int fd;
    uint32_t eventflags;
};


// event flags
const uint32_t IPOLLING_READ =         0x01 << 0;
const uint32_t IPOLLING_WRITE =        0x01 << 1;
const uint32_t IPOLLING_EDGETRIGGER =  0x01 << 2;
const uint32_t IPOLLING_RDHUP =        0x01 << 3;
const uint32_t IPOLLING_HUP =          0x01 << 4;
const uint32_t IPOLLING_PRI =          0x01 << 5;
const uint32_t IPOLLING_ERROR =        0x01 << 6;


class IPolling : public IRefCounted
{
public:
    virtual HRESULT Initialize() = 0;
    virtual HRESULT Close() = 0;
    virtual HRESULT Add(int fd, uint32_t eventflags) = 0;
    virtual HRESULT Remove(int fd) = 0;
    virtual HRESULT ChangeEventSet(int fd, uint32_t eventflags) = 0;
    virtual HRESULT WaitForNextEvent(PollEvent* pPollEvent, int timeoutMilliseconds) = 0;
};


const uint32_t IPOLLING_TYPE_BEST  = 0x01 << 0;
const uint32_t IPOLLING_TYPE_EPOLL = 0x01 << 1;
const uint32_t IPOLLING_TYPE_POLL  = 0x01 << 2;

HRESULT CreatePollingInstance(uint32_t type, IPolling** ppPolling);



#endif	/* POLLING_H */

