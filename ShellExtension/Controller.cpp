#include "Controller.h"

#define IOCTL_BFP_CONTROL CTL_CODE(FILE_DEVICE_UNKNOWN, 0xba0, METHOD_BUFFERED, FILE_ANY_ACCESS)

Controller::Controller(const std::wstring& device) : _device(device) {}

Controller::~Controller()
{
    if (_handle != INVALID_HANDLE_VALUE)
        CloseHandle(_handle);
}

DWORD Controller::Connect()
{
    _handle = CreateFile(
        _device.c_str(),                // DeviceName
        GENERIC_READ | GENERIC_WRITE,   // DesiredAccess
        0,                              // ShareMode
        NULL,                           // SecurityAttributes
        OPEN_EXISTING,                  // CreationDisposition
        FILE_ATTRIBUTE_NORMAL,          // FlagsAndAttributes
        NULL);                          // TemplateFile
    if (_handle == INVALID_HANDLE_VALUE)
        return GetLastError();
    return ERROR_SUCCESS;
}

DWORD Controller::Control()
{
    if (DeviceIoControl(
        _handle,                // Device
        IOCTL_BFP_CONTROL,      // IoControlCode
        (LPVOID) "a",           // InputBuffer
        1,                      // InputBufferLength
        NULL,                   // OutputBuffer
        0,                      // OutputBufferLength
        NULL,                   // BytesReturned
        NULL) == FALSE)         // Overlapped
        return GetLastError();
    return ERROR_SUCCESS;
}