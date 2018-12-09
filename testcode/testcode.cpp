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
#include "stuncore.h"

#include "testdatastream.h"
#include "testreader.h"
#include "testbuilder.h"
#include "testmessagehandler.h"
#include "testcmdline.h"
#include "testintegrity.h"
#include "testclientlogic.h"
#include "testrecvfromex.h"
#include "testfasthash.h"
#include "testcrc32.h"
#include "cmdlineparser.h"
#include "oshelper.h"
#include "prettyprint.h"
#include "polling.h"
#include "testpolling.h"
#include "testratelimiter.h"

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
    std::vector<std::shared_ptr<IUnitTest>> vecTests;
    
    std::shared_ptr<CTestDataStream> spTestDataStream(new CTestDataStream);
    std::shared_ptr<CTestReader> spTestReader(new CTestReader);
    std::shared_ptr<CTestBuilder> spTestBuilder(new CTestBuilder);
    std::shared_ptr<CTestIntegrity> spTestIntegrity(new CTestIntegrity);
    std::shared_ptr<CTestMessageHandler> spTestMessageHandler(new CTestMessageHandler);
    std::shared_ptr<CTestCmdLineParser> spTestCmdLineParser(new CTestCmdLineParser);
    std::shared_ptr<CTestClientLogic> spTestClientLogic(new CTestClientLogic);
    std::shared_ptr<CTestRecvFromExIPV4> spTestRecvFromEx4(new CTestRecvFromExIPV4);
    std::shared_ptr<CTestRecvFromExIPV6> spTestRecvFromEx6(new CTestRecvFromExIPV6);
    std::shared_ptr<CTestFastHash> spTestFastHash(new CTestFastHash);
    std::shared_ptr<CTestPolling> spTestPolling(new CTestPolling);
    std::shared_ptr<CTestRateLimiter> spTestRateLimiter(new CTestRateLimiter);
    std::shared_ptr<CTestCRC32> spTestCRC32(new CTestCRC32);

    vecTests.push_back(spTestDataStream);
    vecTests.push_back(spTestReader);
    vecTests.push_back(spTestBuilder);
    vecTests.push_back(spTestIntegrity);
    vecTests.push_back(spTestMessageHandler);
    vecTests.push_back(spTestCmdLineParser);
    vecTests.push_back(spTestClientLogic);
    vecTests.push_back(spTestRecvFromEx4);
    vecTests.push_back(spTestRecvFromEx6);
    vecTests.push_back(spTestFastHash);
    vecTests.push_back(spTestPolling);
    vecTests.push_back(spTestRateLimiter);
    vecTests.push_back(spTestCRC32);

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

void benchmark_test(int threads);

int main(int argc, char** argv)
{
    CCmdLineParser cmdline;
    std::string strFuzz;
    std::string strPP;
    std::string strBench;
    bool fParseError = false;

    
    cmdline.AddOption("fuzz", no_argument, &strFuzz);
    cmdline.AddOption("pp", no_argument, &strPP);
    cmdline.AddOption("bench", optional_argument, &strBench);
    
    cmdline.ParseCommandLine(argc, argv, 1, &fParseError);
    
    if (strFuzz.size() > 0)
    {
        ReaderFuzzTest();
    }
    else if (strPP.size() > 0)
    {
        PrettyPrintTest();
    }
    else if (strBench.size() > 0)
    {
        int threads = atoi(strBench.c_str());
        benchmark_test(threads);
    }
    else
    {
        RunUnitTests();
    }

    return 0;
}




