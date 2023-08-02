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
    void log(_In_z_ char* szString);

    template<typename... T>
    void log(_In_z_ char* format, T... args)
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, format, args...);
    }
}

#define Log(format, ...) shared::log(__FUNCTION__"() "##format"\n", __VA_ARGS__)


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


#include <ntdef.h>
#include <ntstatus.h>
template <typename T>
struct failable_object
{
protected: T _error;
public:    T error() const noexcept = 0;
};

template<>
struct failable_object<bool>
{
protected: bool _error = false;
public:    bool error() const noexcept { return _error; };
};

template<>
struct failable_object<NTSTATUS>
{
protected: NTSTATUS _error = STATUS_SUCCESS;
public:    NTSTATUS error() const noexcept { return _error; };
};