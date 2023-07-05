#pragma once
#include <sal.h>


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
#include <C++/queue.h>


/*********************
*       Logging      *
*********************/

namespace shared {
    void log(_In_ unsigned int level, _In_z_ char* szString);

    template<typename... T>
    void log(_In_ unsigned int level, _In_z_ char* format, T... args)
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, level, format, args...);
    }
}

#define Log(format, ...) shared::log(DPFLTR_ERROR_LEVEL, __FUNCTION__"() "##format"\n", __VA_ARGS__)


/*********************
*        Others      *
*********************/

// Source: https://stackoverflow.com/a/42060129
// Modified for rvalue
struct defer_dummy {};
template<class F>
struct deferer
{
    F _f;
    deferer(F&& f) : _f(f) {}
    ~deferer() { _f(); }
};
template<class F>
inline deferer<F> operator*(defer_dummy, F&& f)
{
    return deferer<F>(std::move(f));
}
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()