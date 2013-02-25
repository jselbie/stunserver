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

#ifndef _WIN32
#include <sys/ioctl.h>
#endif

const size_t DEFAULT_CONSOLE_WIDTH = 80;

static size_t GetConsoleWidthUnix()
{
    // this ioctl call on file id 0 (stdin) works on MacOSX and Linux.  So it's probably universal
    struct winsize ws = {};
    int ret;
    size_t retvalue = DEFAULT_CONSOLE_WIDTH;
    ret = ioctl(0, TIOCGWINSZ, &ws);
    if ((ret != -1) && (ws.ws_col > 0))
    {
        retvalue = ws.ws_col;
    }
    return retvalue;
}

size_t GetConsoleWidth()
{
#ifdef _WIN32
    return DEFAULT_CONSOLE_WIDTH;  // todo - call appropriate console apis when we port to windows
#else
    return GetConsoleWidthUnix();
#endif
}

