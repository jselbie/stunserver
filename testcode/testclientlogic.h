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


#ifndef TESTCLIENTLOGIC_H
#define	TESTCLIENTLOGIC_H

#include "unittest.h"


#include "testmessagehandler.h"


class CTestClientLogic : public IUnitTest
{
private:
    
    CSocketAddress _addrServerPP;
    CSocketAddress _addrServerPA;
    CSocketAddress _addrServerAP;
    CSocketAddress _addrServerAA;
    
    CSocketAddress _addrLocal;
    
    CSocketAddress _addrMappedPP; // what PP returns
    CSocketAddress _addrMappedPA; // what PA returns
    CSocketAddress _addrMappedAP; // what AP returns
    CSocketAddress _addrMappedAA; // what AA returns
    
    bool _fAllowChangeRequestAA;
    bool _fAllowChangeRequestPA;
    
    TransportAddressSet _tsa;
    
    
    boost::shared_ptr<CStunClientLogic> _spClientLogic;
    

        
    HRESULT ValidateBindingRequest(CRefCountedBuffer& spMsg, StunTransactionId* pTransId);
    HRESULT GenerateBindingResponseMessage(const CSocketAddress& addrMapped , const StunTransactionId& transid, CRefCountedBuffer& spMsg);
    
    HRESULT Test1();
    
    
    HRESULT TestBehaviorAndFiltering(bool fBehaviorTest, NatBehavior behavior, bool fFilteringTest, NatFiltering filtering);
    
    HRESULT CommonInit(NatBehavior behavior, NatFiltering filtering);

    
    HRESULT GetMappedAddressForDestinationAddress(const CSocketAddress& addrDest, CSocketAddress* pAddrMapped);
    SocketRole GetSocketRoleForDestinationAddress(const CSocketAddress& addrDest);
    
public:
    
    CTestClientLogic() {;}
    ~CTestClientLogic() {;}
    
    HRESULT Run();
    
    UT_DECLARE_TEST_NAME("CTestClientLogic");    
    
};



#endif	/* TESTCLIENTLOGIC_H */

