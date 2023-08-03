#pragma once

#ifdef _KERNEL_MODE
#include <fltKernel.h>
#else
#include <Windows.h>
#include <string>
#endif

#define COMPORT_NAME L"\\BfpDrvPort"

//#define IOCTL_BFP_CONTROL CTL_CODE(FILE_DEVICE_UNKNOWN, 0xba0, METHOD_BUFFERED, FILE_ANY_ACCESS)

#pragma pack(push, 1)
namespace Communication
{
    struct Credential
    {
        UCHAR Password[256];
    };

    enum Type {
        Init = 0,
        Protect = 1,
        Unprotect = 2,
    };

    struct MessageHeader
    {
        UINT32 Type;        // Type of request
        UINT32 Size;        // Total size of request include header
    };

    struct MessageProtect
    {
        static const UINT32 Type = Communication::Type::Protect;
        WCHAR DosPath[256];
    };

    struct MessageUnprotect
    {
        static const UINT32 Type = Communication::Type::Unprotect;
        WCHAR DosPath[256];
    };

    template<typename T>
    struct ClientMessage
    {
        MessageHeader Header;   // Header of request
        T             Body;     // Body of request
        ClientMessage() { Header.Type = T::Type; Header.Size = sizeof(T); }
    };
}
#pragma pack(pop)

