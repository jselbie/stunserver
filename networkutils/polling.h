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
    virtual HRESULT Initialize(size_t maxSockets) = 0;
    virtual HRESULT Close() = 0;
    virtual HRESULT Add(int fd, uint32_t eventflags) = 0;
    virtual HRESULT Remove(int fd) = 0;
    virtual HRESULT ChangeEventSet(int fd, uint32_t eventflags) = 0;
    virtual HRESULT WaitForNextEvent(PollEvent* pPollEvent, int timeoutMilliseconds) = 0;
};


const uint32_t IPOLLING_TYPE_BEST  = 0x01 << 0;
const uint32_t IPOLLING_TYPE_EPOLL = 0x01 << 1;
const uint32_t IPOLLING_TYPE_POLL  = 0x01 << 2;

HRESULT CreatePollingInstance(uint32_t type, size_t maxSockets, IPolling** ppPolling);



#endif	/* POLLING_H */

