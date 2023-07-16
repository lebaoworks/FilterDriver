#pragma once
#include <Windows.h>
#include <string>

class Controller
{
private:
    std::wstring _device;
    HANDLE _handle = INVALID_HANDLE_VALUE;
public:
    Controller(const std::wstring& device);
    ~Controller();

    DWORD Connect();
    DWORD Control();
};
