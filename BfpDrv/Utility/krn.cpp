#include "krn.h"

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


namespace krn
{
    constexpr UNICODE_STRING VOLUME_PREFIX = RTL_CONSTANT_STRING(L"\\??\\Volume{");

    namespace path
    {
        NTSTATUS QuerySymbolicLink(
            _In_ UNICODE_STRING& NtPath,
            _When_(return == STATUS_SUCCESS, __drv_allocatesMem(Mem) _Outptr_) PUNICODE_STRING& TargetPath)
        {
            // Open as symbolic link object
            HANDLE handle;
            OBJECT_ATTRIBUTES oa;
            InitializeObjectAttributes(&oa, &NtPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
            auto status = ZwOpenSymbolicLinkObject(&handle, SYMBOLIC_LINK_QUERY, &oa);
            if (status != STATUS_SUCCESS)
                return status;
            defer{ ZwClose(handle); };
    
            // Prepare output
            UNICODE_STRING* path = reinterpret_cast<UNICODE_STRING*>(ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(UNICODE_STRING) + 256, '++C0'));
            if (path == NULL)
                return STATUS_NO_MEMORY;
            defer{ if (path != NULL) ExFreePool(path); };
            path->Buffer = reinterpret_cast<WCHAR*>(path + 1);
            path->MaximumLength = 256;

            ULONG len = 0;
            
            // Initial query
            status = ZwQuerySymbolicLinkObject(handle, path, &len);
            if (status == STATUS_SUCCESS)
            {
                TargetPath = path;
                path = NULL;
                return STATUS_SUCCESS;
            }
            if (status != STATUS_BUFFER_TOO_SMALL)
                return status;

            // Reallocate
            ExFreePool(path);
            path = reinterpret_cast<UNICODE_STRING*>(ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(UNICODE_STRING) + len, '++C0'));
            if (path == NULL)
                return STATUS_NO_MEMORY;
            path->Buffer = reinterpret_cast<WCHAR*>(path + 1);
            path->MaximumLength = static_cast<USHORT>(len); // This cast is safe cause len returned from api

            status = ZwQuerySymbolicLinkObject(handle, path, &len);
            if (status == STATUS_SUCCESS)
            {
                TargetPath = path;
                path = NULL;
                return STATUS_SUCCESS;
            }
            return status;
        }

