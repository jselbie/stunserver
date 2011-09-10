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


#ifndef SOCKETROLE_H
#define	SOCKETROLE_H


enum SocketRole
{
    RolePP=0,  // primary address, primary port
    RolePA=1,  // primary address, alternate port
    RoleAP=2,  // alternate address, primary port
    RoleAA=3   // alternate address, alternate port
};


inline bool IsValidSocketRole(SocketRole sr)
{
    return ((sr >= 0) && (sr <= 3));
}

inline SocketRole SocketRoleSwapPort(SocketRole sr)
{
    return (SocketRole)(((uint16_t)sr) ^ 0x01);
}

inline SocketRole SocketRoleSwapIP(SocketRole sr)
{
    return (SocketRole)(((uint16_t)sr) ^ 0x02);
}


#endif	/* SOCKETROLE_H */

