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
#include <C++/list.h>
#include <C++/queue.h>
#include <C++/string.h>
#include <C++/map.h>
#include <C++/vector.h>
#include <C++/mutex.h>

/*********************
*      Features      *
*********************/
#include <CustomFeature/defer.h>
#include <CustomFeature/object.h>
#include <CustomFeature/tag.h>

class eresource_lock
{
private:
    ERESOURCE _resource;
public:
    eresource_lock();
    ~eresource_lock();

    _Acquires_lock_(_Global_critical_region_)
        _Acquires_exclusive_lock_(_resource)
        _IRQL_requires_max_(DISPATCH_LEVEL)
        void lock();

    _Requires_lock_held_(_Global_critical_region_)
        _Releases_lock_(_Global_critical_region_)
        _Requires_lock_held_(_resource)
        _Releases_lock_(_resource)
        _IRQL_requires_max_(DISPATCH_LEVEL)
        void unlock();
};

class unicode_string : public failable_object<bool>
{
private:
    UNICODE_STRING _raw;
public:
    unicode_string(size_t size);
    ~unicode_string();

    UNICODE_STRING& raw();
};

/*********************
*       Logging      *
*********************/

namespace shared {
    template<typename... T>
    void log(_In_z_ const char* format, T&&... args)
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, format, args...);
    }
}

#define Log(format, ...) shared::log(__FUNCTION__"() "##format"\n", __VA_ARGS__)
