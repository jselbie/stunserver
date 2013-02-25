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



#ifndef _TESTRECVFROMEX_H
#define _TESTRECVFROMEX_H



#include "unittest.h"

class CTestRecvFromEx
{
public:
    static HRESULT DoTest(bool fUseIPV6);
};

class CTestRecvFromExIPV4  : public IUnitTest
{
    HRESULT Run();
    UT_DECLARE_TEST_NAME("CTestRecvFromEx(IPV4)");
};

class CTestRecvFromExIPV6  : public IUnitTest
{
    HRESULT Run();
    UT_DECLARE_TEST_NAME("CTestRecvFromEx(IPV6)");
};


#endif
