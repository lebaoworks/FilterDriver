#include "base.h"

__drv_allocatesMem(Mem)
_Post_writable_byte_size_(size)
_Check_return_
_Ret_maybenull_
void* _cdecl operator new(_In_ size_t size) { return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, '++C0'); }

_Ret_notnull_
void* _cdecl operator new(_In_ size_t size, _Inout_updates_(size) void* buffer) { size; return buffer; }

void _cdecl operator delete(_Pre_notnull_ __drv_freesMem(Mem) void* object) { ExFreePool(object); }

void _cdecl operator delete(_Inout_updates_(size) void* object, _In_ size_t size) { size; ExFreePool(object); }


