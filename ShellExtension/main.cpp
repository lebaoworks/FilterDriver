//#pragma comment(linker, "/export:DllMain")
//#pragma comment(linker, "/export:DllGetClassObject,PRIVATE")
//#pragma comment(linker, "/export:DllCanUnloadNow,PRIVATE")
//#pragma comment(linker, "/export:DllRegisterServer,PRIVATE")
//#pragma comment(linker, "/export:DllUnregisterServer,PRIVATE")

#include "main.h"

// Builtin Libraries
#include <Windows.h>
#include <Fltuser.h>

// Shared Libraries
#include <Shared.h>
#include <ShellExt.h>
#include <Win32.h>
#include <Communication.h>

#include <thread>


HRESULT SetupRegistry()
{
    auto module_path_str = Win32::Module::GetCurrentModulePath();
    auto module_path = module_path_str.c_str();
    Log("module path -> %s", module_path);
    auto guid_str = ShellExt::GuidToString(CLSID_ShellMenu);
    auto guid = guid_str.c_str();

    // Add the CLSID of this DLL to the registry
    auto sid_key = shared::format("HKEY_CLASSES_ROOT\\CLSID\\%s", guid);
    if (Win32::Registry::CreateKey(sid_key, SHELL_EXTENSION_DESCRIPTION) != ERROR_SUCCESS)
        return E_ACCESSDENIED;

    // Define the path and parameters of our DLL:
    {
        auto key = shared::format("HKEY_CLASSES_ROOT\\CLSID\\%s\\ProgID", guid);
        if (Win32::Registry::CreateKey(key, std::string(SHELL_EXTENSION_CLASS_NAME) + ".1") != ERROR_SUCCESS)
            return E_ACCESSDENIED;
    }
    {
        auto key = shared::format("HKEY_CLASSES_ROOT\\CLSID\\%s\\VersionIndependentProgID", guid);
        if (Win32::Registry::CreateKey(key, SHELL_EXTENSION_CLASS_NAME) != ERROR_SUCCESS)
            return E_ACCESSDENIED;
    }
    {
        auto key = shared::format("HKEY_CLASSES_ROOT\\CLSID\\%s\\Programmable", guid);
        if (Win32::Registry::CreateKey(key) != ERROR_SUCCESS)
            return E_ACCESSDENIED;
    }
    {
        auto key = shared::format("HKEY_CLASSES_ROOT\\CLSID\\%s\\InprocServer32", guid);
        if (Win32::Registry::CreateKey(key, module_path) != ERROR_SUCCESS)
            return E_ACCESSDENIED;
        if (Win32::Registry::SetValueString(key, "ThreadingModel", "Apartment") != ERROR_SUCCESS)
            return E_ACCESSDENIED;
    }

    // Register the shell extension for all the file types
    {
        auto key = shared::format("HKEY_CLASSES_ROOT\\*\\shellex\\ContextMenuHandlers\\%s", SHELL_EXTENSION_CLASS_NAME);
        if (Win32::Registry::CreateKey(key, guid) != ERROR_SUCCESS)
            return E_ACCESSDENIED;
        printf("Registered %s -> %s\n", key.c_str(), guid);
    }
    // Register the shell extension for directories
    {
        auto key = shared::format("HKEY_CLASSES_ROOT\\Directory\\shellex\\ContextMenuHandlers\\%s", SHELL_EXTENSION_CLASS_NAME);
        if (Win32::Registry::CreateKey(key, guid) != ERROR_SUCCESS)
            return E_ACCESSDENIED;
    }
    // Register the shell extension for folders
    {
        auto key = shared::format("HKEY_CLASSES_ROOT\\Folder\\shellex\\ContextMenuHandlers\\%s", SHELL_EXTENSION_CLASS_NAME);
        if (Win32::Registry::CreateKey(key, guid) != ERROR_SUCCESS)
            return E_ACCESSDENIED;
    }
    // Register the shell extension for the desktop or the file explorer's background
    {
        auto key = shared::format("HKEY_CLASSES_ROOT\\Directory\\Background\\shellex\\ContextMenuHandlers\\%s", SHELL_EXTENSION_CLASS_NAME);
        if (Win32::Registry::CreateKey(key, guid) != ERROR_SUCCESS)
            return E_ACCESSDENIED;
    }
    // Register the shell extension for drives
    {
        auto key = shared::format("HKEY_CLASSES_ROOT\\Drive\\shellex\\ContextMenuHandlers\\%s", SHELL_EXTENSION_CLASS_NAME);
        if (Win32::Registry::CreateKey(key, guid) != ERROR_SUCCESS)
            return E_ACCESSDENIED;
    }
    // Register the shell extension to the system's approved Shell Extensions
    {
        auto key = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";
        if (Win32::Registry::CreateKey(key) != ERROR_SUCCESS)
            return E_ACCESSDENIED;
        if (Win32::Registry::SetValueString(key, guid, SHELL_EXTENSION_DESCRIPTION))
            return E_ACCESSDENIED;
    }

    //Register version 1 of our class
    {
        auto key = shared::format("HKEY_CLASSES_ROOT\\%s.1", SHELL_EXTENSION_CLASS_NAME);
        if (Win32::Registry::CreateKey(key, SHELL_EXTENSION_DESCRIPTION) != ERROR_SUCCESS)
            return E_ACCESSDENIED;
    }
    {
        auto key = shared::format("HKEY_CLASSES_ROOT\\%s.1\\CLSID", SHELL_EXTENSION_CLASS_NAME);
        if (Win32::Registry::CreateKey(key, guid) != ERROR_SUCCESS)
            return E_ACCESSDENIED;
    }

    //Register current version of our class
    {
        auto key = shared::format("HKEY_CLASSES_ROOT\\%s", SHELL_EXTENSION_CLASS_NAME);
        if (Win32::Registry::CreateKey(key, SHELL_EXTENSION_DESCRIPTION) != ERROR_SUCCESS)
            return E_ACCESSDENIED;
    }
    {
        auto key = shared::format("HKEY_CLASSES_ROOT\\%s\\CLSID", SHELL_EXTENSION_CLASS_NAME);
        if (Win32::Registry::CreateKey(key, guid) != ERROR_SUCCESS)
            return E_ACCESSDENIED;
    }
    {
        auto key = shared::format("HKEY_CLASSES_ROOT\\%s\\CurVer", SHELL_EXTENSION_CLASS_NAME);
        if (Win32::Registry::CreateKey(key, std::string(SHELL_EXTENSION_CLASS_NAME)+".1") != ERROR_SUCCESS)
            return E_ACCESSDENIED;
    }
    return S_OK;
}

