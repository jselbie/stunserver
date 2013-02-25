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
#include "unittest.h"

#include "polling.h"

#include "testpolling.h"

CTestPolling::CTestPolling()
{
    ;
}

CTestPolling::~CTestPolling()
{
    TestUnInit();
}

HRESULT CTestPolling::Run()
{
    HRESULT hr = S_OK;
    
#ifdef HAS_EPOLL
    _polltype = IPOLLING_TYPE_EPOLL;
    ChkA(Test1());
    ChkA(Test2());
#endif
    
    _polltype = IPOLLING_TYPE_POLL;
    ChkA(Test1());
    ChkA(Test2());
    ChkA(Test3());
Cleanup:
    return hr;
}

void CTestPolling::TestUnInit()
{
    size_t size = _pipes.size();
    _spPolling.ReleaseAndClear();
    
    for (size_t index = 0; index < size; index++)
    {
        close(_pipes[index].readpipe);
        close(_pipes[index].writepipe);
    }
    
    _pipes.clear();
    
}

HRESULT CTestPolling::SetNonBlocking(int fd)
{
    HRESULT hr = S_OK;
    int result;
    int flags;
    
    flags = ::fcntl(fd, F_GETFL, 0);
    
    ChkIfA(flags == -1, ERRNOHR);
    
    flags |= O_NONBLOCK;
    
    result = fcntl(fd , F_SETFL , flags);
    
    ChkIfA(result == -1, ERRNOHR);
    
Cleanup:
    return hr;  
    
}

HRESULT CTestPolling::CreateAndAddPipe()
{
    HRESULT hr = S_OK;
    PipePair pp = {};
    int ret = -1;
    int fds[2];
    ret = ::pipe(fds);
    
    ChkIfA(ret == -1, ERRNOHR);

    pp.readpipe = fds[0];
    pp.writepipe = fds[1];
    pp.fDataPending = false;

    ChkA(SetNonBlocking(pp.readpipe));
    ChkA(SetNonBlocking(pp.writepipe));

    _pipes.push_back(pp);

    ChkA(_spPolling->Add(fds[0], IPOLLING_READ)); 
    
Cleanup:
    return hr;
}

HRESULT CTestPolling::TestInit(size_t sizePolling, size_t sizePipeArray)
{
    HRESULT hr = S_OK;
    
    TestUnInit();
    
    ChkA(CreatePollingInstance(_polltype, sizePolling, _spPolling.GetPointerPointer()));
    
    for (size_t index = 0; index < sizePipeArray; index++)
    {
        ChkA(CreateAndAddPipe());
    }
    
Cleanup:
    return hr;
}

HRESULT CTestPolling::WritePipe(PipePair* pPair)
{
    HRESULT hr = S_OK;
    char ch = 'x';
    int ret = -1;
    
    ret = write(pPair->writepipe, &ch, 1);
    ChkIfA(ret < 0, ERRNOHR);
    ChkIfA(ret == 0, E_UNEXPECTED);
    pPair->fDataPending = true;
    
Cleanup:
    return hr;
}

HRESULT CTestPolling::ConsumeEvent(int* pFD, int* pCount)
{
    HRESULT hr = S_OK;
    HRESULT hrResult = S_OK;
    PollEvent event;
    char ch;
    int result;
    int count = 0;
    int fd = -1;
    int pipesindex = -1;
    
    hrResult = _spPolling->WaitForNextEvent(&event, 0);
    ChkA(hrResult);
    
    ChkIfA(hrResult == S_FALSE, S_FALSE);
    
    fd = event.fd;
    
    while (true)
    {
        result = ::read(fd, &ch, 1);
        if (result <= 0)
        {
            break;
        }
        count++;
    }
    
    pipesindex = FindPipePairIndex(fd);
    ChkIfA(pipesindex == -1, E_UNEXPECTED);
    
    ChkIfA(count == 0, E_UNEXPECTED);
    
    ChkIfA(_pipes[pipesindex].fDataPending == false, E_UNEXPECTED);
    _pipes[pipesindex].fDataPending = false;
    
Cleanup:
    if (pFD)    
    {
        *pFD = fd;
    }
    if (pCount)
    {
        *pCount = count;
    }
    return hr;
}

int CTestPolling::FindPipePairIndex(int fd)
{
    size_t size = _pipes.size();
    
    for (size_t index = 0; index < size; index++)
    {
        if ((_pipes[index].readpipe == fd) || (_pipes[index].writepipe == fd))
        {
            return index;
        }
    }
    
    return -1;
}

size_t CTestPolling::GetPendingCount()
{
    size_t size = _pipes.size();
    size_t count = 0;
    
    for (size_t index = 0; index < size; index++)
    {
        if (_pipes[index].fDataPending)
        {
            count++;
        }
    }
    
    return count;
}

