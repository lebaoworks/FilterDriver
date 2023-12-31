#include "Win32.h"

// Builtin Libraries
#include <utility>
#include <windows.h>
#include <wincred.h>

// Shared Libraries
#include "Shared.h"


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
    LSTATUS CheckValue(const std::string& key, const std::string& value)
    {
        HKEY h;
        auto ret = OpenKey(key, h);
        if (ret != ERROR_SUCCESS)
            return ret;
        defer{ RegCloseKey(h); };
        BYTE buffer[1024];
        DWORD size = 1024;
        DWORD type = REG_SZ;
        return RegQueryValueExA(h, value.c_str(), NULL, &type, buffer, &size);
    }
}

namespace Win32::Module
{
    std::string GetCurrentModulePath()
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
}

namespace Win32::Credential
{
    DWORD GetCredential(std::string& username, std::string& password)
    {
        CREDUI_INFOW credui = {};
        credui.cbSize = sizeof(credui);
        credui.hwndParent = nullptr;
        credui.pszMessageText = L"Enter password:";
        credui.pszCaptionText = L"Bfp Password Prompt";
        credui.hbmBanner = nullptr;

        ULONG authPackage = 0;
        LPVOID outCredBuffer = nullptr;
        ULONG outCredSize = 0;
        BOOL save = false;

        DWORD result = CredUIPromptForWindowsCredentialsW(
            &credui,
            0,
            &authPackage,
            nullptr,
            0,
            &outCredBuffer,
            &outCredSize,
            &save,
            CREDUIWIN_GENERIC);
        if (result != ERROR_SUCCESS)
            return result;
        defer{ CoTaskMemFree(outCredBuffer); };

        WCHAR usr[CREDUI_MAX_USERNAME_LENGTH + 1] = {};
        DWORD usrCch = CREDUI_MAX_USERNAME_LENGTH + 1;
        WCHAR passwd[CREDUI_MAX_PASSWORD_LENGTH + 1] = {};
        DWORD passwdCch = CREDUI_MAX_PASSWORD_LENGTH + 1;
        if (FALSE == CredUnPackAuthenticationBufferW(
            CRED_PACK_PROTECTED_CREDENTIALS,
            outCredBuffer,
            outCredSize,
            usr,
            &usrCch,
            nullptr,
            nullptr,
            passwd,
            &passwdCch))
            return GetLastError();

        auto w_usr = std::wstring(usr);
        auto w_passwd = std::wstring(passwd);

        username = std::string(w_usr.begin(), w_usr.end());
        password = std::string(w_passwd.begin(), w_passwd.end());
        return ERROR_SUCCESS;
    }
}