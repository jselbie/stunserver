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


// This test validates the code in stuncore\datastream.cpp
HRESULT CTestDataStream::Run()
{
    CDataStream stream;
    HRESULT hr;

    const char* str1 = "ABCDEFGHIJ";
    const char* str2 = "KLMNOPQRSTUVW";
    const char* str3 = "mnop";
    const char* str4 = "XYZ";
    char ch1= ' ', ch2=' ', ch3= ' ';


    Chk(stream.Write(str1, strlen(str1)));
    Chk(stream.Write(str2, strlen(str2)));
    Chk(stream.SeekDirect(12)); // seek to "M"
    Chk(stream.Write(str3, strlen(str3)));
    Chk(stream.SeekDirect(stream.GetSize()));
    Chk(stream.Write(str4, strlen(str4)));
    Chk(stream.WriteUint8(0));

    stream.SeekDirect(9);

    stream.ReadInt8((int8_t*)&ch1);

    ChkIf(ch1 != 'J', E_FAIL);
    Chk(stream.ReadInt8((int8_t*)&ch2));
    ChkIf(ch2 != 'K', E_FAIL);

    stream.SeekRelative(1);
    stream.ReadInt8((int8_t*)&ch3);
    ChkIf(ch3 != 'm', E_FAIL);

Cleanup:
    return hr;


}
