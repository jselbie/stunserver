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


#ifndef LOGGING_H
#define LOGGING_H



const uint32_t LL_ALWAYS = 0;      // only help messages, output the user expects to see, and critical error messages
const uint32_t LL_DEBUG = 1;    // messages helpful for debugging
const uint32_t LL_VERBOSE = 2;     // every packet
const uint32_t LL_VERBOSE_EXTREME = 3; // every packet and all the details

namespace Logging
{
    uint32_t GetLogLevel();
    void SetLogLevel(uint32_t level);
    void LogMsg(uint32_t level, const char* pszFormat, ...);
}

#endif
