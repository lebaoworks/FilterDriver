#pragma once

#ifdef _KERNEL_MODE
#include <fltKernel.h>
#else
#include <Windows.h>
#include <string>
#endif

#define CONTROL_POINT L"\\\\.\\BfpDevice"
#define MESSAGE_SIGNATURE 'B40z'
#define IOCTL_BFP_CONTROL CTL_CODE(FILE_DEVICE_UNKNOWN, 0xba0, METHOD_BUFFERED, FILE_ANY_ACCESS)

#pragma pack(push, 1)
namespace Control
{
    enum RequestType
    {
        Protect = 0,
        Unprotect,
    };

    struct RequestHeader
    {
        UINT32 Signature = MESSAGE_SIGNATURE;   // 'B40z'
        UINT32 Size;                            // Total size of request include header
        UINT32 Type;                            // Type of request
        UCHAR  Password[32];                    // Password for authentication
    };
    struct ResponseHeader
    {
        NTSTATUS Status;
    };

    struct RequestProtect
    {
        RequestHeader Header = {};
        USHORT DosPathLength;
        WCHAR DosPath[1];
    };
    struct RequestUnprotect
    {
        RequestHeader Header;
        USHORT DosPathLength;
        WCHAR DosPath[1];
    };


    class Authenticator
    {
        UCHAR _credential[32];
    public:
        Authenticator(UCHAR credential[32]);
        bool Verify(UCHAR password[32]) const;
    };

#ifndef _KERNEL_MODE
    class Controller
    {
    private:
        std::wstring _device;
        HANDLE _handle = INVALID_HANDLE_VALUE;
    public:
        Controller(const std::wstring& device);
        ~Controller();

        DWORD Connect();
        DWORD RequestProtect(const std::wstring& path) const;
        DWORD ProtectUnprotect(const std::wstring& path) const;
    };
#endif
}
#pragma pack(pop)

