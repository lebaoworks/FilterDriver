#include "Win32.h"

namespace Win32::Registry
{
    NTSTATUS OpenKey(_In_ UNICODE_STRING* KeyPath, _In_ ACCESS_MASK DesiredAccess, _Out_ HANDLE* Handle)
    {
        OBJECT_ATTRIBUTES oa;
        InitializeObjectAttributes(&oa, KeyPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
        return ZwOpenKey(Handle, DesiredAccess, &oa);
    }
    NTSTATUS GetValue(
        _In_ UNICODE_STRING* KeyPath,
        _In_ UNICODE_STRING* ValueName,
        _Out_writes_(DataSize) PVOID Data,
        _In_ ULONG DataSize,
        _Out_ ULONG* ResultSize)
    {
        *ResultSize = 0;

        // Open key
        HANDLE hKey = NULL;
        auto status = OpenKey(KeyPath, GENERIC_ALL, &hKey);
        if (status != STATUS_SUCCESS)
            return status;
        defer{ ZwClose(hKey); };
        
        // alloc buffer
        ULONG size = 0;
        status = ZwQueryValueKey(hKey, ValueName, KeyValueFullInformation, 0, 0, &size);
        if (status != STATUS_SUCCESS &&
            status != STATUS_BUFFER_TOO_SMALL &&
            status != STATUS_BUFFER_OVERFLOW)
            return status;
        auto buffer = new unsigned char[size];
        if (buffer == nullptr)
            return STATUS_NO_MEMORY;
        defer { delete[] buffer; };

        // query full value
        status = ZwQueryValueKey(hKey, ValueName, KeyValueFullInformation, buffer, size, &size);
        if (status != STATUS_SUCCESS)
            return status;
        auto info = reinterpret_cast<KEY_VALUE_FULL_INFORMATION*>(buffer);

        // setup output
        if (info->DataLength > DataSize)
            return STATUS_BUFFER_TOO_SMALL;
        memcpy(Data, buffer + info->DataOffset, info->DataLength);
        *ResultSize = info->DataLength;

        return STATUS_SUCCESS;
    }

    NTSTATUS SetValue(
        _In_ UNICODE_STRING* KeyPath,
        _In_ UNICODE_STRING* ValueName,
        _In_ ULONG ValueType,
        _In_reads_bytes_(DataSize) PVOID Data,
        _In_ ULONG DataSize)
    {
        // Open key
        HANDLE hKey = NULL;
        auto status = OpenKey(KeyPath, GENERIC_ALL, &hKey);
        if (status != STATUS_SUCCESS)
            return status;
        defer{ ZwClose(hKey); };

        // set value
        return ZwSetValueKey(hKey, ValueName, 0, ValueType, Data, DataSize);
    }
}