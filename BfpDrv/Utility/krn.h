#pragma once
#include <ntifs.h>
#include <mountmgr.h>

#include <base.h>

namespace krn
{
    namespace lock
    {
        class eresource
        {
        private:
            ERESOURCE _resource;
        public:
            inline eresource() { ExInitializeResourceLite(&_resource); }
            inline ~eresource() { ExDeleteResourceLite(&_resource); }

            _Acquires_lock_(_Global_critical_region_)
            _Acquires_exclusive_lock_(_resource)
            _IRQL_requires_max_(DISPATCH_LEVEL)
            inline void lock()
            {
                KeEnterCriticalRegion();
                ExAcquireResourceExclusiveLite(&_resource, TRUE);
            }

            _Requires_lock_held_(_Global_critical_region_)
            _Releases_lock_(_Global_critical_region_)
            _Requires_lock_held_(_resource)
            _Releases_lock_(_resource)
            _IRQL_requires_max_(DISPATCH_LEVEL)
            inline void unlock()
            {
                ExReleaseResourceLite(&_resource);
                KeLeaveCriticalRegion();
            }
        };
    }
}

namespace krn
{
    namespace string
    {
        class unicode : failable
        {
        private:
            UNICODE_STRING _raw = { 0 };

            inline WCHAR* alloc(USHORT CbSize) { return static_cast<WCHAR*>(ExAllocatePool2(POOL_FLAG_NON_PAGED, CbSize, 'rts0')); }
            inline void free(void* address) { ExFreePool(address); }
        public:
            virtual NTSTATUS status() const noexcept override
            {
                return _raw.Buffer == NULL ? STATUS_UNSUCCESSFUL : STATUS_SUCCESS;
            }

            //
            // Enums
            //

            enum { npos = MAXSIZE_T };

            //
            // Constructors
            //

            unicode(USHORT CbMaximumLength)
            {
                if (CbMaximumLength < 2)
                _raw.Buffer = alloc(CbMaximumLength);
                if (_raw.Buffer == NULL)
                    return;
                _raw.Length = 0;
                _raw.MaximumLength = CbMaximumLength;
            }
            unicode(_In_z_ const WCHAR* WideString)
            {
                size_t cchLen = wcslen(WideString);
                if (cchLen * sizeof(WCHAR) > MAXUSHORT || cchLen == 0)
                    return;
                USHORT cbLen = static_cast<USHORT>(cchLen) * sizeof(WCHAR);
                _raw.Buffer = alloc(cbLen);
                if (_raw.Buffer == NULL)
                    return;
                RtlCopyMemory(_raw.Buffer, WideString, cbLen);
                _raw.Length = _raw.MaximumLength = cbLen;
            }
            unicode(_In_ const UNICODE_STRING& UnicodeString)
            {
                if (UnicodeString.Buffer == NULL || UnicodeString.MaximumLength == 0)
                    return;
                _raw.Buffer = alloc(UnicodeString.MaximumLength);
                if (_raw.Buffer == NULL)
                    return;
                RtlCopyMemory(_raw.Buffer, UnicodeString.Buffer, UnicodeString.Length);
                _raw.Length = UnicodeString.Length;
                _raw.MaximumLength = UnicodeString.MaximumLength;
            }
            ~unicode()
            {
                if (_raw.Buffer != NULL)
                    free(_raw.Buffer);
            }

            //
            // Assignments
            //

            inline unicode& operator=(unicode&& other) noexcept
            {
                if (_raw.Buffer != NULL)
                    free(_raw.Buffer);
                _raw = other._raw;
                other._raw = { 0 };
                return *this;
            }
            inline unicode& operator=(const unicode& other) noexcept
            {
                // Validify
                if (other._raw.Buffer == NULL)
                {
                    _raw = { 0 };
                    return *this;
                }

                // Realloc
                if (_raw.Buffer != NULL && _raw.MaximumLength < other._raw.Length)
                {
                    free(_raw.Buffer);
                    _raw.Buffer = NULL;
                }
                if (_raw.Buffer == NULL)
                {
                    _raw.Buffer = alloc(other._raw.Length);
                    if (_raw.Buffer == NULL)
                        return *this;
                }
                
                // Copy value
                RtlCopyMemory(_raw.Buffer, other._raw.Buffer, other._raw.Length);
                _raw.Length = _raw.MaximumLength = other._raw.Length;
                return *this;
            }

            //
            // Modifiers
            //

            inline unicode& operator+=(const unicode& other)
            {
                *this += other._raw;
                return *this;
            }
            inline unicode& operator+=(const UNICODE_STRING& other)
            {
                // Validify inputs
                if (_raw.Buffer == NULL)
                    return *this;
                if (other.Buffer == NULL)
                {
                    _raw = { 0 };
                    return *this;
                }

                // Validify output
                USHORT cbLen1 = _raw.Length - (_raw.Length & 1);
                USHORT cbLen2 = other.Length - (other.Length & 1);
                if (static_cast<size_t>(cbLen1) + cbLen2 > MAXUSHORT)
                {
                    _raw = { 0 };
                    return *this;
                }
                USHORT cbNewSize = cbLen1 + cbLen2;

                // No need to realloc
                if (_raw.Buffer != NULL && _raw.MaximumLength >= cbNewSize)
                {
                    RtlCopyMemory(reinterpret_cast<char*>(_raw.Buffer) + cbLen1, other.Buffer, cbLen2);
                    _raw.Length = cbNewSize;
                    return *this;
                }
                auto buffer = alloc(cbNewSize);
                if (buffer == NULL)
                {
                    _raw = { 0 };
                    return *this;
                }
                RtlCopyMemory(buffer, _raw.Buffer, cbLen1);
                RtlCopyMemory(reinterpret_cast<char*>(buffer) + cbLen1, other.Buffer, cbLen2);
                free(_raw.Buffer);
                _raw.Buffer = buffer;
                _raw.Length = _raw.MaximumLength = cbNewSize;
                return *this;
            }

            // Observers
            inline const UNICODE_STRING& raw() { return _raw; }

            // Comparisons
            inline bool operator==(const unicode& other) const { return RtlEqualUnicodeString(&_raw, &other._raw, FALSE) == TRUE; }
            inline bool operator!=(const unicode& other) const { return RtlEqualUnicodeString(&_raw, &other._raw, FALSE) == FALSE; }
            inline bool operator< (const unicode& other) const { return RtlCompareUnicodeString(&_raw, &other._raw, FALSE) < 0; }
            inline bool operator> (const unicode& other) const { return RtlCompareUnicodeString(&_raw, &other._raw, FALSE) > 0; }
            inline bool operator<=(const unicode& other) const { return RtlCompareUnicodeString(&_raw, &other._raw, FALSE) <= 0; }
            inline bool operator>=(const unicode& other) const { return RtlCompareUnicodeString(&_raw, &other._raw, FALSE) >= 0; }
        };

        class ansi : failable
        {

        };
    }
}

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


namespace krn
{
    namespace path
    {
        NTSTATUS FinalizePath(
            _In_ UNICODE_STRING& NtPath,
            _When_(return == STATUS_SUCCESS, __drv_allocatesMem(Mem) _Outptr_) PUNICODE_STRING& FinalPath);
    }
}

#ifdef _TEST

namespace krn
{
    NTSTATUS test();
}

#endif