//#pragma once
//
//#include <Shared.h>
//
//namespace Win32
//{
//    namespace Registry
//    {
//        NTSTATUS OpenKey(_In_ UNICODE_STRING* KeyPath, _In_ ACCESS_MASK DesiredAccess, _Out_ HANDLE* Handle);
//        NTSTATUS GetValue(
//            _In_ UNICODE_STRING* KeyPath,
//            _In_ UNICODE_STRING* ValueName,
//            _Out_writes_(DataSize) PVOID Data,
//            _In_ ULONG DataSize,
//            _Out_ ULONG* ResultSize);
//
//        NTSTATUS SetValue(
//            _In_ UNICODE_STRING* KeyPath,
//            _In_ UNICODE_STRING* ValueName,
//            _In_ ULONG ValueType,
//            _In_reads_bytes_(DataSize) PVOID Data,
//            _In_ ULONG DataSize);
//    }
//
//    namespace Path
//    {
//        NTSTATUS QueryTarget(_In_ UNICODE_STRING* SymbolicPath, _Out_ UNICODE_STRING* TargetPath);
//        NTSTATUS QueryAbsoluteTarget(_In_ UNICODE_STRING* SymbolicPath, _Out_ UNICODE_STRING* TargetPath);
//    }
//}