#pragma once
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
        // A valid unicode has _raw.Buffer != NULL
        // _status may not be STATUS_SUCCESS even if _raw.Buffer != NULL
        class unicode : failable
        {
        private:
            
            UNICODE_STRING _raw;

            // Change capability
            //inline void resize(USHORT CbSize)
            //{
            //    if (CbSize <= _raw.MaximumLength)
            //        return;

            //    WCHAR* new_allocation = NULL;
            //    if (_raw.Buffer != NULL && _raw.MaximumLength < CbSize)
            //    {
            //        ExFreePool(_raw.Buffer);
            //        _raw.Buffer = NULL;
            //    }
            //    if (_raw.Buffer == NULL)
            //    {
            //        _raw.Buffer = static_cast<WCHAR*>(ExAllocatePool2(POOL_FLAG_NON_PAGED, CbSize, 'rts0'));;
            //        if (_raw.Buffer == NULL)
            //        {
            //            _status = STATUS_UNSUCCESSFUL;
            //            return;
            //        }
            //        _raw.MaximumLength = CbSize;
            //    }
            //    _status = STATUS_SUCCESS;
            //}
        public:
            //
            // Constructors
            //

            unicode() { failable::_status = STATUS_UNSUCCESSFUL; }
            ~unicode()
            {
                if (_raw.Buffer != NULL)
                    ExFreePool(_raw.Buffer);
            }

            // Take ownership of WideString.
            // WideString must be freeable by ExFreePool().
            unicode(_In_z_ _When_(this->_status == STATUS_SUCCESS, _Post_null_) WCHAR*&& WideString)
            {
                size_t cchLen = wcslen(WideString);
                if (cchLen > MAXUSHORT)
                {
                    _status = STATUS_INVALID_PARAMETER;
                    return;
                }
                _raw.Buffer = WideString;
                _raw.Length =  static_cast<USHORT>(cchLen) * sizeof(WCHAR);
                _raw.MaximumLength = _raw.Length;
                WideString = nullptr;
            }

            // Take ownership of UnicodeString.
            // UnicodeString.Buffer must be freeable by ExFreePool().
            unicode(_In_z_ _When_(this->_status == STATUS_SUCCESS, _Post_invalid_) UNICODE_STRING&& UnicodeString)
            {
                _raw = UnicodeString;
                UnicodeString.Buffer = NULL;
                UnicodeString.Length = UnicodeString.MaximumLength = 0;
            }


            //
            // Assignments
            //

            //inline unicode& operator=(unicode&& other) noexcept
            //{
            //    if (_raw.Buffer != NULL)
            //        ExFreePool(_raw.Buffer);
            //    _raw = other._raw;
            //    _status = other._status;
            //    other._raw.Buffer = NULL;
            //    other._raw.Length = other._raw.MaximumLength = 0;
            //    other._status = STATUS_UNSUCCESSFUL;
            //    return *this;
            //}

            //// this would be invalid if other is invalid
            //inline unicode& operator=(const unicode& other)
            //{
            //    if (other._status != STATUS_SUCCESS)
            //    {
            //        _status = STATUS_INVALID_PARAMETER;
            //        return;
            //    }
            //        
            //    realloc_if_needed(other._raw.Length);
            //    if (_status != STATUS_SUCCESS)
            //        return;
            //    RtlCopyMemory(_raw.Buffer, other._raw.Buffer, other._raw.Length);
            //    _raw.Length = other._raw.Length;
            //    return *this;
            //}

            //
            // Modifiers
            //

            //unicode& operator+=(const unicode& other)
            //{
            //    if (_status != STATUS_SUCCESS)
            //        return;
            //    if (other._status != STATUS_SUCCESS)
            //    {
            //        _status = STATUS_INVALID_PARAMETER;
            //        return;
            //    }

            //    size_t new_size = static_cast<size_t>(_raw.Length) + other._raw.Length;
            //    if (new_size > MAXUSHORT)
            //    {
            //        _status = STATUS_INVALID_PARAMETER;
            //        return;
            //    }

            //    realloc_if_needed(new_size);
            //    if (_status != STATUS_SUCCESS)
            //        return;
            //    RtlCopyMemory(_raw.Buffer, other._raw.Buffer, other._raw.Length);
            //    _raw.Length = other._raw.Length;
            //    return *this;
            //}

            // Observers
            inline UNICODE_STRING& raw() { return _raw; }

            //// Dereferences
            //inline T* operator->() const { return _t; }

            //// Comparisons
            //inline bool operator==(const object<T>& other) const { return *_t == *other._t; }
            //inline bool operator!=(const object<T>& other) const { return *_t != *other._t; }
            //inline bool operator< (const object<T>& other) const { return *_t < *other._t; }
            //inline bool operator> (const object<T>& other) const { return *_t > *other._t; }
            //inline bool operator<=(const object<T>& other) const { return *_t <= *other._t; }
            //inline bool operator>=(const object<T>& other) const { return *_t >= *other._t; }

            //inline bool operator==(const T& other) const { return *_t == other; }
            //inline bool operator!=(const T& other) const { return *_t != other; }
            //inline bool operator< (const T& other) const { return *_t < other; }
            //inline bool operator> (const T& other) const { return *_t > other; }
            //inline bool operator<=(const T& other) const { return *_t <= other; }
            //inline bool operator>=(const T& other) const { return *_t >= other; }

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