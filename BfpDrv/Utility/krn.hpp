#pragma once
#include <sal.h>
#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>

#include <std.hpp>


namespace krn
{
    template<ULONG TAG>
    struct tag
    {
        void* _cdecl operator new(_In_ size_t size) { return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, TAG); }
        void* _cdecl operator new(_In_ size_t size, _In_ void* ptr) { size; return ptr; }

        void _cdecl operator delete(_In_ void* object) { ExFreePool(object); }
        void _cdecl operator delete(_In_ void* object, _In_ size_t size) { size; ExFreePool(object); }
    };

    struct failable
    {
    protected:
        NTSTATUS _status = STATUS_SUCCESS;
    public:
        virtual NTSTATUS status() const noexcept { return _status; };
    };

}

namespace krn
{
    template<typename T>
    struct expected;

    struct unexpected
    {
        template<typename T>
        friend struct expected;
    private:
        NTSTATUS _status = STATUS_SUCCESS;
    public:
        explicit unexpected(const NTSTATUS& status) : _status(status) {}

        template<typename T>
        operator expected<T> () const { return expected<T>(*this); }
    };

    template<typename T>
    struct expected
    {
    private:
        NTSTATUS _status = STATUS_SUCCESS;
        CHAR _value_data[sizeof(T)];
        bool _has_value = true;
    
    public:
        expected(const T& value) { new (reinterpret_cast<T*>(&_value_data[0])) T(value); }
        expected(T&& value) { new (reinterpret_cast<T*>(&_value_data[0])) T(std::move(value)); }
        explicit expected(const unexpected& error) : _status(error._status) { _has_value = false; }
        explicit expected(unexpected&& error) : _status(error._status) { _has_value = false; }
        ~expected() { if (_has_value) reinterpret_cast<T*>(&_value_data[0])->~T(); }

        NTSTATUS status() const noexcept { return _status; }
        T& value() { return *reinterpret_cast<T*>(&_value_data[0]); };
    };
}

/*********************
*       Utility      *
*********************/

//namespace base
//{
//    template<typename T>
//    class result
//    {
//        static_assert(base::is_array<T>() == false);
//    private:
//        T* val = nullptr;
//        NTSTATUS err = STATUS_SUCCESS;
//    
//    public:
//        result(T* v) : val(v) {}
//        result(NTSTATUS e) : err(e) {}
//        result(const result&) = delete;
//        ~result() { if (val != nullptr) delete val; }
//
//        NTSTATUS error() const noexcept { return err; }
//        T* release() { auto ret = val; val = nullptr; return ret; }
//    };
//
//    template<typename T, typename... Args>
//    result<T> make(const Args&... args)
//    {
//        static_assert(base::is_array<T>() == false);
//
//        auto val = new T(args...);
//        if (val == nullptr)
//            return STATUS_NO_MEMORY;
//        if (base::is_base_of<failable, T>() == false)
//            return val;
//        
//        auto err = reinterpret_cast<failable*>(val)->status();
//        if (err == STATUS_SUCCESS)
//            return val;
//
//        delete val;
//        return err;
//    }
//
//    template<typename T, typename... Args>
//    result<T> make(Args&&... args)
//    {
//        static_assert(base::is_array<T>() == false);
//
//        auto val = new T(std::move(args)...);
//        if (val == nullptr)
//            return STATUS_NO_MEMORY;
//        if (base::is_base_of<failable, T>() == false)
//            return val;
//
//        auto err = reinterpret_cast<failable*>(val)->status();
//        if (err == STATUS_SUCCESS)
//            return val;
//
//        delete val;
//        return err;
//    }
//}

/*********************
*      Logging       *
*********************/

namespace logging
{
    template<typename... T>
    _IRQL_requires_(PASSIVE_LEVEL)
    void log_passive(
        _In_z_ const char* level,
        _In_z_ const char* caller,
        _In_z_ const char* format,
        _In_ T&&... args)
    {
        CHAR* buffer = reinterpret_cast<CHAR*>(ExAllocatePool2(POOL_FLAG_PAGED, 512, 'gol0'));
        if (buffer == NULL)
            return;
        defer { ExFreePool(buffer); };

        if (RtlStringCchCopyA(buffer, 512, "[%04d:%02d:%02d-%02d:%02d:%02d][PID: %5d][TID: %5d][%-7s] ") != STATUS_SUCCESS)
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
            PsGetProcessId(PsGetCurrentProcess()), PsGetThreadId(PsGetCurrentThread()),
            level,
            args...);
    }

}

#define Log(format, ...)        logging::log_passive("INFO",    __FUNCTION__"() ", format"\n", __VA_ARGS__)
#define LogDebug(format, ...)   logging::log_passive("DEBUG",   __FUNCTION__"() ", format"\n", __VA_ARGS__)
#define LogWarning(format, ...) logging::log_passive("WARNING", __FUNCTION__"() ", format"\n", __VA_ARGS__)
#define LogError(format, ...)   logging::log_passive("ERROR",   __FUNCTION__"() ", format"\n", __VA_ARGS__)


/*********************
*        TEST        *
*********************/

#if _TEST
namespace krn
{
    void test();
}
#endif