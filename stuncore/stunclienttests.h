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



#ifndef STUNCLIENTTESTS_H
#define	STUNCLIENTTESTS_H

#include "stuntypes.h"
#include "buffer.h"
#include "stunbuilder.h"

struct StunClientLogicConfig;
struct StunClientResults;




class IStunClientTest
{
public:
    virtual HRESULT Init(StunClientLogicConfig* pConfig, StunClientResults* pResults) = 0;
    virtual void PreRunCheck() = 0; // gives a test a chance to complete right away based on the results of a previous test
    virtual bool IsReadyToRun() = 0;
    virtual HRESULT GetMessage(CRefCountedBuffer& spMsg, CSocketAddress* pAddrDest) = 0;
    virtual HRESULT ProcessResponse(CRefCountedBuffer& spMsg, CSocketAddress& addrRemote, CSocketAddress& addrLocal) = 0;
    virtual void NotifyTimeout() = 0;
    virtual bool IsCompleted() = 0;
};


class CStunClientTestBase : public IStunClientTest
{
protected:
    bool _fInit;
    StunClientLogicConfig* _pConfig;
    StunClientResults* _pResults;
    bool _fCompleted;
    StunTransactionId _transid;
    
    
    HRESULT StartBindingRequest(CStunMessageBuilder& builder);
    
    HRESULT BasicReaderValidation(CRefCountedBuffer& spMsg, CStunMessageReader& reader);
    
    
public:
    CStunClientTestBase();
    virtual HRESULT Init(StunClientLogicConfig* pConfig, StunClientResults* pResults);
    virtual void PreRunCheck();
    virtual bool IsCompleted();
};


class CBasicBindingTest : public CStunClientTestBase
{
public:
    
    bool IsReadyToRun();
    HRESULT GetMessage(CRefCountedBuffer& spMsg, CSocketAddress* pAddrDest) ;
    HRESULT ProcessResponse(CRefCountedBuffer& spMsg, CSocketAddress& addrRemote, CSocketAddress& addrLocal) ;
    void NotifyTimeout();
};

class CBehaviorTest : public CStunClientTestBase
{
protected:
    bool _fIsTest3;
    
public:
    CBehaviorTest();
    
    void PreRunCheck();
    bool IsReadyToRun();
    HRESULT GetMessage(CRefCountedBuffer& spMsg, CSocketAddress* pAddrDest) ;
    HRESULT ProcessResponse(CRefCountedBuffer& spMsg, CSocketAddress& addrRemote, CSocketAddress& addrLocal) ;
    void NotifyTimeout();
    
    void RunAsTest3(bool fSetAsTest3);
};


class CFilteringTest : public CStunClientTestBase
{
protected:
    bool _fIsTest3;
    
public:
    CFilteringTest();

    bool IsReadyToRun();
    HRESULT GetMessage(CRefCountedBuffer& spMsg, CSocketAddress* pAddrDest) ;
    HRESULT ProcessResponse(CRefCountedBuffer& spMsg, CSocketAddress& addrRemote, CSocketAddress& addrLocal) ;
    void NotifyTimeout();
    
    void RunAsTest3(bool fSetAsTest3);
};



#endif	/* STUNCLIENTTESTS_H */

