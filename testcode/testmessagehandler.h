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

#ifndef _TEST_MESSAGE_HANDLER_H
#define _TEST_MESSAGE_HANDLER_H

class CMockTransport : 
     public CBasicRefCount,
     public CObjectFactory<CMockTransport>,
     public IStunResponder
{
private:
    CSocketAddress m_addrs[4];

public:
    virtual HRESULT SendResponse(SocketRole roleOutput, const CSocketAddress& addr, CRefCountedBuffer& spResponse);
    virtual bool HasAddress(SocketRole role);
    virtual HRESULT GetSocketAddressForRole(SocketRole role, /*out*/ CSocketAddress* pAddr);

    CMockTransport();
    ~CMockTransport();

    HRESULT Reset();
    HRESULT ClearStream();
    
    CDataStream& GetOutputStream() {return m_outputstream;}

    HRESULT AddPP(const CSocketAddress& addr);
    HRESULT AddPA(const CSocketAddress& addr);
    HRESULT AddAP(const CSocketAddress& addr);
    HRESULT AddAA(const CSocketAddress& addr);

    CDataStream m_outputstream;
    SocketRole m_outputRole;
    CSocketAddress m_addrDestination;
    
    ADDREF_AND_RELEASE_IMPL();
};



class CMockAuthShort : 
     public CBasicRefCount,
     public CObjectFactory<CMockAuthShort>,
     public IStunAuth
{
public:
    virtual HRESULT DoAuthCheck(AuthAttributes* pAuthAttributes, AuthResponse* pResponse);
    ADDREF_AND_RELEASE_IMPL();
};


class CMockAuthLong : 
     public CBasicRefCount,
     public CObjectFactory<CMockAuthLong>,
     public IStunAuth
{
public:
    virtual HRESULT DoAuthCheck(AuthAttributes* pAuthAttributes, AuthResponse* pResponse);
    ADDREF_AND_RELEASE_IMPL();
};



class CTestMessageHandler : public IUnitTest
{
private:
    CRefCountedPtr<CMockTransport> _spTransport;
    CRefCountedPtr<CMockAuthShort> _spAuthShort;
    CRefCountedPtr<CMockAuthLong> _spAuthLong;
    
    
    HRESULT InitBindingRequest(CStunMessageBuilder& builder);
    HRESULT ValidateMappedAddress(CStunMessageReader& reader, const CSocketAddress& addr);
    HRESULT ValidateOriginAddress(CStunMessageReader& reader, SocketRole socketExpected);
    HRESULT ValidateResponseAddress(const CSocketAddress& addr);
    
public:
    CTestMessageHandler();
    HRESULT Test1();
    HRESULT Test2();
    HRESULT Test3();
    HRESULT Test4();
    HRESULT Run();

    UT_DECLARE_TEST_NAME("CTestMessageHandler");
};


#endif
