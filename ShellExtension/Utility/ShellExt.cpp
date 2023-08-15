#include "ShellExt.h"

// Builtin Libraries
#include <windows.h>
#include <ShlObj.h>

// Shared Libraries
#include <Shared.h>
#include <Win32.h>


static LONG _DllRefCount = 0;
LONG ShellExt::DllAddRef(void) { return InterlockedIncrement(&_DllRefCount); }
LONG ShellExt::DllRelease(void) { return InterlockedDecrement(&_DllRefCount); }
BOOL ShellExt::DllReferenceCount(void) { return _DllRefCount; }


std::string ShellExt::GuidToString(const GUID& guid) { return shared::format("{%08X-%04hX-%04hX-%02X%02X-%02X%02X%02X%02X%02X%02X}", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]); }

namespace ShellExt::Setup
{
    DWORD Install(const std::string& module_path, const std::string& guid, const std::string& class_name)
    {
        // Add the CLSID of this DLL to the registry
        {
            auto key = shared::format("HKEY_CLASSES_ROOT\\CLSID\\%s", guid.c_str());
            auto err = Win32::Registry::CreateKey(key);
            Log("Create %s -> %d", key.c_str(), err);
            if (err != ERROR_SUCCESS)
                return err;
        }
        // Define the path and parameters of our DLL:
        {
            auto key = shared::format("HKEY_CLASSES_ROOT\\CLSID\\%s\\ProgID", guid.c_str());
            auto err = Win32::Registry::CreateKey(key, class_name + ".1");
            Log("Create %s -> %d", key.c_str(), err);
            if (err != ERROR_SUCCESS)
                return err;
        }
        {
            auto key = shared::format("HKEY_CLASSES_ROOT\\CLSID\\%s\\VersionIndependentProgID", guid.c_str());
            auto err = Win32::Registry::CreateKey(key, class_name);
            Log("Create %s -> %d", key.c_str(), err);
            if (err != ERROR_SUCCESS)
                return err;
        }
        {
            auto key = shared::format("HKEY_CLASSES_ROOT\\CLSID\\%s\\Programmable", guid.c_str());
            auto err = Win32::Registry::CreateKey(key);
            Log("Create %s -> %d", key.c_str(), err);
            if (err != ERROR_SUCCESS)
                return err;
        }
        {
            auto key = shared::format("HKEY_CLASSES_ROOT\\CLSID\\%s\\InprocServer32", guid.c_str());
            auto err = Win32::Registry::CreateKey(key, module_path);
            Log("Create %s -> %d", key.c_str(), err);
            if (err != ERROR_SUCCESS)
                return err;
            err = Win32::Registry::SetValueString(key, "ThreadingModel", "Apartment");
            Log("Create %s, ThreadingModel -> %d", key.c_str(), err);
            if (err != ERROR_SUCCESS)
                return err;
        }

        ////Register current version of our class
        //{
        //    auto key = shared::format("HKEY_CLASSES_ROOT\\%s", SHELL_EXTENSION_CLASS_NAME);
        //    if (Win32::Registry::CreateKey(key, SHELL_EXTENSION_DESCRIPTION) != ERROR_SUCCESS)
        //        return E_ACCESSDENIED;
        //}
        //{
        //    auto key = shared::format("HKEY_CLASSES_ROOT\\%s\\CLSID", SHELL_EXTENSION_CLASS_NAME);
        //    if (Win32::Registry::CreateKey(key, guid) != ERROR_SUCCESS)
        //        return E_ACCESSDENIED;
        //}
        //{
        //    auto key = shared::format("HKEY_CLASSES_ROOT\\%s\\CurVer", SHELL_EXTENSION_CLASS_NAME);
        //    if (Win32::Registry::CreateKey(key, std::string(SHELL_EXTENSION_CLASS_NAME) + ".1") != ERROR_SUCCESS)
        //        return E_ACCESSDENIED;
        //}

        //Register version 1 of our class
        {
            auto key = shared::format("HKEY_CLASSES_ROOT\\%s.1", class_name.c_str());
            auto err = Win32::Registry::CreateKey(key);
            Log("Create %s -> %d", key.c_str(), err);
            if (err != ERROR_SUCCESS)
                return err;
        }
        {
            auto key = shared::format("HKEY_CLASSES_ROOT\\%s.1\\shellex", class_name.c_str());
            auto err = Win32::Registry::CreateKey(key);
            Log("Create %s -> %d", key.c_str(), err);
            if (err != ERROR_SUCCESS)
                return err;
        }
        {
            auto key = shared::format("HKEY_CLASSES_ROOT\\%s.1\\shellex\\ContextMenuHandlers", class_name.c_str());
            auto err = Win32::Registry::CreateKey(key, guid);
            Log("Create %s -> %d", key.c_str(), err);
            if (err != ERROR_SUCCESS)
                return err;
        }
        {
            auto key = shared::format("HKEY_CLASSES_ROOT\\%s.1\\CLSID", class_name.c_str());
            auto err = Win32::Registry::CreateKey(key, guid);
            Log("Create %s -> %d", key.c_str(), err);
            if (err != ERROR_SUCCESS)
                return err;
        }

        // Register the shell extension for all the file types
        {
            auto key = shared::format("HKEY_CLASSES_ROOT\\*\\shellex\\ContextMenuHandlers\\%s", class_name.c_str());
            if (Win32::Registry::CreateKey(key, guid) != ERROR_SUCCESS)
                return E_ACCESSDENIED;
            printf("Registered %s -> %s\n", key.c_str(), guid.c_str());
        }
        //// Register the shell extension for directories
        //{
        //    auto key = shared::format("HKEY_CLASSES_ROOT\\Directory\\shellex\\ContextMenuHandlers\\%s", class_name.c_str());
        //    if (Win32::Registry::CreateKey(key, guid) != ERROR_SUCCESS)
        //        return E_ACCESSDENIED;
        //}
        //// Register the shell extension for folders
        //{
        //    auto key = shared::format("HKEY_CLASSES_ROOT\\Folder\\shellex\\ContextMenuHandlers\\%s", class_name.c_str());
        //    if (Win32::Registry::CreateKey(key, guid) != ERROR_SUCCESS)
        //        return E_ACCESSDENIED;
        //}
        //// Register the shell extension for the desktop or the file explorer's background
        //{
        //    auto key = shared::format("HKEY_CLASSES_ROOT\\Directory\\Background\\shellex\\ContextMenuHandlers\\%s", class_name.c_str());
        //    if (Win32::Registry::CreateKey(key, guid) != ERROR_SUCCESS)
        //        return E_ACCESSDENIED;
        //}
        //// Register the shell extension for drives
        //{
        //    auto key = shared::format("HKEY_CLASSES_ROOT\\Drive\\shellex\\ContextMenuHandlers\\%s", class_name.c_str());
        //    if (Win32::Registry::CreateKey(key, guid) != ERROR_SUCCESS)
        //        return E_ACCESSDENIED;
        //}
        // Register the shell extension to the system's approved Shell Extensions
        {
            auto key = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";
            if (Win32::Registry::CreateKey(key) != ERROR_SUCCESS)
                return E_ACCESSDENIED;
            if (Win32::Registry::SetValueString(key, guid, class_name.c_str()))
                return E_ACCESSDENIED;
        }

        SHChangeNotify(SHCNE_ASSOCCHANGED, 0, 0, 0);
        return ERROR_SUCCESS;
    }
    DWORD Uninstall(const std::string& guid)
    {
        auto err = Win32::Registry::DeleteKey(shared::format("HKEY_CLASSES_ROOT\\CLSID\\%s", guid.c_str()));
        if (err != ERROR_SUCCESS)
            return err;
        SHChangeNotify(SHCNE_ASSOCCHANGED, 0, 0, 0);
        return ERROR_SUCCESS;
    }
}