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




#ifndef STRINGHELPER_H
#define	STRINGHELPER_H

#include <string>

namespace StringHelper
{
    
    bool IsNullOrEmpty(const char* psz);
    
    void ToLower(std::string& str);
    void Trim(std::string& str);
    
    int ValidateNumberString(const char* psz, int nMinValue, int nMaxValue, int* pnResult);    
}



#endif	/* STRINGHELPER_H */

