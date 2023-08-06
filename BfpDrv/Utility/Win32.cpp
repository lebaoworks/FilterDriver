#include "Win32.h"

namespace Win32::Registry
{
    NTSTATUS OpenKey(const UNICODE_STRING& KeyPath, ACCESS_MASK DesiredAccess, HANDLE& handle)
    {
        OBJECT_ATTRIBUTES oa;
        InitializeObjectAttributes(&oa, const_cast<UNICODE_STRING*>(&KeyPath), OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
        return ZwOpenKey(&handle, DesiredAccess, &oa);
    }
    NTSTATUS GetBinaryString(const UNICODE_STRING& KeyPath, unsigned char*& Data, size_t& DataSize)
    {
        Data = nullptr;
        DataSize = 0;

        HANDLE hKey = NULL;
        auto status = OpenKey(KeyPath, GENERIC_ALL, hKey);
        if (status != STATUS_SUCCESS)
        {
            Log("OpenKey %wZ failed with status %08X", &KeyPath, status);
            return status;
        }
        Log("OpenKey %wZ success", &KeyPath);
        ZwClose(hKey);
        return STATUS_NOT_SUPPORTED;
    }
}