HRESULT CTestPolling::RemovePipe(int pipeindex)
{
    HRESULT hr = S_OK;
    
    size_t size = _pipes.size();
    
    ChkIfA(pipeindex < 0, E_FAIL);
    ChkIfA(pipeindex >= (int)size, E_FAIL);
    
    ChkA(_spPolling->Remove(_pipes[pipeindex].readpipe));
    
    close(_pipes[pipeindex].readpipe);
    _pipes[pipeindex].readpipe = -1;
    
    close(_pipes[pipeindex].writepipe);
    _pipes[pipeindex].writepipe = -1;
    
    _pipes.erase(_pipes.begin()+pipeindex);
    
Cleanup:
    return hr;
}



// simplest of all tests. Just set a file descriptor and see that it's available
// repeat many times
HRESULT CTestPolling::Test1()
{
    HRESULT hr = S_OK;
    HRESULT hrResult;
    size_t size;
    PollEvent event;    
    int fd;
    int count = 0;
    
    srand(100);

    ChkA(TestInit(10, 10));
    
    size = _pipes.size();
    
    hrResult = _spPolling->WaitForNextEvent(&event, 0);
    ChkIfA(hrResult != S_FALSE, E_UNEXPECTED);    
    
    // one event at a time model
    for (int index = 0; index < 100; index++)
    {

        size_t item = rand() % size;
        
        ChkA(WritePipe(&_pipes[item]));
        
        ConsumeEvent(&fd, &count);
        
        ChkIfA(fd != _pipes[item].readpipe, E_UNEXPECTED);
        ChkIfA(count != 1, E_UNEXPECTED);
    }
    
    hrResult = _spPolling->WaitForNextEvent(&event, 0);
    ChkIfA(hrResult != S_FALSE, E_UNEXPECTED);
    
Cleanup:
    return hr;
}



// create a polling set
HRESULT CTestPolling::Test2()
{
    // simulate the following events in random order:
    //    socket added (did it succeed as expected)
    //    incoming data (write to a random pipe)
    //    WaitForNextEvent called (did it return an expected result/socket)
    //        Remove socket last notified about

    HRESULT hr = S_OK;
    HRESULT hrResult;
    PollEvent event;  
    const size_t c_maxSockets = 10;
    
    srand(100);

    ChkA(TestInit(c_maxSockets, 0));
    
   
    hrResult = _spPolling->WaitForNextEvent(&event, 0);
    ChkIfA(hrResult != S_FALSE, E_UNEXPECTED);    
    
    
    for (size_t index = 0; index < 1000; index++)
    {
        int randresult = ::rand() % 4;
        
        switch (randresult)
        {
            case 0:
            {
                // simulate a new socket being added
                if (_pipes.size() >= c_maxSockets)
                {
                    continue;
                }
                
                ChkA(CreateAndAddPipe());

                break;
            }
            
            case 1:
            {
                // simulate incoming data
                size_t size = _pipes.size();
                size_t itemindex;
                
                if (size == 0)
                {
                    continue;
                }
                
                itemindex = rand() % size;
                ChkA(WritePipe(&_pipes[itemindex]));
                
                break;
            }
            
            case 2:
            case 3:
            {
                int fd;
                size_t pending = GetPendingCount();
                if (pending == 0)
                {
                    continue;
                }
                
                ChkA(ConsumeEvent(&fd, NULL));
                
                if (randresult == 3)
                {
                    // simulate removing this pipe from the set
                    ChkA(RemovePipe(FindPipePairIndex(fd)));
                }
                break;
            } // case
        } // switch
    } // for

Cleanup:
    return hr;
}

HRESULT CTestPolling::Test3()
{
    HRESULT hr = S_OK;

    const size_t c_maxSockets = 10;
    
    ChkA(TestInit(c_maxSockets, 0));
    
    ChkA(_spPolling->Add(3, IPOLLING_READ));
    ChkA(_spPolling->Remove(3));
    ChkA(_spPolling->Add(5, IPOLLING_READ));
    ChkA(_spPolling->Add(7, IPOLLING_READ));
    ChkA(_spPolling->Add(9, IPOLLING_READ));
    ChkA(_spPolling->Remove(5));
    ChkA(_spPolling->Add(11, IPOLLING_READ));
    ChkA(_spPolling->Add(13, IPOLLING_READ));
    ChkA(_spPolling->Remove(7));
    ChkA(_spPolling->Add(15, IPOLLING_READ));
    ChkA(_spPolling->Add(17, IPOLLING_READ));
    ChkA(_spPolling->Add(19, IPOLLING_READ));
    ChkA(_spPolling->Remove(11));
    ChkA(_spPolling->Add(21, IPOLLING_READ));
    ChkA(_spPolling->Add(23, IPOLLING_READ));
    ChkA(_spPolling->Add(25, IPOLLING_READ));
    ChkA(_spPolling->Add(27, IPOLLING_READ));
    ChkA(_spPolling->Remove(13));
    
Cleanup:
    return hr;
    
}


