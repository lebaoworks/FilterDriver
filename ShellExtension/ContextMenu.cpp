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

#ifdef IMPLEMENT
    //From this point, it is safe to use class members without other threads interference
    CCriticalSectionGuard cs_guard(&m_CS);

    

    //Filter out queries that have nothing selected.
    //This can happend if user is copy & pasting files (using CTRL+C and CTRL+V)
    //and if the shell extension is registered as a DragDropHandlers.
    if (m_Context.GetElements().size() == 0)
    {
        //Don't know what to do with this
        LOG(INFO) << __FUNCTION__ << "(), skipped, nothing is selected.";
        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0); //nothing inserted
    }

    //Filter out queries that are called twice for the same directory.
    if (hMenu == m_previousMenu)
    {
        //Issue  #6 - Right-click on a directory with Windows Explorer in the left panel shows the menus twice.
        //Issue #31 - Error in logs for CContextMenu::GetCommandString().
        //Using a static variable is a poor method for solving the issue but it is a "good enough" strategy.
        LOG(INFO) << __FUNCTION__ << "(), skipped, QueryContextMenu() called twice and menu is already populated once.";
        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0); //nothing inserted
    }

    //Remember current menu to prevent issues calling twice QueryContextMenu()
    m_previousMenu = hMenu;

    //Log what is selected by the user
    const shellanything::StringList& elements = m_Context.GetElements();
    size_t num_selected_total = elements.size();
    int num_files = m_Context.GetNumFiles();
    int num_directories = m_Context.GetNumDirectories();
    LOG(INFO) << __FUNCTION__ << "(), SelectionContext have " << num_selected_total << " element(s): " << num_files << " files and " << num_directories << " directories.";

    //Keep a reference the our first command id. We will need it when InvokeCommand is called.
    m_FirstCommandId = first_command_id;

    //Refresh the list of loaded configuration files
    shellanything::ConfigManager& cmgr = shellanything::ConfigManager::GetInstance();
    cmgr.Refresh();

    //Update all menus with the new context
    //This will refresh the visibility flags which is required before calling ConfigManager::AssignCommandIds()
    cmgr.Update(m_Context);

    //Assign unique command id to visible menus. Issue #5
    UINT next_command_id = cmgr.AssignCommandIds(first_command_id);

    //Build the menus
    BuildMenuTree(hMenu);

    //Log information about menu statistics.
    UINT menu_last_command_id = (UINT)-1; //confirmed last command id
    if (next_command_id != first_command_id)
        menu_last_command_id = next_command_id - 1;
    UINT num_menu_items = next_command_id - first_command_id;
    LOG(INFO) << __FUNCTION__ << "(), Menu: first_command_id=" << first_command_id << " menu_last_command_id=" << menu_last_command_id << " next_command_id=" << next_command_id << " num_menu_items=" << num_menu_items << ".\n";

    //debug the constructed menu tree
#ifdef _DEBUG
    std::string menu_tree = Win32Utils::GetMenuTree(hMenu, 2);
    LOG(INFO) << __FUNCTION__ << "(), Menu tree:\n" << menu_tree.c_str();
#endif

    HRESULT hr = MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, num_menu_items);
    return hr;
#endif
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

#ifdef IMPLEMENT
    //define the type of structure pointed by pici
    const char* struct_name = "UNKNOWN";
    if (pici->cbSize == sizeof(CMINVOKECOMMANDINFO))
        struct_name = "CMINVOKECOMMANDINFO";
    else if (pici->cbSize == sizeof(CMINVOKECOMMANDINFOEX))
        struct_name = "CMINVOKECOMMANDINFOEX";

    //define how we should interpret pici->lpVerb
    std::string verb;
    if (IS_INTRESOURCE(pici->lpVerb))
    {
        // D:\Projects\ShellAnything\src\shellext.cpp(632) : warning C4311 : 'reinterpret_cast' : pointer truncation from 'LPCSTR' to 'int'
        // D:\Projects\ShellAnything\src\shellext.cpp(632) : warning C4302 : 'reinterpret_cast' : truncation from 'LPCSTR' to 'int'
#pragma warning( push )
#pragma warning( disable: 4302 )
#pragma warning( disable: 4311 )
        verb = ra::strings::ToString(reinterpret_cast<int>(pici->lpVerb));
#pragma warning( pop )
    }
    else
        verb = pici->lpVerb;

    LOG(INFO) << __FUNCTION__ << "(), pici->cbSize=" << struct_name << ", pici->fMask=" << pici->fMask << ", pici->lpVerb=" << verb << " this=" << ToHexString(this);

    //validate
    if (!IS_INTRESOURCE(pici->lpVerb))
        return E_INVALIDARG; //don't know what to do with pici->lpVerb

    UINT target_command_offset = LOWORD(pici->lpVerb); //matches the command_id offset (command id of the selected menu - command id of the first menu)
    UINT target_command_id = m_FirstCommandId + target_command_offset;

    //From this point, it is safe to use class members without other threads interference
    CCriticalSectionGuard cs_guard(&m_CS);

    //find the menu that is requested
    shellanything::ConfigManager& cmgr = shellanything::ConfigManager::GetInstance();
    shellanything::Menu* menu = cmgr.FindMenuByCommandId(target_command_id);
    if (menu == NULL)
    {
        LOG(ERROR) << __FUNCTION__ << "(), unknown menu for pici->lpVerb=" << verb;
        return E_INVALIDARG;
    }

    //compute the visual menu title
    shellanything::PropertyManager& pmgr = shellanything::PropertyManager::GetInstance();
    std::string title = pmgr.Expand(menu->GetName());

    //found a menu match, execute menu action
    LOG(INFO) << __FUNCTION__ << "(), executing action(s) for menu '" << title.c_str() << "'...";

    //execute actions
    const shellanything::IAction::ActionPtrList& actions = menu->GetActions();
    for (size_t i = 0; i < actions.size(); i++)
    {
        LOG(INFO) << __FUNCTION__ << "(), executing action " << (i + 1) << " of " << actions.size() << ".";
        const shellanything::IAction* action = actions[i];
        if (action)
        {
            ra::errors::ResetLastErrorCode(); //reset win32 error code in case the action fails.
            bool success = action->Execute(m_Context);

            if (!success)
            {
                //try to get an error message from win32
                ra::errors::errorcode_t dwError = ra::errors::GetLastErrorCode();
                if (dwError)
                {
                    std::string error_message = ra::errors::GetErrorCodeDescription(dwError);
                    LOG(ERROR) << __FUNCTION__ << "(), action #" << (i + 1) << " has failed: " << error_message;
                }
                else
                {
                    //simply log an error
                    LOG(ERROR) << __FUNCTION__ << "(), action #" << (i + 1) << " has failed.";
                }

                //stop executing the next actions
                i = actions.size();
            }
        }
    }

#endif
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