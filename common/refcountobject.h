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


#ifndef _REFCOUNTOBJECT_H
#define	_REFCOUNTOBJECT_H






class IRefCounted
{
public:
    virtual int AddRef() = 0;
    virtual int Release() = 0;
};


class CBasicRefCount
{
protected:
    int m_nRefs;

public:
    CBasicRefCount();
    virtual ~CBasicRefCount();

    int InternalAddRef();
    int InternalRelease();

    virtual void OnFinalRelease();
};

#define ADDREF_AND_RELEASE_IMPL() \
    inline int AddRef() {return InternalAddRef();} \
    inline int Release() {return InternalRelease();}




template <class T>
class CRefCountedPtr
{
protected:
    T* m_ptr;

public:


    CRefCountedPtr() : m_ptr(NULL)
    {
        ;
    }

    CRefCountedPtr(T* ptr) : m_ptr(ptr)
    {
        if (m_ptr)
        {
            m_ptr->AddRef();
        }
    }

    CRefCountedPtr(const CRefCountedPtr<T>& sp) : m_ptr(sp.m_ptr)
    {
         if (m_ptr)
         {
             m_ptr->AddRef();
         }
    }

    ~CRefCountedPtr()
    {
        if (m_ptr)
        {
            m_ptr->Release();
            m_ptr = NULL;
        }
    }

    T* GetPointer()
    {
        return m_ptr;
    }

    operator T*() const
    {
        return m_ptr;
    }

    // std::vector chokes because it references &element to infer behavior
    // Use GetPointerPointer instead
    //T** operator&()
    //{
    //    return &m_ptr;
    //}

    T** GetPointerPointer()
    {
         return &m_ptr;
    }


    T* operator->() const
    {
        return m_ptr;
    }

    T* operator = (T* ptr)
    {
        if (ptr)
        {
            ptr->AddRef();
        }
        if (m_ptr)
        {
            m_ptr->Release();
        }
        m_ptr = ptr;
        return m_ptr;
    }

    T* operator = (const CRefCountedPtr<T>& sp)
    {
        if (sp.m_ptr)
        {
            sp.m_ptr->AddRef();
        }
        if (m_ptr)
        {
            m_ptr->Release();
        }
        m_ptr = sp.m_ptr;
        return m_ptr;
    }

    bool operator ! () const
    {
        return (m_ptr == NULL);
    }

    bool operator != (T* ptr) const
    {
        return (ptr != m_ptr);
    }

    bool operator == (T* ptr) const
    {
        return (ptr == m_ptr);
    }

    // manual release
    void ReleaseAndClear()
    {
        if (m_ptr)
        {
            m_ptr->Release();
            m_ptr = NULL;
        }
    }

    void Attach(T* ptr)
    {
        if (m_ptr)
        {
            m_ptr->Release();
        }
        m_ptr = ptr;
    }

    T* Detach()
    {
        T* ptr = m_ptr;
        m_ptr = NULL;
        return ptr;
    }

    HRESULT CopyTo(T** ppT)
    {
        if (ppT == NULL)
        {
            return E_POINTER;
        }

        *ppT = m_ptr;
        if (m_ptr)
        {
            m_ptr->AddRef();
        }
        return S_OK;
    }

};



#endif	/* _REFCOUNTOBJECT_H */

