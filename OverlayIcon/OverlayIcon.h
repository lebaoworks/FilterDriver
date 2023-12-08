#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <windows.h>
#include <shlobj.h>
#include <string>


class OverlayIcon : IShellIconOverlayIdentifier
{
public:
    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv);
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();

    //IShellIconOverlayIdentifier
    STDMETHOD(GetOverlayInfo)(LPWSTR pwszIconFile, int cchMax, int* pIndex, DWORD* pdwFlags);
    STDMETHOD(GetPriority)(int* pPriority);
    STDMETHOD(IsMemberOf)(LPCWSTR pwszPath, DWORD dwAttrib);

    static HRESULT ComponentCreator(const IID& iid, void** ppv);

    OverlayIcon(void);

protected:
    ~OverlayIcon(void);

private:
    long m_cRef;
};
