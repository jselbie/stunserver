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

#ifndef _OBJECTFACTORY_H
#define	_OBJECTFACTORY_H





template <typename T>
class CObjectFactory
{
public:

    template <typename X>
    static HRESULT CreateInstanceNoInit(X** ppInstance)
    {
        T* pT = NULL;

        if (ppInstance == NULL)
        {
            return E_INVALIDARG;
        }

        pT = new T();
        if (pT == NULL)
        {
            return E_OUTOFMEMORY;
        }

        pT->AddRef();
        *ppInstance = pT;
        return S_OK;
    }
    

    template <typename I>
    static HRESULT CreateInstance(I** ppI)
    {
        T* pInstance = NULL;
        HRESULT hr = S_OK;

        ChkIf(ppI == NULL, E_NOINTERFACE);
        Chk(CreateInstanceNoInit(&pInstance));
        Chk(pInstance->Initialize());
        *ppI = pInstance;
        pInstance = NULL;
    Cleanup:
        // Cleanup
        if (pInstance)
        {
            pInstance->Release();
            pInstance = NULL;
        }
        return hr;
    }

    template <typename A, typename I>
    static HRESULT CreateInstance(A paramA, I** ppI)
    {
        T* pInstance = NULL;
        HRESULT hr = S_OK;

        ChkIf(ppI == NULL, E_NOINTERFACE);
        Chk(CreateInstanceNoInit(&pInstance));
        Chk(pInstance->Initialize(paramA));
        *ppI = pInstance;
        pInstance = NULL;
    Cleanup:
        // Cleanup
        if (pInstance)
        {
            pInstance->Release();
            pInstance = NULL;
        }
        return hr;
    }

    template <typename A, typename B, typename I>
    static HRESULT CreateInstance(A paramA, B paramB, I** ppI)
    {
        T* pInstance = NULL;
        HRESULT hr = S_OK;

        ChkIf(ppI == NULL, E_NOINTERFACE);
        Chk(CreateInstanceNoInit(&pInstance));
        Chk(pInstance->Initialize(paramA, paramB));
        *ppI = pInstance;
        pInstance = NULL;
    Cleanup:
        // Cleanup
        if (pInstance)
        {
            pInstance->Release();
            pInstance = NULL;
        }
        return hr;
    }

};



#endif	/* _OBJECTFACTORY_H */

