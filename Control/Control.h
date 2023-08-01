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
    namespace Request{
        struct Credential
        {
            UCHAR Password[256];
        };
        struct Header
        {
            UINT32 Signature = MESSAGE_SIGNATURE;   // 'B40z'
            UINT32 Size;                            // Total size of request include header
            UINT32 Type;                            // Type of request
            Credential Credential = {};
        };

        enum Type {
            Init = 0,
            Protect = 1,
            Unprotect = 2,
        };
        struct Init
        {
            Header Header = {};
        };
        struct Protect
        {
            Header Header = {};
            USHORT DosPathLength;
            WCHAR DosPath[1];
        };
        struct Unprotect
        {
            Header Header = {};
            USHORT DosPathLength;
            WCHAR DosPath[1];
        };
    }

    class Authenticator
    {
        UCHAR _credential[32];
    public:
        Authenticator(const UINT8 credential[32]);
        bool Verify(const Request::Credential& credential) const;
    };
}
#pragma pack(pop)

