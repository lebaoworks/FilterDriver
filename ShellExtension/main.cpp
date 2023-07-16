#include <Windows.h>

#include "Controller.h"

#ifdef _DEBUG
#include <iostream>

int main()
{
    std::wstring device = L"\\\\.\\BfpDevice";
    Controller controller(device);
    auto status = controller.Connect();
    if (status != ERROR_SUCCESS)
    {
        printf("Connect to %ws failed %X", device.c_str(), status);
        return 1;
    }
    status = controller.Control();
    printf("Control -> %X", status);
    return 0;
}
#else
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
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
#endif