#define _CRT_SECURE_NO_WARNINGS

#include "ContextMenu.h"

// Built-in Libraries
#include <atlbase.h>
#include <atlcom.h>
#include <strsafe.h>

// Shared Libraries
#include <Shared.h>
#include <ShellExt.h>

#include "Control.h"

// Definitions
#define IDM_PROTECT 0
#define IDM_UNPROTECT 1


ContextMenu::ContextMenu() : _ref(1) { LogDebug("NewInstance: %p -> [%4d]", this, ShellExt::DllAddRef()); }
ContextMenu::~ContextMenu() { LogDebug("DeleteInstance: %p -> [%4d]", this, ShellExt::DllRelease()); }

// IUnknown
// ========

HRESULT STDMETHODCALLTYPE ContextMenu::QueryInterface(
    _In_         REFIID riid,
    _COM_Outptr_ void** ppvObject)
{
    static const QITAB qit[] = {
      QITABENT(ContextMenu, IShellExtInit),
      QITABENT(ContextMenu, IContextMenu),
      {0, 0},
    };
    auto hr = QISearch(this, qit, riid, ppvObject);
    LogDebug("Search interface %s -> %s", ShellExt::GuidToString(riid).c_str(), SUCCEEDED(hr) ? "Found" : "Unknown");
    return hr;
}

ULONG STDMETHODCALLTYPE ContextMenu::AddRef() { return InterlockedIncrement(&_ref); }

ULONG STDMETHODCALLTYPE ContextMenu::Release()
{
    auto ref = InterlockedDecrement(&_ref);
    if (ref == 0)
        delete this;
    return ref;
}

// IShellExtInit
// =============

HRESULT STDMETHODCALLTYPE ContextMenu::Initialize(
    _In_opt_ PCIDLIST_ABSOLUTE pidlFolder,
    _In_opt_ IDataObject*      pdtobj,
    _In_opt_ HKEY              hkeyProgID)
{
    LogDebug("IdList: %p, DataObj: %p, Key: %p", pidlFolder, pdtobj, hkeyProgID);
    this->_selected_files.clear();
    defer{
        LogDebug("Clicked on %d entries", this->_selected_files.size());
        for (auto& file : this->_selected_files)
            LogDebug("  %ws", file.c_str());
    };

    // Clicked on a folder's background or the desktop directory?
    if (pidlFolder != nullptr)
    {
        auto buffer = std::make_unique<WCHAR[]>(MAXSHORT + 1);
        if (SHGetPathFromIDListW(pidlFolder, buffer.get()) == FALSE)
        {
            LogError("GetPath failed: %d", GetLastError());
            return E_INVALIDARG;
        }
        this->_selected_files.push_back(buffer.get());
        return S_OK;

    }
    // Clicked on one or more file or directory?
    else if (pdtobj != nullptr)
    {
        FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stg = { TYMED_HGLOBAL };

        // The selected files are expected to be in HDROP format.
        if (FAILED(pdtobj->GetData(&fmt, &stg)))
        {
            LogError("selected files are not in HDROP format");
            return E_INVALIDARG;
        }

        // Get a locked pointer to the files data.
        auto hDropInfo = static_cast<HDROP>(GlobalLock(stg.hGlobal));
        if (hDropInfo == NULL)
        {
            ReleaseStgMedium(&stg);
            LogError("failed to get lock on selected files");
            return E_INVALIDARG;
        }
        defer{ GlobalUnlock(stg.hGlobal); ReleaseStgMedium(&stg); };

        auto num_files = DragQueryFileW(hDropInfo, 0xFFFFFFFF, NULL, 0);
        auto buffer = std::make_unique<WCHAR[]>(MAXSHORT + 1);
        for (UINT i = 0; i < num_files; i++)
        {
            if (DragQueryFileW(hDropInfo, i, buffer.get(), MAXSHORT + 1) != 0)
                this->_selected_files.push_back(buffer.get());
        }
        return S_OK;
    }
    return S_OK;
}

// IContextMenu
// ============

HRESULT STDMETHODCALLTYPE ContextMenu::QueryContextMenu(
    _In_ HMENU hMenu,
    _In_ UINT  indexMenu,
    _In_ UINT  idCmdFirst,
    _In_ UINT  idCmdLast,
    _In_ UINT  uFlags)
{
    // Note on flags...
    // Right-click on a file or directory with Windows Explorer on the right area:  flags=0x00020494=132244(dec)=(CMF_NORMAL|CMF_EXPLORE|CMF_CANRENAME|CMF_ITEMMENU|CMF_ASYNCVERBSTATE)
    // Right-click on the empty area      with Windows Explorer on the right area:  flags=0x00020424=132132(dec)=(CMF_NORMAL|CMF_EXPLORE|CMF_NODEFAULT|CMF_ASYNCVERBSTATE)
    // Right-click on a directory         with Windows Explorer on the left area:   flags=0x00000414=001044(dec)=(CMF_NORMAL|CMF_EXPLORE|CMF_CANRENAME|CMF_ASYNCVERBSTATE)
    // Right-click on a drive             with Windows Explorer on the left area:   flags=0x00000414=001044(dec)=(CMF_NORMAL|CMF_EXPLORE|CMF_CANRENAME|CMF_ASYNCVERBSTATE)
    // Right-click on the empty area      on the Desktop:                           flags=0x00020420=132128(dec)=(CMF_NORMAL|CMF_NODEFAULT|CMF_ASYNCVERBSTATE)
    // Right-click on a directory         on the Desktop:                           flags=0x00020490=132240(dec)=(CMF_NORMAL|CMF_CANRENAME|CMF_ITEMMENU|CMF_ASYNCVERBSTATE)
    LogDebug("Menu: %08X, Flags: %08X", hMenu, uFlags);

    if (!(CMF_DEFAULTONLY & uFlags))
    {
        InsertMenuW(hMenu, indexMenu, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_PROTECT, L"&Protect");
        InsertMenuW(hMenu, indexMenu, MF_STRING | MF_BYPOSITION, idCmdFirst + IDM_UNPROTECT, L"&Unprotect");

        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(IDM_UNPROTECT + 1));
    }

    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));
}

