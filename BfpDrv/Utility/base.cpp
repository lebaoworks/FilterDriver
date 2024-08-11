#include "base.h"

_When_(return != nullptr, __drv_allocatesMem(Mem))
void* _cdecl operator new(_In_ size_t size)
{ return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, '++C0'); }

_Ret_notnull_
void* _cdecl operator new(_In_ size_t size, _Inout_updates_(size) void* buffer)
{ size; return buffer; }

void _cdecl operator delete(_Pre_notnull_ __drv_freesMem(Mem) void* object)
{ ExFreePool(object); }

void _cdecl operator delete(_Inout_updates_(size) void* object, _In_ size_t size)
{ size; ExFreePool(object); }
