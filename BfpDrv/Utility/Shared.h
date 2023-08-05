#pragma once
#include <sal.h>

/*********************
*       Logging      *
*********************/

namespace shared {
    void log(_In_z_ char* szString);

    template<typename... T>
    void log(_In_z_ char* format, T... args)
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, format, args...);
    }
}

#define Log(format, ...) shared::log(__FUNCTION__"() "##format"\n", __VA_ARGS__)


/*********************
*         C++        *
*********************/
void* _cdecl operator new(_In_ size_t size);
void* _cdecl operator new[](_In_ size_t size);
void* _cdecl operator new(_In_ size_t size, _In_ void* ptr);
void* _cdecl operator new[](_In_ size_t size, _In_ void* ptr);
void _cdecl operator delete(_In_ void* object);
void _cdecl operator delete[](_In_ void* object);
void _cdecl operator delete(_In_ void* object, _In_ size_t size);
void _cdecl operator delete[](_In_ void* object, _In_ size_t size);

#include <C++/memory.h>
#include <C++/list.h>
#include <C++/queue.h>
#include <C++/string.h>
#include <C++/map.h>
#include <C++/vector.h>

/*********************
*      Features      *
*********************/
#include <CustomFeature/defer.h>
#include <CustomFeature/object.h>
#include <CustomFeature/tag.h>