        NTSTATUS QueryReparsePoint(
            _In_ UNICODE_STRING& NtPath,
            _When_(return == STATUS_SUCCESS, __drv_allocatesMem(Mem) _Outptr_) PREPARSE_DATA_BUFFER& ReparseData)
        {
            ReparseData = NULL;

            // Open path
            HANDLE handle;
            IO_STATUS_BLOCK iosb;
            OBJECT_ATTRIBUTES oa;
            InitializeObjectAttributes(&oa, &NtPath, OBJ_CASE_INSENSITIVE, NULL, NULL);            

            NTSTATUS status = ZwCreateFile(
                &handle,
                FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                &oa,
                &iosb,
                NULL,
                FILE_ATTRIBUTE_NORMAL,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                FILE_OPEN,
                FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT | FILE_SYNCHRONOUS_IO_NONALERT,
                NULL,
                0);
            if (status != STATUS_SUCCESS)
                return status;
            defer{ ZwClose(handle); };

            // Get reparse point
            ULONG buffer_size = 4096; // Must be large enough to hold REPARSE_GUID_DATA_BUFFER
            while (true)
            {
                // Alloc buffer for output
                auto buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, buffer_size, '++C0');
                if (buffer == NULL)
                    return STATUS_NO_MEMORY;
                defer { if (buffer != NULL) ExFreePool(buffer); };

                // Query
                status = ZwFsControlFile(
                    handle,
                    NULL,
                    NULL,
                    NULL,
                    &iosb,
                    FSCTL_GET_REPARSE_POINT,
                    NULL,
                    0,
                    buffer,
                    buffer_size);
                if (status == STATUS_SUCCESS)
                {
                    ReparseData = reinterpret_cast<REPARSE_DATA_BUFFER*>(buffer);
                    buffer = NULL;
                    return STATUS_SUCCESS;
                }
                if (status == STATUS_BUFFER_OVERFLOW)
                {
                    buffer_size += reinterpret_cast<REPARSE_DATA_BUFFER*>(buffer)->ReparseDataLength;
                    continue;
                }
                return status;
            }
        }

        NTSTATUS QueryVolumeDevice(
            _Inout_ UNICODE_STRING& Volume,
            _When_(return == STATUS_SUCCESS, __drv_allocatesMem(Mem) _Outptr_) PMOUNTDEV_NAME& Device)
        {
            if (Volume.Length < VOLUME_PREFIX.Length)
                return STATUS_INVALID_PARAMETER;

            // Remove slash at back
            bool back_is_slash = Volume.Buffer[Volume.Length / 2 - 1] == L'\\';
            if (back_is_slash) Volume.Length -= 2;
            defer {if (back_is_slash) Volume.Length +=2; };

            // Open volume
            HANDLE handle;
            IO_STATUS_BLOCK iosb;
            OBJECT_ATTRIBUTES oa;
            InitializeObjectAttributes(&oa, &Volume, OBJ_CASE_INSENSITIVE, NULL, NULL);
            NTSTATUS status = ZwOpenFile(
                &handle,
                FILE_READ_ATTRIBUTES | SYNCHRONIZE,
                &oa,
                &iosb,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                FILE_SYNCHRONOUS_IO_ALERT);
            if (status != STATUS_SUCCESS)
                return status;
            defer{ ZwClose(handle); };

            // Get device name
            ULONG buffer_size = 4096; // Must be large enough to hold MOUNTDEV_NAME
            while (true)
            {
                // Alloc buffer for output
                auto buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, buffer_size, '++C0');
                if (buffer == NULL)
                    return STATUS_NO_MEMORY;
                defer{ if (buffer != NULL) ExFreePool(buffer); };

                // Query
                status = ZwDeviceIoControlFile(
                    handle,
                    NULL,
                    NULL,
                    NULL,
                    &iosb,
                    IOCTL_MOUNTDEV_QUERY_DEVICE_NAME,
                    NULL,
                    0,
                    buffer,
                    buffer_size);
                if (status == STATUS_SUCCESS)
                {
                    Device = reinterpret_cast<MOUNTDEV_NAME*>(buffer);
                    buffer = NULL;
                    return STATUS_SUCCESS;
                }
                if (status == STATUS_BUFFER_OVERFLOW)
                {
                    buffer_size += reinterpret_cast<MOUNTDEV_NAME*>(buffer)->NameLength;
                    continue;
                }
                return status;
            }
        }
        
        NTSTATUS FinalizePath(
            _In_ UNICODE_STRING& NtPath,
            _When_(return == STATUS_SUCCESS, __drv_allocatesMem(Mem) _Outptr_) PUNICODE_STRING& FinalPath)
        {
            UNICODE_STRING* path = reinterpret_cast<UNICODE_STRING*>(ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(UNICODE_STRING) + NtPath.MaximumLength, '++C0'));
            if (path == NULL)
                return STATUS_NO_MEMORY;
            defer { if (path != NULL) ExFreePool(path); };
            path->Buffer = reinterpret_cast<WCHAR*>(path + 1);
            path->MaximumLength = NtPath.MaximumLength;

            auto status = RtlUnicodeStringCopy(path, &NtPath);
            if (status != STATUS_SUCCESS)
                return status;

            USHORT i = 0;
            while (i <= path->Length / 2)
            {
                if (path->Buffer[i] == L'\\' || i == path->Length/2)
                {
                    UNICODE_STRING tmp;
                    tmp.Buffer = path->Buffer;
                    tmp.MaximumLength = tmp.Length = i*2;

                    Log("tmp -> %wZ", &tmp);

                    auto replace_path_prefix = [&](UNICODE_STRING& Replace) -> NTSTATUS {
                        // Check if able to replace
                        USHORT remaining_cb = path->Length - tmp.Length;
                        if (MAXUSHORT - remaining_cb < Replace.Length)
                            return STATUS_BUFFER_OVERFLOW;
                        
                        // Make new path
                        USHORT new_cb = Replace.Length + remaining_cb;
                        UNICODE_STRING* new_path = reinterpret_cast<UNICODE_STRING*>(ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(UNICODE_STRING) + new_cb, '++C0'));
                        if (new_path == NULL)
                            return STATUS_NO_MEMORY;
                        defer{ if (new_path != NULL) ExFreePool(new_path); };
                        new_path->Buffer = reinterpret_cast<WCHAR*>(new_path + 1);;
                        new_path->MaximumLength = new_cb;

                        // Copy prefix
                        status = RtlUnicodeStringCopy(new_path, &Replace);
                        if (status != STATUS_SUCCESS)
                            return status;

                        // Concatenate remaining
                        UNICODE_STRING remaining;
                        remaining.Buffer = &path->Buffer[i];
                        remaining.Length = remaining.MaximumLength =  remaining_cb;
                        status = RtlUnicodeStringCat(new_path, &remaining);
                        if (status != STATUS_SUCCESS)
                            return status;

                        // Swap
                        ExFreePool(path);
                        path = new_path;
                        new_path = NULL;
                        return STATUS_SUCCESS;
                    };

                    // Check parse point
                    REPARSE_DATA_BUFFER* reparse = NULL;
                    status = QueryReparsePoint(tmp, reparse);
                    if (status == STATUS_SUCCESS)
                    {
                        defer{ ExFreePool(reparse); };
                        switch (reparse->ReparseTag)
                        {
                        case IO_REPARSE_TAG_SYMLINK:
                        {
                            // TODO: Parse filesystem symbolic link

                            //UNICODE_STRING raw;
                            //raw.Buffer = (wchar_t*)((UCHAR*)reparse->SymbolicLinkReparseBuffer.PathBuffer + reparse->SymbolicLinkReparseBuffer.SubstituteNameOffset);
                            //raw.MaximumLength = raw.Length = reparse->SymbolicLinkReparseBuffer.SubstituteNameLength;
                            
                            break;
                        }
                        case IO_REPARSE_TAG_MOUNT_POINT:
                        {
                            UNICODE_STRING raw;
                            raw.Buffer = (wchar_t*)((UCHAR*)reparse->MountPointReparseBuffer.PathBuffer + reparse->MountPointReparseBuffer.SubstituteNameOffset);
                            raw.MaximumLength = raw.Length = reparse->MountPointReparseBuffer.SubstituteNameLength;

                            if (RtlPrefixUnicodeString(&VOLUME_PREFIX, &raw, TRUE) == TRUE)
                            {
                                MOUNTDEV_NAME* device = NULL;
                                status = QueryVolumeDevice(raw, device);
                                if (status == STATUS_SUCCESS)
                                {
                                    defer{ ExFreePool(device); };
    
                                    UNICODE_STRING name;
                                    name.Buffer = device->Name;
                                    name.MaximumLength = name.Length = device->NameLength;
                                    Log("ParsePointDevice -> %wZ", &name);

                                    status = replace_path_prefix(name);
                                    if (status == STATUS_SUCCESS)
                                    {
                                        i = 0;
                                        Log("Replace Volume -> %wZ", path);
                                        continue;
                                    }
                                }
                            }
                            else
                            {
                                status = replace_path_prefix(raw);
                                if (status == STATUS_SUCCESS)
                                {
                                    i = 0;
                                    Log("Replace Mountpoint -> %wZ", path);
                                    continue;
                                }
                            }
                            break;
                        }
                        }
                    }

                    // Check symbolic link
                    UNICODE_STRING* symbolic_target = NULL;
                    status = QuerySymbolicLink(tmp, symbolic_target);
                    if (status == STATUS_SUCCESS)
                    {
                        defer { ExFreePool(symbolic_target); };
                        status = replace_path_prefix(*symbolic_target);
                        if (status == STATUS_SUCCESS)
                        {
                            i = 0;
                            Log("Replace Symbolic -> %wZ", path);
                            continue;
                        }
                    }
                }
                ++i;
            }
            FinalPath = path;
            path = NULL;
            return STATUS_UNSUCCESSFUL;
        }
    }
}

