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
    CRefCountedPtr<CMockAuthShort> _spAuthShort;
    CRefCountedPtr<CMockAuthLong> _spAuthLong;
    
    
    CSocketAddress _addrLocal;
    CSocketAddress _addrMapped;
    CSocketAddress _addrServerPP;
    CSocketAddress _addrServerPA;
    CSocketAddress _addrServerAP;
    CSocketAddress _addrServerAA;
    
    
    CSocketAddress _addrDestination;
    CSocketAddress _addrMappedExpected;
    CSocketAddress _addrOriginExpected;
    
    
    
    
    void ToAddr(const char* pszIP, uint16_t port, CSocketAddress* pAddr);
    
    void InitTransportAddressSet(TransportAddressSet& tas, bool fRolePP, bool fRolePA, bool fRoleAP, bool fRoleAA);
    
    HRESULT InitBindingRequest(CStunMessageBuilder& builder);

    HRESULT ValidateMappedAddress(CStunMessageReader& reader, const CSocketAddress& addrExpected, bool fLegacyOnly);
    HRESULT ValidateResponseOriginAddress(CStunMessageReader& reader, const CSocketAddress& addrExpected);
    
    HRESULT ValidateOtherAddress(CStunMessageReader& reader, const CSocketAddress& addrExpected);
    
    HRESULT SendHelper(CStunMessageBuilder& builderRequest, CStunMessageReader* pReaderResponse, IStunAuth* pAuth);
    
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
