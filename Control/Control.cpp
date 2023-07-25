#include "Control.h"


Control::Authenticator::Authenticator(UCHAR credential[32])
{
    RtlCopyMemory(_credential, credential, 32);
}

bool Control::Authenticator::Verify(UCHAR password[32]) const
{
    return RtlEqualMemory(_credential, password, 32);
}

#ifndef _KERNEL_MODE
#include <memory>

Control::Controller::Controller(const std::wstring& device) : _device(device) {}
Control::Controller::~Controller() { if (_handle != INVALID_HANDLE_VALUE) CloseHandle(_handle); }

DWORD Control::Controller::Connect()
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

DWORD Control::Controller::RequestProtect(const std::wstring& path) const
{
    auto buffer = std::make_unique<byte[]>(sizeof(Control::RequestProtect) + path.size() * sizeof(wchar_t));
    Control::RequestProtect* req = new (buffer.get()) Control::RequestProtect();
    req->Header.Type = Control::RequestType::Protect;
    req->Header.Size = sizeof(Control::RequestProtect) + path.size() * sizeof(wchar_t);
    req->DosPathLength = path.size() * sizeof(wchar_t);
    RtlCopyMemory(req->DosPath, path.c_str(), path.size() * sizeof(wchar_t));

    Control::ResponseHeader resp;

    if (DeviceIoControl(
        _handle,                // Device
        IOCTL_BFP_CONTROL,      // IoControlCode
        (LPVOID)req,            // InputBuffer
        req->Header.Size,       // InputBufferLength
        (LPVOID)&resp,          // OutputBuffer
        sizeof(resp),           // OutputBufferLength
        NULL,                   // BytesReturned
        NULL) == FALSE)         // Overlapped
        return GetLastError();
    return ERROR_SUCCESS;
}
#endif
