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
#include <openssl/hmac.h>
#include "stuncore.h"
#include "stunsocket.h"
#include "stunsocketthread.h"
#include "server.h"
#include "sampleauthprovider.h"


static const char* c_szPrivateKey = "Change this string if you are going to use this code";
static const char* c_szRealm = "YourRealmNameHere";


static HRESULT LookupPassword(bool fWithRealm, const char* pszUserName, const char* pszRealm, char* pszPassword)
{
    const char* users[] =     {"bruce",    "steve",     "nicko",           "dave",        "adrian"};
    const char* passwords[] = {"AcesHigh", "2MinToMid", "fearofthedark!",  "#ofthebeast", "Run2TheHills" };
    
    if (fWithRealm)
    {
        if ((pszRealm == NULL) || (strcmp(pszRealm, c_szRealm)))
        {
            return E_FAIL;
        }
    }
    
    if (pszUserName == NULL)
    {
        return E_FAIL;
    }
    
    for (size_t index = 0; index < ARRAYSIZE(users); index++)
    {
        if (strcmp(pszUserName, users[index]))
        {
            continue;
        }
        
        strcpy(pszPassword, passwords[index]);
        return S_OK;
    }
    
    return E_FAIL;
}




HRESULT CShortTermAuth::DoAuthCheck(AuthAttributes* pAuthAttributes, AuthResponse* pResponse)
{
    // if you want to authenticate in "short term credential mode", then this function needs to return
    // a password for the passed in user name
    
    // short-term example
    
    // indicate to the server we are returning a short-term credential so it knows to use
    // only the pResponse->szPassword field for computing the message integrity
    pResponse->authCredMech = AuthCredShortTerm;
    
    if (pAuthAttributes->fMessageIntegrityPresent == false)
    {
        // RFC 5389 indicates to send back a "400" if there is no message integrity.  That's
        // what "Reject" will signal to the server to respond with
        pResponse->responseType = Reject;
        return S_OK;
    }
    
    if (SUCCEEDED(LookupPassword(false, pAuthAttributes->szUser, NULL, pResponse->szPassword)))
    {
        // Returning "AllowConditional" indicates that the request can be accepted if and only if the
        // message integrity attribute can be validated with the value placed into pResponse->szPassword
        pResponse->responseType = AllowConditional;
        return S_OK;
    }
    
    // If it's not a valid user (or no password could be found), just return Unauthorized.
    // This will result in a 401 getting sent back
    pResponse->responseType = Unauthorized;
    
    return S_OK;
}

HRESULT CLongTermAuth::DoAuthCheck(AuthAttributes* pAuthAttributes, AuthResponse* pResponse)
{
    HRESULT hr = S_OK;
    
    pResponse->authCredMech = AuthCredLongTerm;
    
    // Go ahead and generate a new nonce and set the realm.
    // The realm and nonce attributes will only get sent back to the client when there is an auth error
    CreateNonce(pResponse->szNonce);
    strcpy(pResponse->szRealm, c_szRealm);

    // if we're missing any authentication attributes, then just return back a 401.
    // This will trigger the server to send back the nonce and realm attributes to the client within the 401 resposne
    if ((pAuthAttributes->fMessageIntegrityPresent == false) || (pAuthAttributes->szNonce[0] == 0) || (pAuthAttributes->szUser[0] == 0))
    {
        pResponse->responseType = Unauthorized;
        return S_OK;
    }

    // copy the user's password into szPassword
    hr = LookupPassword(true, pAuthAttributes->szUser, pAuthAttributes->szNonce, pResponse->szPassword);
    if (FAILED(hr))
    {
        // if not a valid user, same as before.  Just send back a 401
        pResponse->responseType = Unauthorized;
        return S_OK;
        
    }
    
    // validate the nonce
    if (FAILED(ValidateNonce(pAuthAttributes->szNonce)))
    {
        pResponse->responseType = StaleNonce;
        return S_OK;
    }
    
    // returning "AllowConditional" indicates that the request can be accepted if and only if the
    // message integrity attribute can be validated with the value placed into pResponse->szPassword
    pResponse->responseType = AllowConditional;
    
    return S_OK;
}




void CLongTermAuth::HmacToString(uint8_t* hmacresult, char* pszResult)
{
    sprintf(pszResult, "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x", 
    hmacresult[0], hmacresult[1], hmacresult[2], hmacresult[3], hmacresult[4], 
    hmacresult[5], hmacresult[6], hmacresult[7], hmacresult[8], hmacresult[9], 
    hmacresult[10], hmacresult[11], hmacresult[12], hmacresult[13], hmacresult[14], 
    hmacresult[15], hmacresult[16], hmacresult[17], hmacresult[18], hmacresult[19]);
}

HRESULT CLongTermAuth::CreateNonce(char *pszNonce)
{
    // This is a sample "nonce provider".  Our implementation of "nonce" is just a a string
    // indicating a timestamp followed by an HMAC hash of the timestamp.
    // Validation of a nonce is to just to make sure the timestamp isn't more than a couple
    // of minutes old and that the hmac hash matches
    
    // If you use this code, make sure you change the value of c_szPrivateKey!
    
    time_t thetime = time(NULL);
    uint8_t hmacresult[20] = {};
    char szHMAC[20*2+1];
    char szTime[sizeof(time_t)*4];
    unsigned int len = ARRAYSIZE(hmacresult);
    
    sprintf(szTime, "%u:", (unsigned int)thetime);
    
    HMAC(::EVP_sha1(), (unsigned char*)c_szPrivateKey, strlen(c_szPrivateKey), (unsigned char*)szTime, strlen(szTime), hmacresult, &len);
    
    HmacToString(hmacresult, szHMAC);

    strcpy(pszNonce, szTime);
    strcat(pszNonce, szHMAC);

    return S_OK;
}

HRESULT CLongTermAuth::ValidateNonce(char* pszNonce)
{
    time_t thecurrenttime = time(NULL);
    time_t thetime;
    uint8_t hmacresult[20] = {};
    char szHMAC[20*2+1];
    char szNonce[100];
    char *pRightHalf = NULL;
    time_t diff;
    unsigned int len = ARRAYSIZE(hmacresult);
    
    strncpy(szNonce, pszNonce, ARRAYSIZE(szNonce));
    szNonce[ARRAYSIZE(szNonce)-1] = 0;
    
    pRightHalf = strstr(szNonce, ":");
    
    if (pRightHalf == NULL)
    {
        return E_FAIL;
    }
    
    *pRightHalf++ = 0;

    thetime = atoi(szNonce);
    
    diff = thecurrenttime - thetime;
    if (((thecurrenttime - thetime) > 120) || (diff < 0))
    {
        // nonce is more than 2 minutes old - reject
        return E_FAIL;
    }
    
    // nonce timestamp is valid, but was it signed by this server?
    HMAC(::EVP_sha1(), (unsigned char*)c_szPrivateKey, strlen(c_szPrivateKey), (unsigned char*)szNonce, strlen(szNonce), hmacresult, &len);
    HmacToString(hmacresult, szHMAC);
    if (strcmp(szHMAC, pRightHalf))
    {
        return E_FAIL;
    }
    
    return S_OK;
}







