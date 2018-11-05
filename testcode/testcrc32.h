/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   testcrc32.h
 * Author: jselbie
 *
 * Created on November 5, 2018, 12:07 AM
 */

#ifndef TESTCRC32_H
#define TESTCRC32_H

class CTestCRC32 : public IUnitTest
{
public:
    HRESULT Run();
    UT_DECLARE_TEST_NAME("CTestCRC32");
};

#endif /* TESTCRC32_H */