#include <atlbase.h>
#include <atlcom.h>

#include "ClassFactory.h"

//CComModule _Module;

HRESULT WINAPI DllMain(
    _In_ HMODULE hModule,
    _In_ DWORD dwReason,
    _In_ LPVOID lpReserved)
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

_Check_return_
STDAPI DllGetClassObject(
    _In_ REFCLSID rclsid,
    _In_ REFIID riid,
    _Outptr_ LPVOID FAR* ppv)
{
    Log("riid = %s", ShellExt::GuidToString(riid).c_str());
    //if (ppv == NULL)
    //    return E_INVALIDARG;

    //if (!IsEqualGUID(rclsid, CLSID_ShellMenu))
    //    return CLASS_E_CLASSNOTAVAILABLE;

    //auto class_factory = new ClassFactory();
    //auto hr = class_factory->QueryInterface(riid, ppv);
    //class_factory->Release();

    //if (SUCCEEDED(hr))
    //    Log("Found interface %s", ShellExt::GuidToString(riid).c_str());
    //else
    //    Log("Unkonwn interface %s", ShellExt::GuidToString(riid).c_str());
    //return hr;

    return CLASS_E_CLASSNOTAVAILABLE;
}

__control_entrypoint(DllExport)
STDAPI DllCanUnloadNow(void)
{
    Log();

    ULONG ulRefCount = ShellExt::DllRelease();
    ulRefCount = ShellExt::DllRelease();

    if (ulRefCount == 0)
    {
        Log("-> Yes");
        return S_OK;
    }
    Log("-> No. Remaining: %d", ulRefCount);
    return S_FALSE;
}

