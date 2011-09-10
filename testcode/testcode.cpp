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


#include "commonincludes.h"
#include "stuncore.h"

#include "testdatastream.h"
#include "testreader.h"
#include "testbuilder.h"
#include "testmessagehandler.h"
#include "testcmdline.h"
#include "testintegrity.h"
#include "testclientlogic.h"
#include "testrecvfromex.h"
#include "cmdlineparser.h"
#include "oshelper.h"
#include "prettyprint.h"

void ReaderFuzzTest()
{
    uint8_t msgbytes[1500];
    ssize_t messagesize;
    CStunMessageReader reader;
    int ret;


    messagesize=0;
    while (messagesize < 1500)
    {
        ret = getchar();
        if (ret < 0)
        {
            break;
        }
        
        unsigned char ch = (unsigned char)ret;
        msgbytes[messagesize] = ch;
        messagesize++;
    }
    
    printf("Processing %d bytes\n", (int)messagesize);
    reader.AddBytes(msgbytes, messagesize);
    

    printf("Test Run (%u)\n", GetMillisecondCounter());
   
    
    exit(0);

}


void RunUnitTests()
{
    std::vector<IUnitTest*> vecTests;

    vecTests.push_back(new CTestDataStream());
    vecTests.push_back(new CTestReader());
    vecTests.push_back(new CTestBuilder());
    vecTests.push_back(new CTestIntegrity());
    vecTests.push_back(new CTestMessageHandler());
    vecTests.push_back(new CTestCmdLineParser());
    vecTests.push_back(new CTestClientLogic());
    vecTests.push_back(new CTestRecvFromEx());

    for (size_t index = 0; index < vecTests.size(); index++)
    {
        HRESULT hr = vecTests[index]->Run();
        printf("Result of %s: %s\n", vecTests[index]->GetName(), SUCCEEDED(hr)?"PASS": "FAIL");
    }
}


void PrettyPrintTest()
{
    const size_t MAX_TEXT_SIZE = 100000;
    char* buffer = new char[MAX_TEXT_SIZE];

    size_t messagesize = 0;
    
    while (messagesize < MAX_TEXT_SIZE)
    {
        int ret = getchar();
        if (ret < 0)
        {
            break;
        }
        
        buffer[messagesize] = (signed char)(ret);
        messagesize++;
    }    
    
    ::PrettyPrint(buffer, 78);
    
    delete [] buffer;
}

int main(int argc, char** argv)
{
    CCmdLineParser cmdline;
    std::string strFuzz;
    std::string strPP;
    bool fParseError = false;

    
    cmdline.AddOption("fuzz", no_argument, &strFuzz);
    cmdline.AddOption("pp", no_argument, &strPP);
    
    cmdline.ParseCommandLine(argc, argv, 1, &fParseError);
    
    if (strFuzz.size() > 0)
    {
        ReaderFuzzTest();
    }
    else if (strPP.size() > 0)
    {
        PrettyPrintTest();
    }
    else
    {
        RunUnitTests();
    }

    return 0;
}




