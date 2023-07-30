#pragma once

#include <Windows.h>
#include <ShlObj.h>

class ContextMenu :
    public IShellExtInit,
    public IContextMenu
{
private:
    ULONG _refCount;

public:
    ContextMenu();
    ~ContextMenu();

    //IUnknown interface
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, LPVOID FAR*);
    ULONG   STDMETHODCALLTYPE AddRef();
    ULONG   STDMETHODCALLTYPE Release();

    //IContextMenu interface
    HRESULT STDMETHODCALLTYPE QueryContextMenu(HMENU hMenu, UINT menu_index, UINT first_command_id, UINT max_command_id, UINT flags);
    HRESULT STDMETHODCALLTYPE InvokeCommand(LPCMINVOKECOMMANDINFO lpcmi);
    HRESULT STDMETHODCALLTYPE GetCommandString(UINT_PTR command_id, UINT flags, UINT FAR* reserved, LPSTR pszName, UINT cchMax);

    //IShellExtInit interface
    HRESULT STDMETHODCALLTYPE Initialize(LPCITEMIDLIST pIDFolder, LPDATAOBJECT pDataObj, HKEY hKeyID);
};

