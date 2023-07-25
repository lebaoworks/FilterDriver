#pragma once

#include <Windows.h>
#include <ShlObj.h>

class ContextMenu :
    public IContextMenu,
    public IShellExtInit,
    public IExplorerCommand,
    public IEnumExplorerCommand
{
public:
    // IShellExtInit
    STDMETHOD(Initialize)(LPCITEMIDLIST pidlFolder, LPDATAOBJECT dataObject, HKEY hkeyProgID);

    // IContextMenu
    STDMETHOD(QueryContextMenu)(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags);
    STDMETHOD(InvokeCommand)(LPCMINVOKECOMMANDINFO lpici);
    STDMETHOD(GetCommandString)(UINT_PTR idCmd, UINT uType, UINT* pwReserved, LPSTR pszName, UINT cchMax);

    HRESULT InitContextMenu(const wchar_t* folder, const wchar_t* const* names, unsigned numFiles);

    void LoadItems(IShellItemArray* psiItemArray);

    // IExplorerCommand
    STDMETHOD(GetTitle)   (IShellItemArray* psiItemArray, LPWSTR* ppszName);
    STDMETHOD(GetIcon)    (IShellItemArray* psiItemArray, LPWSTR* ppszIcon);
    STDMETHOD(GetToolTip) (IShellItemArray* psiItemArray, LPWSTR* ppszInfotip);
    STDMETHOD(GetCanonicalName) (GUID* pguidCommandName);
    STDMETHOD(GetState)   (IShellItemArray* psiItemArray, BOOL fOkToBeSlow, EXPCMDSTATE* pCmdState);
    STDMETHOD(Invoke)     (IShellItemArray* psiItemArray, IBindCtx* pbc);
    STDMETHOD(GetFlags)   (EXPCMDFLAGS* pFlags);
    STDMETHOD(EnumSubCommands) (IEnumExplorerCommand** ppEnum);

    // IEnumExplorerCommand
    STDMETHOD(Next) (ULONG celt, IExplorerCommand** pUICommand, ULONG* pceltFetched);
    STDMETHOD(Skip) (ULONG celt);
    STDMETHOD(Reset) (void);
    STDMETHOD(Clone) (IEnumExplorerCommand** ppenum);
};

