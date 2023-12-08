#include "OverlayIcon.h"
#include <strsafe.h>
#include <Shlwapi.h>
#include <memory>

#include "ShellExt.h"
#include "Shared.h"
#include "Win32.h"

OverlayIcon::OverlayIcon(void) : m_cRef(1)
{
    ShellExt::DllAddRef();
}

OverlayIcon::~OverlayIcon(void)
{
    ShellExt::DllRelease();
}

#pragma region IUnknown

IFACEMETHODIMP OverlayIcon::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(OverlayIcon, IShellIconOverlayIdentifier),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) OverlayIcon::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) OverlayIcon::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }
    return cRef;
}

#pragma endregion

#pragma region IShellIconOverlayIdentifier

STDMETHODIMP OverlayIcon::GetOverlayInfo(LPWSTR pwszIconFile, int cchMax,
    int* pIndex, DWORD* pdwFlags)
{
    Log("GetOverlayInfo");

    if (pwszIconFile == 0)
    {
        return E_POINTER;
    }
    if (pIndex == 0)
    {
        return E_POINTER;
    }
    if (pdwFlags == 0)
    {
        return E_POINTER;
    }
    if (cchMax < 1)
    {
        return E_INVALIDARG;
    }

    StrNCpy(pwszIconFile, LR"(C:\Icon.ico)", cchMax);

    // index of icon to use in list of resources
    *pIndex = 0;
    *pdwFlags = ISIOI_ICONFILE | ISIOI_ICONINDEX;

    return S_OK;
}

STDMETHODIMP OverlayIcon::GetPriority(int* pPriority)
{
    Log("GetPriority");
    if (!pPriority)
    {
        return E_POINTER;
    }

    *pPriority = 0;
    return S_OK;
}

STDMETHODIMP OverlayIcon::IsMemberOf(LPCWSTR pwszPath, DWORD dwAttrib)
{
    std::wstring wpath = pwszPath;
    std::string path = std::string(wpath.begin(), wpath.end());
    auto ret = Win32::Registry::CheckValue("HKEY_CURRENT_USER\\Protected", path.c_str());
    
    Log("IsMemberOf %ws -> %d", pwszPath, ret);
    if (ret == ERROR_SUCCESS)
        return S_OK;
    else 
        return S_FALSE;
}

#pragma endregion

HRESULT OverlayIcon::ComponentCreator(const IID& iid, void** ppv)
{
    HRESULT hr = E_OUTOFMEMORY;

    OverlayIcon* pExt = new (std::nothrow) OverlayIcon();
    if (pExt)
    {
        hr = pExt->QueryInterface(iid, ppv);
        pExt->Release();
    }

    return hr;
}
