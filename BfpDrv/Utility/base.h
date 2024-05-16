#pragma once
#include <sal.h>
#include <wdm.h>
#include <ntstrsafe.h>

/*********************
*        Base        *
*********************/

__drv_allocatesMem(object)
_Check_return_
_Ret_maybenull_
_Post_writable_byte_size_(size)
void* _cdecl operator new(_In_ size_t size);

_Ret_notnull_
void* _cdecl operator new(_In_ size_t size, _Inout_updates_(size) void* buffer);

void _cdecl operator delete(_Pre_notnull_ __drv_freesMem(object) void* object);

void _cdecl operator delete(_Inout_updates_(size) void* object, _In_ size_t size);

namespace std
{
    // Convert object to rvalue
    template<typename T>
    inline T&& move(T& obj) noexcept { return static_cast<T&&>(obj); }

    // Convert object to rvalue
    template<typename T>
    inline T&& move(T&& obj) noexcept { return static_cast<T&&>(obj); }

    // Swap objects' value
    template<typename T>
    inline void swap(T& a, T& b) noexcept { auto x = move(a); a = move(b); b = move(x); }
}

#include <base/defer.h>
#include <base/object.h>
#include <base/tag.h>


/*********************
*      Logging       *
*********************/

namespace logging
{
    template<typename... T>
    void log_caller(
        _In_z_ const char* caller,
        _In_z_ const char* format,
        _In_z_ T&... args)
    {
        CHAR* buffer = reinterpret_cast<CHAR*>(ExAllocatePool2(POOL_FLAG_NON_PAGED, 512, 'gol0'));
        if (buffer == NULL)
            return;
        defer { ExFreePool(buffer); };

        if (RtlStringCchCopyA(buffer, 512, "[%4d:%2d:%2d-%2d:%2d:%2d] ") != STATUS_SUCCESS)
            return;
        if (RtlStringCchCatA(buffer, 512, caller) != STATUS_SUCCESS)
            return;
        if (RtlStringCchCatA(buffer, 512, format) != STATUS_SUCCESS)
            return;

        LARGE_INTEGER time_stamp;
        KeQuerySystemTime(&time_stamp);
        TIME_FIELDS time;
        RtlTimeToTimeFields(&time_stamp, &time);

        DbgPrintEx(
            DPFLTR_IHVDRIVER_ID,
            0,
            buffer,
            time.Year, time.Month, time.Day,
            time.Hour, time.Minute, time.Second,
            args...);
    }

}

#define Log(format, ...) logging::log_caller(__FUNCTION__"() ", format"\n", __VA_ARGS__)

//#include <C++/memory.h>
//#include <C++/list.h>
//#include <C++/queue.h>
//#include <C++/string.h>
//#include <C++/map.h>
//#include <C++/vector.h>
//#include <C++/mutex.h>

/*********************
*      Features      *
*********************/

//class eresource_lock
//{
//private:
//    ERESOURCE _resource;
//public:
//    eresource_lock();
//    ~eresource_lock();
//
//    _Acquires_lock_(_Global_critical_region_)
//        _Acquires_exclusive_lock_(_resource)
//        _IRQL_requires_max_(DISPATCH_LEVEL)
//        void lock();
//
//    _Requires_lock_held_(_Global_critical_region_)
//        _Releases_lock_(_Global_critical_region_)
//        _Requires_lock_held_(_resource)
//        _Releases_lock_(_resource)
//        _IRQL_requires_max_(DISPATCH_LEVEL)
//        void unlock();
//};
//
//class unicode_string : public failable_object<bool>
//{
//private:
//    UNICODE_STRING _raw;
//public:
//    unicode_string(size_t size);
//    ~unicode_string();
//
//    UNICODE_STRING& raw();
//};