#ifdef _TEST

namespace krn
{
    NTSTATUS test_parsepoint()
    {
        UNICODE_STRING path = RTL_CONSTANT_STRING(L"\\??\\C:\\D\\bootmgr.efi");

        UNICODE_STRING* finalized;
        auto status = path::FinalizePath(path, finalized);
        if (status == STATUS_SUCCESS)
        {
            defer {ExFreePool(finalized);};
            Log("Finalized -> %wZ", finalized);
        }

        return STATUS_SUCCESS;
    }
    NTSTATUS test()
    {
        auto status = test_parsepoint();
        if (status == STATUS_SUCCESS)
            Log("[ParsePoint] Success");
        else
            LogError("[ParsePoint] -> %X", status);

        //// Construct unicode
        //{
        //    krn::string::unicode unicode(L"bao");
        //    Log("unicode -> %wZ", &unicode.raw());
        //}
        //// Construct with UNICODE_STRING
        //{
        //    UNICODE_STRING str = { 0 };
        //    krn::string::unicode unicode(str);
        //    Log("unicode + empty UNICODE_STRING -> %wZ", &unicode.raw());
        //}
        //// Append string: valid + valid UNICODE_STRING
        //{
        //    krn::string::unicode unicode(L"bao");
        //    UNICODE_STRING str = RTL_CONSTANT_STRING(L"_test");
        //    unicode += str;
        //    Log("unicode + valid UNICODE_STRING -> %wZ", &unicode.raw());
        //}
        //// Append string: valid + invalid UNICODE_STRING
        //{
        //    krn::string::unicode unicode(L"bao");
        //    UNICODE_STRING str = { 0 };
        //    str.Buffer = reinterpret_cast<WCHAR*>(1);
        //    unicode += str;
        //    Log("unicode + invalid UNICODE_STRING -> %wZ", &unicode.raw());
        //}

        return STATUS_SUCCESS;
    }
}

#endif