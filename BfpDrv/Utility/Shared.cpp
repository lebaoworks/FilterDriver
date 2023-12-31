#include "Shared.h"
#include <wdm.h>

/*********************
*         C++        *
*********************/

void* _cdecl operator new(_In_ size_t size) { return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, '++C0'); }
void* _cdecl operator new[](_In_ size_t size) { return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, '++C0'); }
void* _cdecl operator new(_In_ size_t size, _In_ void* ptr) { size; return ptr; }
void* _cdecl operator new[](_In_ size_t size, _In_ void* ptr) { size; return ptr; }

void _cdecl operator delete(_In_ void* object) { ExFreePool(object); }
void _cdecl operator delete[](_In_ void* object) { ExFreePool(object); }
void _cdecl operator delete(_In_ void* object, _In_ size_t size) { size; ExFreePool(object); }
void _cdecl operator delete[](_In_ void* object, _In_ size_t size) { size; ExFreePool(object); }

/*********************
*      Features      *
*********************/

eresource_lock::eresource_lock() { ExInitializeResourceLite(&_resource); }
eresource_lock::~eresource_lock() { ExDeleteResourceLite(&_resource); }

_Acquires_lock_(_Global_critical_region_)
_Acquires_exclusive_lock_(_resource)
_IRQL_requires_max_(DISPATCH_LEVEL)
void eresource_lock::lock()
{
    KeEnterCriticalRegion();
    ExAcquireResourceExclusiveLite(&_resource, 1);
}

_Requires_lock_held_(_Global_critical_region_)
_Releases_lock_(_Global_critical_region_)
_Requires_lock_held_(_resource)
_Releases_lock_(_resource)
_IRQL_requires_max_(DISPATCH_LEVEL)
void eresource_lock::unlock()
{
    ExReleaseResourceLite(&_resource);
    KeLeaveCriticalRegion();
}

unicode_string::unicode_string(size_t size)
{
    _raw.Length = 0;
    _raw.MaximumLength = (USHORT)size;
    _raw.Buffer = new WCHAR[size/2+1];
    if (_raw.Buffer == nullptr)
        failable_object::_error = true;
}
unicode_string::~unicode_string()
{
    if (!error())
        delete[]_raw.Buffer;
}
UNICODE_STRING& unicode_string::raw()
{
    return _raw;
}