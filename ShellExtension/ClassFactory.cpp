#include "ClassFactory.h"

#include <atlbase.h>
#include <atlcom.h>

#include <Shared.h>
#include <ShellExt.h>

#include "ContextMenu.h"

ClassFactory::ClassFactory() : _refCount(1) { Log("NewInstance: %X", this); ShellExt::DllAddRef(); }
ClassFactory::~ClassFactory() { Log("DeleteInstance %X", this); ShellExt::DllRelease(); }

HRESULT STDMETHODCALLTYPE ClassFactory::QueryInterface(REFIID riid, LPVOID FAR* ppv)
{
    Log("riid = %s", ShellExt::GuidToString(riid).c_str());
    if (ppv == NULL)
        return E_INVALIDARG;

    static const QITAB qit[] = {
        QITABENT(ClassFactory, IClassFactory),
        {0, 0}
    };
    auto hr = QISearch(this, qit, riid, ppv);
    if (SUCCEEDED(hr))
        Log("Found interface %s", ShellExt::GuidToString(riid).c_str());
    else
        Log("Unkonwn interface %s", ShellExt::GuidToString(riid).c_str());
    return hr;
}

ULONG STDMETHODCALLTYPE ClassFactory::AddRef() { return InterlockedIncrement(&_refCount); }

ULONG STDMETHODCALLTYPE ClassFactory::Release()
{
    LONG refCount = InterlockedDecrement(&_refCount);
    if (refCount == 0)
        delete this;
    return refCount;
}

HRESULT STDMETHODCALLTYPE ClassFactory::CreateInstance(LPUNKNOWN pUnkOuter, REFIID riid, LPVOID FAR* ppv)
{
    Log("riid = %s", ShellExt::GuidToString(riid).c_str());
    if (ppv == NULL)
        return E_INVALIDARG;
    if (pUnkOuter != NULL)
        return CLASS_E_NOAGGREGATION;

    // Query the specified interface.
    auto ext = new ContextMenu();
    auto hr = ext->QueryInterface(riid, ppv);
    ext->Release();

    if (SUCCEEDED(hr))
        Log("Found interface %s", ShellExt::GuidToString(riid).c_str());
    else
        Log("Unkonwn interface %s", ShellExt::GuidToString(riid).c_str());
    return hr;
}

HRESULT STDMETHODCALLTYPE ClassFactory::LockServer(BOOL bLock)
{
    Log("Instance: %X, bLock = %d", this, bLock);
    if (bLock)
        ShellExt::DllAddRef();
    else
        ShellExt::DllRelease();
    return S_OK;
}