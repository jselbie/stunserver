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
#include "oshelper.h"


static uint32_t GetMillisecondCounterUnix()
{
    uint64_t milliseconds = 0;
    uint32_t retvalue;
    timeval tv = {};
    gettimeofday(&tv, NULL);
    milliseconds = (tv.tv_sec * (unsigned long long)1000) + (tv.tv_usec / 1000);
    retvalue = (uint32_t)(milliseconds & (unsigned long long)0xffffffff);
    return retvalue;
}

uint32_t GetMillisecondCounter()
{
#ifdef _WIN32
    return GetTickCount();
#else
    return GetMillisecondCounterUnix();    
#endif
}
