/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   testcrc32.cpp
 * Author: jselbie
 * 
 * Created on November 5, 2018, 12:07 AM
 */

#include "commonincludes.hpp"
#include "crc32.h"
#include "unittest.h"
#include "testcrc32.h"

struct crc_test
{
    const char* psz;
    uint32_t expected;
};

static crc_test g_tests[] = {
    {"", 0},
    {"The quick brown fox jumped over the lazy dog.", 0x82a34642}    
};


HRESULT CTestCRC32::Run()
{
    size_t numTests = ARRAYSIZE(g_tests);
    HRESULT hr = S_OK;
    
    for (size_t i = 0; i < numTests; i++)
    {
        uint32_t val = crc32(0, (uint8_t*)(g_tests[i].psz), strlen(g_tests[i].psz));
        ChkIf(val != g_tests[i].expected, E_FAIL);
    }
    
Cleanup:
    return hr;
}

