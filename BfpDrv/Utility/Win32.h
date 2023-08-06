#pragma once

#include <Shared.h>

namespace Win32
{
    namespace Registry
    {
        NTSTATUS OpenKey(const UNICODE_STRING& KeyPath, HANDLE& handle);
        NTSTATUS GetBinaryString(const UNICODE_STRING& KeyPath, unsigned char*& Data, size_t& DataSize);
    }
}