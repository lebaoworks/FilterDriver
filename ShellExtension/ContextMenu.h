#pragma once

#include <vector>
#include <string>

#include <Windows.h>
#include <ShlObj.h>

class ContextMenu :
    public IShellExtInit,
    public IContextMenu
{
private:
    ULONG _ref;
    std::vector<std::wstring> _selected_files;

public:
    ContextMenu();
    ~ContextMenu();

    // IUnknown interface
    HRESULT STDMETHODCALLTYPE QueryInterface(
        _In_         REFIID riid,
        _COM_Outptr_ void** ppvObject);
    ULONG   STDMETHODCALLTYPE AddRef();
    ULONG   STDMETHODCALLTYPE Release();

    // IShellExtInit interface
    HRESULT STDMETHODCALLTYPE Initialize(
        _In_opt_ PCIDLIST_ABSOLUTE pidlFolder,
        _In_opt_ IDataObject*      pdtobj,
        _In_opt_ HKEY              hkeyProgID);

    // IContextMenu interface
    HRESULT STDMETHODCALLTYPE QueryContextMenu(
        _In_ HMENU hMenu,
        _In_ UINT  indexMenu,
        _In_ UINT  idCmdFirst,
        _In_ UINT  idCmdLast,
        _In_ UINT  uFlags);
    HRESULT STDMETHODCALLTYPE InvokeCommand(
        _In_ CMINVOKECOMMANDINFO* pici);
    HRESULT STDMETHODCALLTYPE GetCommandString(
        _In_       UINT_PTR    idCmd,
        _In_       UINT        uType,
        _Reserved_ UINT*,
        _Out_writes_bytes_((uType& GCS_UNICODE) ? (cchMax * sizeof(wchar_t)) : cchMax) _When_(!(uType& (GCS_VALIDATEA | GCS_VALIDATEW)), _Null_terminated_) CHAR* pszName,
        _In_       UINT        cchMax);
};