STDAPI DllRegisterServer(void)
{
    ///*const std::string guid_str_tmp = GuidToString(CLSID_ShellAnythingMenu).c_str();
    //const char* guid_str = guid_str_tmp.c_str();
    //const std::string class_name_version1 = std::string(ShellExtensionClassName) + ".1";
    //const std::string module_path = GetCurrentModulePath();*/

    ////#define TRACELINE() MessageBox(NULL, (std::string("Line: ") + ra::strings::ToString(__LINE__)).c_str(), "DllUnregisterServer() DEBUG", MB_OK);

    ////Register version 1 of our class
    //{
    //    std::string key = shared::format("HKEY_CLASSES_ROOT\\%s", SHELL_EXTENSION_CLASS_NAME);
    //    if (Win32::Registry::CreateKey(key) != ERROR_SUCCESS)
    //        return E_ACCESSDENIED;
    //    if (Win32::Registry::SetValueString(key, NULL, SHELL_EXTENSION_DESCRIPTION) != ERROR_SUCCESS)
    //        return E_ACCESSDENIED;
    //}
    //{
    //    std::string key = ra::strings::Format("HKEY_CLASSES_ROOT\\%s.1\\CLSID", ShellExtensionClassName);
    //    if (!Win32Registry::CreateKey(key.c_str(), guid_str))
    //        return E_ACCESSDENIED;
    //}

    ////Register current version of our class
    //{
    //    std::string key = ra::strings::Format("HKEY_CLASSES_ROOT\\%s", ShellExtensionClassName);
    //    if (!Win32Registry::CreateKey(key.c_str(), ShellExtensionDescription))
    //        return E_ACCESSDENIED;
    //}
    //{
    //    std::string key = ra::strings::Format("HKEY_CLASSES_ROOT\\%s\\CLSID", ShellExtensionClassName);
    //    if (!Win32Registry::CreateKey(key.c_str(), guid_str))
    //        return E_ACCESSDENIED;
    //}
    //{
    //    std::string key = ra::strings::Format("HKEY_CLASSES_ROOT\\%s\\CurVer", ShellExtensionClassName);
    //    if (!Win32Registry::CreateKey(key.c_str(), class_name_version1.c_str()))
    //        return E_ACCESSDENIED;
    //}


    // Notify the Shell to pick the changes:
    // https://docs.microsoft.com/en-us/windows/desktop/shell/reg-shell-exts#predefined-shell-objects
    // Any time you create or change a Shell extension handler, it is important to notify the system that you have made a change.
    // Do so by calling SHChangeNotify, specifying the SHCNE_ASSOCCHANGED event.
    // If you do not call SHChangeNotify, the change might not be recognized until the system is rebooted.
    //SHChangeNotify(SHCNE_ASSOCCHANGED, 0, 0, 0);

    /*auto ret = SetupRegistry();
    if (ret != S_OK)
        return ret;
    SHChangeNotify(SHCNE_ASSOCCHANGED, 0, 0, 0);*/
    return S_OK;
}

