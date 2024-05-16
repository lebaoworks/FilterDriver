#pragma once

#include <ntdef.h>

template<ULONG TAG>
struct tag
{
    void* _cdecl operator new(_In_ size_t size) { return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, TAG); }
    void* _cdecl operator new[](_In_ size_t size) { return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, TAG); }
    void* _cdecl operator new(_In_ size_t size, _In_ void* ptr) { size; return ptr; }
    void* _cdecl operator new[](_In_ size_t size, _In_ void* ptr) { size; return ptr; }

    void _cdecl operator delete(_In_ void* object) { ExFreePool(object); }
    void _cdecl operator delete[](_In_ void* object) { ExFreePool(object); }
    void _cdecl operator delete(_In_ void* object, _In_ size_t size) { size; ExFreePool(object); }
    void _cdecl operator delete[](_In_ void* object, _In_ size_t size) { size; ExFreePool(object); }
};