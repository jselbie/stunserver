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
#include <getopt.h>

#include "cmdlineparser.h"


const option* CCmdLineParser::GenerateOptions()
{
    _options.clear();

    size_t len = _listOptionDetails.size();
    for (size_t index = 0; index < len; index++)
    {
        option opt = {};
        opt.has_arg = _listOptionDetails[index].has_arg;
        
        // Solaris 11 (released in 2011), only sees fit to include header files from 2004 where "option::name" is just a char* and not const char*
        opt.name = (char*)(_listOptionDetails[index].strName.c_str());
        
        
        _options.push_back(opt);
    }
    
    option zero = {0,0,0,0};
    _options.push_back(zero);


    return &_options.front();
}



HRESULT CCmdLineParser::AddOption(const char* pszName, int has_arg, std::string* pStrResult)
{
    HRESULT hr = S_OK;
    OptionDetail od;

    ChkIfA(pszName==NULL, E_INVALIDARG);
    ChkIfA(has_arg < 0, E_INVALIDARG);
    ChkIfA(has_arg > 2, E_INVALIDARG);
    ChkIfA(pStrResult==NULL, E_INVALIDARG);

    od.has_arg = has_arg;
    od.pStrResult = pStrResult;
    od.strName = pszName;

    _listOptionDetails.push_back(od);

Cleanup:
    return hr;

}

HRESULT CCmdLineParser::AddNonOption(std::string* pStrResult)
{
    _namelessArgs.push_back(pStrResult);
    return S_OK;
}


HRESULT CCmdLineParser::ParseCommandLine(int argc, char** argv, int startindex, bool* pErrorFlag)
{

    int oldopterr = ::opterr;
    size_t offset  = 0;


    ::opterr = 0;
    ::optind = startindex;



    const option* longopts = GenerateOptions();

    if (pErrorFlag)
    {
        *pErrorFlag = false;
    }

    while (true)
    {
        int index = 0;
        int ret;
        
        ret = ::getopt_long_only(argc, argv, "", longopts, &index);

        if (ret < 0)
        {
            break;
        }

        if ((ret == '?') || (ret == ':'))
        {
            if (pErrorFlag)
            {
                *pErrorFlag = true;
            }
            continue;
        }

        if ((longopts[index].has_arg != 0) && (optarg != NULL))
        {
           _listOptionDetails[index].pStrResult->assign(optarg);
        }
        else
        {
            _listOptionDetails[index].pStrResult->assign("1");
        }
    }
    
    offset = 0;
    for (int j = ::optind; j < argc; j++)
    {
        if (strcmp("--", argv[j]) == 0)
        {
            continue;
        }
        
        if (_namelessArgs.size() <= offset)
        {
            // should we set the error flag if we don't have a binding for this arg?
            break;
        }
        
        _namelessArgs[offset]->assign(argv[j]);
        offset++;
    }
    

    ::opterr = oldopterr;
    return S_OK;
}

