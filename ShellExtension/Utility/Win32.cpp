#include "Win32.h"

// Builtin Libraries
#include <utility>
#include <windows.h>

// Shared Libraries
#include <Shared.h>


namespace Win32::Registry {
    static std::pair<HKEY, std::string> ParseKey(const std::string& key)
    {
        if (key.starts_with("HKEY_CLASSES_ROOT\\"))
            return { HKEY_CLASSES_ROOT, key.substr(18, key.size()) };
        if (key.starts_with("HKEY_CURRENT_USER\\"))
            return { HKEY_CURRENT_USER, key.substr(18, key.size()) };
        if (key.starts_with("HKEY_LOCAL_MACHINE\\"))
            return { HKEY_LOCAL_MACHINE, key.substr(19, key.size()) };
        if (key.starts_with("HKEY_USERS\\"))
            return { HKEY_USERS, key.substr(11, key.size()) };
        if (key.starts_with("HKEY_CURRENT_CONFIG\\"))
            return { HKEY_CURRENT_CONFIG, key.substr(20, key.size()) };
        return { nullptr, "" };
    }

    LSTATUS OpenKey(const std::string& key, HKEY& handle)
    {
        auto k = ParseKey(key);
        return RegOpenKeyExA(
            k.first,                    // hKey
            k.second.c_str(),           // lpSubKey
            0,                          // ulOptions
            KEY_ALL_ACCESS,             // samDesired
            &handle                     // phkResult
        );
    }

    LSTATUS CreateKey(const std::string& key)
    {
        auto k = ParseKey(key);
        HKEY hKey = NULL;
        auto ret = RegCreateKeyExA(
            k.first,                    // hKey
            k.second.c_str(),           // lpSubKey
            0,                          // Reserved
            nullptr,                    // lpClass
            REG_OPTION_NON_VOLATILE,    // dwOptions
            KEY_ALL_ACCESS,             // samDesired
            nullptr,                    // lpSecurityAttributes
            &hKey,                      // phkResult
            nullptr                     // lpdwDisposition
        );
        if (ret == ERROR_SUCCESS)
            RegCloseKey(hKey);
        return ret;
    }

    LSTATUS DeleteKey(const std::string& key)
    {
        auto k = ParseKey(key);
        return RegDeleteTreeA(k.first, k.second.c_str());
    }

    LSTATUS SetValueString(const std::string& key, const std::string& value, const std::string& data)
    {
        HKEY hkey = NULL;
        auto ret = OpenKey(key, hkey);
        if (ret != ERROR_SUCCESS)
            return ret;
        return RegSetValueExA(
            hkey,                       // hKey
            value.c_str(),              // lpValueName
            0,                          // Reserved
            REG_SZ,                     // dwType
            (const BYTE*)data.c_str(),  // lpData
            (DWORD)data.size()          // cbData
        );
    }

    LSTATUS CreateKey(const std::string& key, const std::string& default_value)
    {
        auto ret = CreateKey(key);
        if (ret != ERROR_SUCCESS)
            return ret;
        return SetValueString(key, "", default_value);
    }
}

std::string Win32::Module::GetCurrentModulePath()
{
    char path[MAX_PATH] = "\0";
    HMODULE module = NULL;

    if (GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |        // dwFlags
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,   // dwFlags
        (LPCSTR)&GetCurrentModulePath,                  // lpModuleName
        &module) == FALSE)                              // phModule
    {
        int ret = GetLastError();
        fprintf(stderr, "GetModuleHandle failed, error = %d\n", ret);
        return "";
    }
    if (GetModuleFileNameA(module, path, MAX_PATH) == 0)
    {
        int ret = GetLastError();
        fprintf(stderr, "GetModuleFileName failed, error = %d\n", ret);
        return "";
    }

    return path;
}