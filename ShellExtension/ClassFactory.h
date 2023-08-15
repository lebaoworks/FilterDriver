#pragma once

#include <Windows.h>
#include <ShlObj.h>

class ClassFactory : public IClassFactory
{
private:
    ULONG _ref;
public:
    ClassFactory();
    ~ClassFactory();

    // IUnknown interface
    HRESULT STDMETHODCALLTYPE QueryInterface(
        _In_         REFIID riid,
        _COM_Outptr_ void** ppvObject);
    ULONG   STDMETHODCALLTYPE AddRef();
    ULONG   STDMETHODCALLTYPE Release();

    // IClassFactory interface
    HRESULT STDMETHODCALLTYPE CreateInstance(
        _In_opt_     IUnknown* pUnkOuter,
        _In_         REFIID    riid,
        _COM_Outptr_ void**    ppvObject);
    HRESULT STDMETHODCALLTYPE LockServer(
        BOOL fLock);
};