HRESULT STDMETHODCALLTYPE ContextMenu::InvokeCommand(
    _In_ CMINVOKECOMMANDINFO* pici)
{
    bool use_unicode = false;

    // Determine which structure is being passed in
    if (pici->cbSize == sizeof(CMINVOKECOMMANDINFOEX))
        if (pici->fMask & CMIC_MASK_UNICODE)
            use_unicode = true;

    // Determines whether the command is identified by its offset or verb.
    // There are two ways to identify commands:
    // 
    //   1) The command's verb string 
    //   2) The command's identifier offset
    // 
    // If the high-order word of pici->lpVerb (for the ANSI case) or 
    // pici->lpVerbW (for the Unicode case) is nonzero, lpVerb or lpVerbW 
    // holds a verb string. If the high-order word is zero, the command 
    // offset is in the low-order word of pici->lpVerb.

    // For the ANSI case, if the high-order word is not zero, the command's 
    // verb string is in pici->lpVerb. 
    if (!use_unicode && HIWORD(pici->lpVerb))
        // Do not support verb
        return E_FAIL;

    // For the Unicode case, if the high-order word is not zero, the 
    // command's verb string is in lpcmi->lpVerbW. 
    if (use_unicode && HIWORD(((CMINVOKECOMMANDINFOEX*)pici)->lpVerbW))
        // Do not support verb
        return E_FAIL;

    // If the command cannot be identified through the verb string, then 
    // check the identifier offset.
    if (LOWORD(pici->lpVerb) == IDM_PROTECT)
    {
        if (Control::Protect(this->_selected_files) == false)
            MessageBox(NULL, L"Failed to protect file(s)", L"Error", MB_OK | MB_ICONERROR);
    }
    else if (LOWORD(pici->lpVerb) == IDM_UNPROTECT)
    {
        if (Control::Unprotect(this->_selected_files) == false)
            MessageBox(NULL, L"Failed to unprotect file(s)", L"Error", MB_OK | MB_ICONERROR);
    }
    else
        return E_FAIL;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE ContextMenu::GetCommandString(
    _In_       UINT_PTR    idCmd,
    _In_       UINT        uType,
    _Reserved_ UINT*,
    _Out_writes_bytes_((uType& GCS_UNICODE) ? (cchMax * sizeof(wchar_t)) : cchMax) _When_(!(uType& (GCS_VALIDATEA | GCS_VALIDATEW)), _Null_terminated_) CHAR* pszName,
    _In_       UINT        cchMax)
{
    HRESULT hr = E_INVALIDARG;

    if (idCmd == IDM_PROTECT)
    {
        switch (uType)
        {
        case GCS_HELPTEXTW:
            hr = StringCchCopyW(reinterpret_cast<PWSTR>(pszName), cchMax, L"Protect file(s)");
            break;
        case GCS_VERBW:
            // GCS_VERBW is an optional feature that enables a caller
            // to discover the canonical name for the verb that is passed in
            // through idCommand.
            hr = StringCchCopyW(reinterpret_cast<PWSTR>(pszName), cchMax, L"Protect");
            break;
        }
    }
    return hr;
}


// IContextMenu
// ============

//HRESULT STDMETHODCALLTYPE ContextMenu::GetOverlayInfo(
//    _Out_ LPWSTR pwszIconFile,
//    _In_ int cchMax,
//    _Out_ int* pIndex,
//    _Out_ DWORD* pdwFlags)
//{
//    LogDebug("GetOverlayInfo");
//    StringCchCopyNW(pwszIconFile, cchMax, LR"(C:\Users\test\Desktop\Artifact\OverlayIcon.ico)", 14);
//    *pIndex = 0;
//    *pdwFlags = ISIOI_ICONFILE | ISIOI_ICONINDEX;
//    return S_OK;
//}
HRESULT STDMETHODCALLTYPE ContextMenu::GetPriority(
    _Out_ int* pPriority)
{
    if (!pPriority)
        return E_POINTER;
    *pPriority = 0;
    return S_OK;
}
HRESULT STDMETHODCALLTYPE ContextMenu::IsMemberOf(
    _In_ LPCWSTR pwszPath,
    _In_ DWORD dwAttrib)
{
    LogDebug("IsMemberOf: %ws", pwszPath);

    //wchar_t* s = _wcsdup(pwszPath);
    HRESULT r = S_OK;

    //_wcslwr(s);
    //// Criteria
    //if (wcsstr(s, L"protect_me") != 0)
    //    r = S_OK;
    //free(s);

    return r;
}