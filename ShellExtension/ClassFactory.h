#pragma once

#include <Windows.h>
#include <ShlObj.h>

class ClassFactory : public IClassFactory
{
private:
    ULONG _refCount;
public:
    ClassFactory();
    ~ClassFactory();

    //IUnknown interface
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, LPVOID FAR*);
    ULONG   STDMETHODCALLTYPE AddRef();
    ULONG   STDMETHODCALLTYPE Release();

    //IClassFactory interface
    HRESULT STDMETHODCALLTYPE CreateInstance(LPUNKNOWN, REFIID, LPVOID FAR*);
    HRESULT STDMETHODCALLTYPE LockServer(BOOL);
};

