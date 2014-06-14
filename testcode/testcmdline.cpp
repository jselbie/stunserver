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
#include "cmdlineparser.h"
#include "unittest.h"
#include "testcmdline.h"


// This test validates that the CCmdLineParser works correctly
HRESULT CTestCmdLineParser::Run()
{
    HRESULT hr = S_OK;

    //char* argv[] = {(char*)"app", (char*)"--zebra", (char*)"--yak=123", (char*)"--walrus=456", (char*)"--vulture"};
    const char* argv[5] = {};
    argv[0] = "app";
    argv[1] = "--zebra";
    argv[2] = "--yak=123";
    argv[3] = "--walrus=456";
    argv[4] = "--vulture";


    int argc = ARRAYSIZE(argv);
    bool fErrorFlag = false;

    std::string strZebra;
    std::string strYak;
    std::string strWalrus;
    std::string strVulture;
    std::string strAardvark;


    CCmdLineParser parser;

    parser.AddOption("aardvark", 1, &strAardvark);
    parser.AddOption("vulture", 0,  &strVulture);
    parser.AddOption("walrus", 1,   &strWalrus);
    parser.AddOption("yak", 2,      &strYak);
    parser.AddOption("zebra", 2,    &strZebra);

    parser.ParseCommandLine(argc, (char**)argv, 1, &fErrorFlag);

    ChkIf(fErrorFlag, E_FAIL);

    ChkIf(strAardvark.length() > 0, E_FAIL);
    ChkIf(strVulture.length() == 0, E_FAIL);
    ChkIf(strWalrus != "456", E_FAIL);
    ChkIf(strYak != "123", E_FAIL);
    ChkIf(strZebra != "1", E_FAIL);

Cleanup:
    return hr;
}
