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

#ifndef STUNSOCKET_H
#define	STUNSOCKET_H



class CStunSocket
{
private:
    int _sock;
    CSocketAddress _addrlocal;
    CSocketAddress _addrremote;
    SocketRole _role;
    
    CStunSocket(const CStunSocket&) {;}
    void operator=(const CStunSocket&) {;}
    
    HRESULT InitCommon(int socktype, const CSocketAddress& addrlocal, SocketRole role, bool fSetReuseFlag);
    
    void Reset();
    
    HRESULT EnablePktInfoImpl(int level, int option1, int option2, bool fEnable);
    HRESULT EnablePktInfo_IPV4(bool fEnable);
    HRESULT EnablePktInfo_IPV6(bool fEnable);
    
    HRESULT SetV6Only(int sock);
    
public:

    CStunSocket();
    ~CStunSocket();
    
    void Close();
    
    bool IsValid();
    
    HRESULT Attach(int sock);
    int Detach();
    
    int GetSocketHandle() const;
    const CSocketAddress& GetLocalAddress() const;
    const CSocketAddress& GetRemoteAddress() const;
    
    SocketRole GetRole() const;
    void SetRole(SocketRole role);
    
    HRESULT EnablePktInfoOption(bool fEnable);
    HRESULT SetNonBlocking(bool fEnable);
    
    
    void UpdateAddresses();
    
    HRESULT UDPInit(const CSocketAddress& local, SocketRole role, bool fSetReuseFlag);
    HRESULT TCPInit(const CSocketAddress& local, SocketRole role, bool fSetReuseFlag);
};

typedef boost::shared_ptr<CStunSocket> CRefCountedStunSocket;



#endif	/* STUNSOCKET_H */

