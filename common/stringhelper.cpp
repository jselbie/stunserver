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
#include <stdlib.h>
#include <vector>
#include <string>
#include "stringhelper.h"



#define ISWHITESPACE(ch) (((ch >= 9)&&(ch<=0xd))||(ch== ' '))

namespace StringHelper
{
    bool IsNullOrEmpty(const char* psz)
    {
        return ((psz == NULL) || (psz[0] == '\0'));
    }


    void ToLower(std::string& str)
    {
        const char* psz = str.c_str();
        size_t length = str.length();
        std::string str2;
        const int diff = ('a' - 'A');

        if ((psz == NULL) || (length == 0))
        {
            return;
        }

        str2.reserve(length);

        for (size_t index = 0; index < length; index++)
        {
            char ch = str[index];
            if ((ch >= 'A') && (ch <= 'Z'))
            {
                ch = ch + diff;
            }

            str2.push_back(ch);
        }

        str = str2;

    }


    void Trim(std::string& str)
    {
        const char* psz = str.c_str();

        if (psz == NULL)
        {
            return;
        }

        int length = str.length();
        int start = -1;
        int end = -1;
        char ch;

        for (int index = 0; index < length; index++)
        {
            ch = psz[index];

            if (ISWHITESPACE(ch))
            {
                continue;
            }
            else if (start == -1)
            {
                start = index;
                end = index;
            }
            else
            {
                end = index;
            }
        }

        if (start != -1)
        {
            str = str.substr(start, end-start+1);
        }
    }


    int ValidateNumberString(const char* psz, int nMinValue, int nMaxValue, int* pnResult)
    {
        int nVal = 0;

        if (IsNullOrEmpty(psz) || (pnResult==NULL))
        {
            return -1;
        }

        nVal = atoi(psz);

        if(nVal < nMinValue) return -1;
        if(nVal > nMaxValue) return -1;

        *pnResult = nVal;
        return 0;

    }

}

