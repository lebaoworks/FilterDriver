#include <Windows.h>

#include <Control.h>

#ifdef _DEBUG
#include <iostream>

int main()
{
    Control::Controller controller(CONTROL_POINT);
    auto status = controller.Connect();
    if (status != ERROR_SUCCESS)
    {
        printf("Connect to %ws failed %X", CONTROL_POINT, status);
        return 1;
    }
    status = controller.RequestProtect(L"C:\\Windows");
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