STDAPI DllUnregisterServer(void)
{
    //const std::string guid_str_tmp = GuidToString(CLSID_ShellAnythingMenu).c_str();
    //const char* guid_str = guid_str_tmp.c_str();
    //const std::string class_name_version1 = std::string(ShellExtensionClassName) + ".1";
    //const std::string guid_icontextmenu_tmp = GuidToString(IID_IContextMenu);
    //const std::string guid_icontextmenu = guid_icontextmenu_tmp.c_str();

    ////#define TRACELINE() MessageBox(NULL, (std::string("Line: ") + ra::strings::ToString(__LINE__)).c_str(), "DllUnregisterServer() DEBUG", MB_OK);

    //// Removed the shell extension from the user's cached Shell Extensions
    //{
    //    std::string key = "HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Cached";
    //    std::string value = ra::strings::Format("%s {000214E4-0000-0000-C000-000000000046} 0xFFFF", guid_str); // {B0D35103-86A1-471C-A653-E130E3439A3B} {000214E4-0000-0000-C000-000000000046} 0xFFFF
    //    if (!Win32Registry::DeleteValue(key.c_str(), value.c_str()))
    //        return E_ACCESSDENIED;
    //}

    //// Unregister the shell extension from the system's approved Shell Extensions
    //{
    //    std::string key = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";
    //    if (!Win32Registry::DeleteValue(key.c_str(), guid_str))
    //        return E_ACCESSDENIED;
    //}

    //// Unregister the shell extension for drives
    //{
    //    std::string key = ra::strings::Format("HKEY_CLASSES_ROOT\\Drive\\shellex\\ContextMenuHandlers\\%s", ShellExtensionClassName);
    //    if (!Win32Registry::DeleteKey(key.c_str()))
    //        return E_ACCESSDENIED;
    //}

    //// Unregister the shell extension for the desktop or the file explorer's background
    //{
    //    std::string key = ra::strings::Format("HKEY_CLASSES_ROOT\\Directory\\Background\\shellex\\ContextMenuHandlers\\%s", ShellExtensionClassName);
    //    if (!Win32Registry::DeleteKey(key.c_str()))
    //        return E_ACCESSDENIED;
    //}

    //// Unregister the shell extension for folders
    //{
    //    std::string key = ra::strings::Format("HKEY_CLASSES_ROOT\\Folders\\shellex\\ContextMenuHandlers\\%s", ShellExtensionClassName);
    //    if (!Win32Registry::DeleteKey(key.c_str()))
    //        return E_ACCESSDENIED;
    //}

    //// Unregister the shell extension for directories
    //{
    //    std::string key = ra::strings::Format("HKEY_CLASSES_ROOT\\Directory\\shellex\\ContextMenuHandlers\\%s", ShellExtensionClassName);
    //    if (!Win32Registry::DeleteKey(key.c_str()))
    //        return E_ACCESSDENIED;
    //}

    //// Unregister the shell extension for all the file types
    //{
    //    std::string key = ra::strings::Format("HKEY_CLASSES_ROOT\\*\\shellex\\ContextMenuHandlers\\%s", ShellExtensionClassName);
    //    if (!Win32Registry::DeleteKey(key.c_str()))
    //        return E_ACCESSDENIED;
    //}

    //// Remove the CLSID of this DLL from the registry
    //{
    //    std::string key = ra::strings::Format("HKEY_CLASSES_ROOT\\CLSID\\%s", guid_str);
    //    if (!Win32Registry::DeleteKey(key.c_str()))
    //        return E_ACCESSDENIED;
    //}

    //// Unregister current and version 1 of our extension
    //{
    //    std::string key = ra::strings::Format("HKEY_CLASSES_ROOT\\%s", class_name_version1.c_str());
    //    if (!Win32Registry::DeleteKey(key.c_str()))
    //        return E_ACCESSDENIED;
    //}
    //{
    //    std::string key = ra::strings::Format("HKEY_CLASSES_ROOT\\%s", ShellExtensionClassName);
    //    if (!Win32Registry::DeleteKey(key.c_str()))
    //        return E_ACCESSDENIED;
    //}

    //// Notify the Shell to pick the changes:
    //// https://docs.microsoft.com/en-us/windows/desktop/shell/reg-shell-exts#predefined-shell-objects
    //// Any time you create or change a Shell extension handler, it is important to notify the system that you have made a change.
    //// Do so by calling SHChangeNotify, specifying the SHCNE_ASSOCCHANGED event.
    //// If you do not call SHChangeNotify, the change might not be recognized until the system is rebooted.
    //SHChangeNotify(SHCNE_ASSOCCHANGED, 0, 0, 0);

    return S_OK;
}


#ifdef _DEBUG
#include <iostream>
#include <stdexcept>

class FilterConnection
{
private:
    HANDLE _port = NULL;
public:
    FilterConnection(const std::wstring& comport_name, const std::wstring& comport_password)
    {
        Communication::Credential credential{"asdasd"};
        printf("Password: %s", credential.Password);
        auto ret = FilterConnectCommunicationPort(
            comport_name.c_str(),
            FLT_PORT_FLAG_SYNC_HANDLE,
            &credential,
            sizeof(credential),
            NULL,
            &_port);
        if (ret != S_OK)
            throw std::runtime_error(shared::format("FilterConnectCommunicationPort failed -> HRESULT: %X", ret));
    }
    ~FilterConnection()
    {
        printf("Closing...\n");
        CloseHandle(_port);
        printf("Closed Port!\n");
    }

    template<typename T>
    void Send(const Communication::ClientMessage<T>& message)
    {
        DWORD bytes_written = 0;
        std::thread([&] {
            printf("Sending...\n");
            auto ret = FilterSendMessage(_port, (PVOID)&message, message.Header.Size, NULL, 0, &bytes_written);
            if (ret != S_OK)
                throw std::runtime_error(shared::format("FilterSendMessage failed -> HRESULT: %X", ret));
            else
                printf("Success!\n");
        }).detach();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        printf("Exit!\n");
        return;
    }
};

void test_control()
{
    try {
        auto connection = FilterConnection(COMPORT_NAME, L"BAO");
        Communication::ClientMessage<Communication::MessageProtect> message;
        wcscpy_s(message.Body.DosPath, L"C:\\Users\\BAO\\Desktop\\test.txt");
        connection.Send(message);
    } catch (std::exception& e)
    {
        printf("Exception: %s", e.what());
    }
}
int main()
{
    /*SetupRegistry();
    SHChangeNotify(SHCNE_ASSOCCHANGED, 0, 0, 0);
    Log("Done");*/
    test_control();
    return 0;
}
#endif