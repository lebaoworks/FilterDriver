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
#include <atlbase.h>
#include <atlcom.h>

// Shared Libraries
#include <Shared.h>
#include <ShellExt.h>
#include <Win32.h>

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


int main(int argc, char**argv)
{
    if (std::string(argv[1]) == "install")
        printf("Install -> %d", ShellExt::Setup::Install(Win32::Module::GetCurrentModulePath(), ShellExt::GuidToString(CLSID_ShellMenu), SHELL_EXTENSION_CLASS_NAME));
    if (std::string(argv[1]) == "uninstall")
        printf("Uninstall -> %d", ShellExt::Setup::Uninstall(ShellExt::GuidToString(CLSID_ShellMenu)));
    return 0;
}