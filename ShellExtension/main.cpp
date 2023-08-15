#define _CRT_SECURE_NO_WARNINGS

#pragma comment(linker, "/export:DllMain")
#pragma comment(linker, "/export:DllGetClassObject,PRIVATE")
#pragma comment(linker, "/export:DllCanUnloadNow,PRIVATE")
#pragma comment(linker, "/export:DllRegisterServer,PRIVATE")
#pragma comment(linker, "/export:DllUnregisterServer,PRIVATE")

#include "main.h"

// Builtin Libraries
/// C++
#include <memory>
/// Windows
#include <Windows.h>
#include <Fltuser.h>
#include <atlbase.h>
#include <atlcom.h>

// Shared Libraries
#include <Shared.h>
#include <ShellExt.h>
#include <Win32.h>
#include <Communication.h>

#include "ClassFactory.h"

//CComModule _Module;

HRESULT WINAPI DllMain(
    _In_ HMODULE hModule,
    _In_ DWORD dwReason,
    _In_ LPVOID lpReserved)
{
    Log("");
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
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
    if (!IsEqualGUID(rclsid, CLSID_ShellMenu))
        return CLASS_E_CLASSNOTAVAILABLE;

    auto class_factory = new ClassFactory();
    auto hr = class_factory->QueryInterface(riid, ppv);
    class_factory->Release();

    LogDebug("Search interface %s -> %s", ShellExt::GuidToString(riid).c_str(), SUCCEEDED(hr) ? "Found" : "Unknown");
    return hr;
}

__control_entrypoint(DllExport)
STDAPI DllCanUnloadNow(void)
{
    auto ref = ShellExt::DllReferenceCount();
    if (ref == 0)
    {
        LogDebug("-> Yes");
        return S_OK;
    }
    LogDebug("-> No. Remaining: %d", ref);
    return S_FALSE;
}

STDAPI DllRegisterServer(void)
{
    try
    {
        auto module_path = Win32::Module::GetCurrentModulePath();
        auto guid = ShellExt::GuidToString(CLSID_ShellMenu);
        auto result = ShellExt::Setup::Install(module_path, guid, SHELL_EXTENSION_CLASS_NAME);
        Log("-> %d", result);
    }
    catch (std::exception& e)
    {
        Log("Got except: %s", e.what());
        return E_UNEXPECTED;
    }
    return S_OK;
}

STDAPI DllUnregisterServer(void)
{
    try
    {
        auto guid = ShellExt::GuidToString(CLSID_ShellMenu);
        auto result = ShellExt::Setup::Uninstall(guid);
        Log("-> %d", result);
    }
    catch (std::exception& e)
    {
        Log("Got except: %s", e.what());
        return E_UNEXPECTED;
    }
    return S_OK;
}


#ifdef _DEBUG
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>

class FilterConnection
{
private:
    HANDLE _port = NULL;
public:
    FilterConnection(const std::wstring& comport_name, const std::string& comport_password)
    {
        Communication::Credential credential;
        strcpy(reinterpret_cast<char*>(credential.Password), comport_password.c_str());
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

void test_protect(const std::string& password, std::wstring& path)
{
    try {
        auto connection = FilterConnection(COMPORT_NAME, password);
        Communication::ClientMessage<Communication::MessageProtect> message;
        wcscpy_s(message.Body.DosPath, path.c_str());
        connection.Send(message);
    } catch (std::exception& e)
    {
        MessageBoxA(NULL, e.what(), "Error", MB_OK);
    }
}

void test_unprotect(const std::string& password, std::wstring& path)
{
    try {
        auto connection = FilterConnection(COMPORT_NAME, password);
        Communication::ClientMessage<Communication::MessageUnprotect> message;
        wcscpy_s(message.Body.DosPath, path.c_str());
        connection.Send(message);
    }
    catch (std::exception& e)
    {
        MessageBoxA(NULL, e.what(), "Error", MB_OK);
    }
}

#include <wincred.h>
std::string get_password()
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
        throw std::runtime_error(shared::format("call api failed %d", result));
    defer{ CoTaskMemFree(outCredBuffer); };
    
    WCHAR username[CREDUI_MAX_USERNAME_LENGTH + 1] = {};
    DWORD usernameCount = CREDUI_MAX_USERNAME_LENGTH + 1;
    WCHAR password[CREDUI_MAX_PASSWORD_LENGTH + 1] = {};
    DWORD passwordCount = CREDUI_MAX_PASSWORD_LENGTH + 1;
    if (FALSE == CredUnPackAuthenticationBufferW(
        CRED_PACK_PROTECTED_CREDENTIALS,
        outCredBuffer,
        outCredSize,
        username,
        &usernameCount,
        nullptr,
        nullptr,
        password,
        &passwordCount))
        throw std::runtime_error(shared::format("call api unpack failed %d", GetLastError()));
    auto wstr = std::wstring(password, passwordCount);
    return std::string(wstr.begin(), wstr.end());
}

int main(int argc, char* argv[])
{
    //auto c = std::string(argv[1]);
    //auto path = std::string(argv[2]);
    //auto wpath = std::wstring(path.begin(), path.end());

    //if (c == "1")
    //{
    //    // do protect
    //    auto password = get_password();
    //    test_protect(password, wpath);
    //}
    //if (c == "2")
    //{
    //    // do unprotect
    //    auto password = get_password();
    //    test_unprotect(password, wpath);
    //}

    std::string module_path = R"(C:\Users\test\Desktop\Artifact\ShellExtension.dll)";
    std::string guid = ShellExt::GuidToString(CLSID_ShellMenu);

    std::string mode = argv[1];
    if (mode == "install")
    {
        auto err = ShellExt::Setup::Install(module_path, guid, SHELL_EXTENSION_CLASS_NAME);
        printf("Install %s -> %d", guid.c_str(), err);
    }
    else if (mode == "uninstall")
    {
        auto err = ShellExt::Setup::Uninstall(guid);
        printf("Uninstall -> %d", err);
    }
    else if (mode == "ask_credential")
    {
        try
        {
            printf("Password: %s\n", get_password().c_str());
        }
        catch (std::exception& e)
        {
            printf("Except: %s", e.what());
        }
    }
    else
    {
        printf("Invalid mode");
        return 1;
    }
    return 0;
}
#endif