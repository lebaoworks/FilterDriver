#include "ClassFactory.h"

#include <atlbase.h>
#include <atlcom.h>

#include "Shared.h"
#include "ShellExt.h"

#include "OverlayIcon.h"

ClassFactory::ClassFactory() : _ref(1) { LogDebug("NewInstance: %X -> [%4d]", this, ShellExt::DllAddRef()); }
ClassFactory::~ClassFactory() { LogDebug("DeleteInstance %X -> [%4d]", this, ShellExt::DllRelease()); }

// IUnknown
// ========

HRESULT STDMETHODCALLTYPE ClassFactory::QueryInterface(
    _In_ REFIID riid,
    _COM_Outptr_ void** ppvObject)
{
    static const QITAB qit[] = {
        QITABENT(ClassFactory, IClassFactory),
        {0, 0}
    };
    auto hr = QISearch(this, qit, riid, ppvObject);
    LogDebug("Search interface %s -> %s", ShellExt::GuidToString(riid).c_str(), SUCCEEDED(hr) ? "Found" : "Unknown");
    return hr;
}

ULONG STDMETHODCALLTYPE ClassFactory::AddRef() { return InterlockedIncrement(&_ref); }

ULONG STDMETHODCALLTYPE ClassFactory::Release()
{
    LONG ref = InterlockedDecrement(&_ref);
    if (ref == 0)
        delete this;
    return ref;
}

// IClassFactory
// =============

HRESULT STDMETHODCALLTYPE ClassFactory::CreateInstance(
    _In_opt_      IUnknown* pUnkOuter,
    _In_          REFIID    riid,
    _COM_Outptr_  void** ppvObject)
{
    *ppvObject = nullptr;
    if (pUnkOuter != NULL)
    {
        LogWarning("Not supporting aggregation");
        return CLASS_E_NOAGGREGATION;
    }

    // Query the specified interface.
    auto ext = new OverlayIcon();
    auto hr = ext->QueryInterface(riid, ppvObject);
    ext->Release();

    LogDebug("Search interface %s -> %s", ShellExt::GuidToString(riid).c_str(), SUCCEEDED(hr) ? "Found" : "Unknown");
    return hr;
}

HRESULT STDMETHODCALLTYPE ClassFactory::LockServer(
    BOOL bLock)
{
    if (bLock)
        ShellExt::DllAddRef();
    else
        ShellExt::DllRelease();
    return S_OK;
}