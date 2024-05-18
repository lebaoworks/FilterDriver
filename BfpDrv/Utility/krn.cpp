#include "krn.h"

/*********************
*         C++        *
*********************/

//
//unicode_string::unicode_string(size_t size)
//{
//    _raw.Length = 0;
//    _raw.MaximumLength = (USHORT)size;
//    _raw.Buffer = new WCHAR[size/2+1];
//    if (_raw.Buffer == nullptr)
//        failable_object::_error = true;
//}
//unicode_string::~unicode_string()
//{
//    if (!error())
//        delete[]_raw.Buffer;
//}
//UNICODE_STRING& unicode_string::raw()
//{
//    return _raw;
//}

 
//namespace Win32::Registry
//{
//    NTSTATUS OpenKey(_In_ UNICODE_STRING* KeyPath, _In_ ACCESS_MASK DesiredAccess, _Out_ HANDLE* Handle)
//    {
//        OBJECT_ATTRIBUTES oa;
//        InitializeObjectAttributes(&oa, KeyPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
//        return ZwOpenKey(Handle, DesiredAccess, &oa);
//    }
//    NTSTATUS GetValue(
//        _In_ UNICODE_STRING* KeyPath,
//        _In_ UNICODE_STRING* ValueName,
//        _Out_writes_(DataSize) PVOID Data,
//        _In_ ULONG DataSize,
//        _Out_ ULONG* ResultSize)
//    {
//        *ResultSize = 0;
//
//        // Open key
//        HANDLE hKey = NULL;
//        auto status = OpenKey(KeyPath, GENERIC_ALL, &hKey);
//        if (status != STATUS_SUCCESS)
//            return status;
//        defer{ ZwClose(hKey); };
//        
//        // alloc buffer
//        ULONG size = 0;
//        status = ZwQueryValueKey(hKey, ValueName, KeyValueFullInformation, 0, 0, &size);
//        if (status != STATUS_SUCCESS &&
//            status != STATUS_BUFFER_TOO_SMALL &&
//            status != STATUS_BUFFER_OVERFLOW)
//            return status;
//        auto buffer = new unsigned char[size];
//        if (buffer == nullptr)
//            return STATUS_NO_MEMORY;
//        defer { delete[] buffer; };
//
//        // query full value
//        status = ZwQueryValueKey(hKey, ValueName, KeyValueFullInformation, buffer, size, &size);
//        if (status != STATUS_SUCCESS)
//            return status;
//        auto info = reinterpret_cast<KEY_VALUE_FULL_INFORMATION*>(buffer);
//
//        // setup output
//        if (info->DataLength > DataSize)
//            return STATUS_BUFFER_TOO_SMALL;
//        memcpy(Data, buffer + info->DataOffset, info->DataLength);
//        *ResultSize = info->DataLength;
//
//        return STATUS_SUCCESS;
//    }
//
//    NTSTATUS SetValue(
//        _In_ UNICODE_STRING* KeyPath,
//        _In_ UNICODE_STRING* ValueName,
//        _In_ ULONG ValueType,
//        _In_reads_bytes_(DataSize) PVOID Data,
//        _In_ ULONG DataSize)
//    {
//        // Open key
//        HANDLE hKey = NULL;
//        auto status = OpenKey(KeyPath, GENERIC_ALL, &hKey);
//        if (status != STATUS_SUCCESS)
//            return status;
//        defer{ ZwClose(hKey); };
//
//        // set value
//        return ZwSetValueKey(hKey, ValueName, 0, ValueType, Data, DataSize);
//    }
//}
//
//namespace Win32::Path
//{
//    NTSTATUS QueryTarget(_In_ UNICODE_STRING* SymbolicPath, _Out_ UNICODE_STRING* TargetPath)
//    {
//        TargetPath->Length = 0;
//
//        HANDLE LinkHandle;
//        OBJECT_ATTRIBUTES Attributes;
//        InitializeObjectAttributes(&Attributes, SymbolicPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
//        auto status = ZwOpenSymbolicLinkObject(&LinkHandle, GENERIC_READ, &Attributes);
//        if (status != STATUS_SUCCESS)
//            return status;
//        defer{ ZwClose(LinkHandle); };
//
//        ULONG len = 0;
//        return ZwQuerySymbolicLinkObject(LinkHandle, TargetPath, &len);
//    }
//
//    NTSTATUS QueryAbsoluteTarget(_In_ UNICODE_STRING* SymbolicPath, _Out_ UNICODE_STRING* TargetPath)
//    {
//        TargetPath->Length = 0;
//        auto x = std::wstring(SymbolicPath->Buffer, SymbolicPath->Length / 2);
//        if (x.error())
//            return STATUS_NO_MEMORY;
//
//        std::list<std::wstring> subs;
//        size_t pos = 0;
//        auto i = x.find(L"\\", 0);
//        while (pos <= x.length())
//        {
//            if (i == std::wstring::npos)
//            {
//                subs.push_back(x.substr(pos, x.length() - pos + 1));
//                break;
//            }
//            subs.push_back(x.substr(pos, i - pos));
//            pos = i + 1;
//            i = x.find(L"\\", pos);
//        }
//
//        // alloc dup buffer
//        auto buffer = new WCHAR[1024];
//        if (buffer == nullptr)
//            return STATUS_NO_MEMORY;
//
//        defer{ delete[] buffer; };
//        auto _TargetPath = reinterpret_cast<PUNICODE_STRING>(buffer);
//        _TargetPath->Length = 0;
//        _TargetPath->MaximumLength = 2000;
//        _TargetPath->Buffer = buffer + sizeof(UNICODE_STRING) / sizeof(WCHAR);
//
//        for (auto it = subs.begin(); it != subs.end();)
//        {
//            RtlUnicodeStringCatString(_TargetPath, it->c_str());
//            //Log("From: %wZ", _TargetPath);
//            if (QueryTarget(_TargetPath, TargetPath) == STATUS_SUCCESS)
//                RtlUnicodeStringCopy(_TargetPath, TargetPath);
//            //Log("To: %wZ", _TargetPath);
//            it++;
//            if (it != subs.end())
//                RtlUnicodeStringCatString(_TargetPath, L"\\");
//        }
//        RtlUnicodeStringCopy(TargetPath, _TargetPath);
//        return STATUS_SUCCESS;
//    }
//}