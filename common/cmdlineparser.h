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


#ifndef CMDLINEPARSER_H
#define	CMDLINEPARSER_H

#include <getopt.h>

class CCmdLineParser
{
    const option* GenerateOptions();

    std::vector<option> _options;
    
    std::vector<std::string*> _namelessArgs;


    struct OptionDetail
    {
        std::string strName;
        int has_arg;
        std::string* pStrResult;
    };

    std::vector<OptionDetail> _listOptionDetails;

public:
    HRESULT AddOption(const char* pszName, int has_arg, std::string* pStrResult);
    HRESULT AddNonOption(std::string* pStrResult);
    HRESULT ParseCommandLine(int argc, char** argv, int startindex, bool* fParseError);
};


#endif	/* CMDLINEPARSER_H */

