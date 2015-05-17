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
#include "cmdlineparser.h"
#include "oshelper.h"
#include "prettyprint.h"
#include "polling.h"
#include "testpolling.h"
#include "testatomichelpers.h"
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
    std::vector<IUnitTest*> vecTests;
    
    boost::shared_ptr<CTestDataStream> spTestDataStream(new CTestDataStream);
    boost::shared_ptr<CTestReader> spTestReader(new CTestReader);
    boost::shared_ptr<CTestBuilder> spTestBuilder(new CTestBuilder);
    boost::shared_ptr<CTestIntegrity> spTestIntegrity(new CTestIntegrity);
    boost::shared_ptr<CTestMessageHandler> spTestMessageHandler(new CTestMessageHandler);
    boost::shared_ptr<CTestCmdLineParser> spTestCmdLineParser(new CTestCmdLineParser);
    boost::shared_ptr<CTestClientLogic> spTestClientLogic(new CTestClientLogic);
    boost::shared_ptr<CTestRecvFromExIPV4> spTestRecvFromEx4(new CTestRecvFromExIPV4);
    boost::shared_ptr<CTestRecvFromExIPV6> spTestRecvFromEx6(new CTestRecvFromExIPV6);
    boost::shared_ptr<CTestFastHash> spTestFastHash(new CTestFastHash);
    boost::shared_ptr<CTestPolling> spTestPolling(new CTestPolling);
    boost::shared_ptr<CTestAtomicHelpers> spTestAtomicHelpers(new CTestAtomicHelpers);
    boost::shared_ptr<CTestRateLimiter> spTestRateLimiter(new CTestRateLimiter);

    vecTests.push_back(spTestDataStream.get());
    vecTests.push_back(spTestReader.get());
    vecTests.push_back(spTestBuilder.get());
    vecTests.push_back(spTestIntegrity.get());
    vecTests.push_back(spTestMessageHandler.get());
    vecTests.push_back(spTestCmdLineParser.get());
    vecTests.push_back(spTestClientLogic.get());
    vecTests.push_back(spTestRecvFromEx4.get());
    vecTests.push_back(spTestRecvFromEx6.get());
    vecTests.push_back(spTestFastHash.get());
    vecTests.push_back(spTestPolling.get());
    vecTests.push_back(spTestAtomicHelpers.get());
    vecTests.push_back(spTestRateLimiter.get());


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




