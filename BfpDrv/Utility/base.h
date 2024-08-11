#pragma once
#include <sal.h>
#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>

/*********************
*      Allocator     *
*********************/

_When_(return != nullptr, __drv_allocatesMem(Mem))
void* _cdecl operator new(_In_ size_t size);

_Ret_notnull_
void* _cdecl operator new(_In_ size_t size, _Inout_updates_(size) void* buffer);

void _cdecl operator delete(_Notnull_ __drv_freesMem(Mem) void* object);

void _cdecl operator delete(_Inout_updates_(size) void* object, _In_ size_t size);


/*********************
*       Memory       *
*********************/

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
    inline void swap(T& a, T& b) noexcept { auto x = std::move(a); a = std::move(b); b = std::move(x); }

    template<typename T>
    class unique_ptr
    {
    private:
        T* _ptr = nullptr;
    public:
        // Constructors
        unique_ptr() noexcept {}
        unique_ptr(T* p) noexcept : _ptr(p) {}
        unique_ptr(const unique_ptr<T>& uptr) = delete;
        unique_ptr(unique_ptr<T>&& uptr) noexcept : _ptr(uptr._ptr) { uptr._ptr = nullptr; }
        ~unique_ptr() { if (_ptr != nullptr) delete _ptr; }

        // Assignments
        inline unique_ptr<T>& operator=(unique_ptr<T>&& uptr) noexcept { std::swap(_ptr, uptr._ptr); return *this; }
        unique_ptr<T>& operator=(const unique_ptr<T>& uptr) = delete;

        // Modifiers
        inline T* release() { auto p = _ptr; _ptr = nullptr; return p; }
        inline void reset(T* p) { if (_ptr != nullptr) delete _ptr; _ptr = p; }
        inline void swap(unique_ptr<T>& other) { swap(_ptr, other._ptr); }

        // Observers
        inline T* get() const { return _ptr; }
        inline operator bool() const { return _ptr != nullptr; }

        // Dereference
        inline T& operator*() const { return *_ptr; }
        inline T* operator->() const { return _ptr; }

        // Comparisons
        inline bool operator==(const unique_ptr& other) const { return _ptr == other._ptr; }
        inline bool operator!=(const unique_ptr& other) const { return _ptr != other._ptr; }
        inline bool operator<(const unique_ptr& other) const { return _ptr < other._ptr; }
        inline bool operator>(const unique_ptr& other) const { return _ptr > other._ptr; }
        inline bool operator<=(const unique_ptr& other) const { return _ptr <= other._ptr; }
        inline bool operator>=(const unique_ptr& other) const { return _ptr >= other._ptr; }
    };
}


/*********************
*        Types       *
*********************/

struct failable
{
protected:
    NTSTATUS _status = STATUS_SUCCESS;
public:
    virtual NTSTATUS status() const noexcept { return _status; };
};

// Traits
namespace base
{
    template<typename Base, typename Derived>
    class is_base_of
    {
    private:
        constexpr static bool Test(Base*) noexcept { return true; };
        constexpr static bool Test(...) noexcept { return false; };
    public:
        constexpr operator bool() const noexcept { return Test(static_cast<Derived*>(nullptr)); }
    };

    template<typename T>
    struct is_array { constexpr operator bool() const noexcept { return false; } };

    template<typename T>
    struct is_array<T[]> { constexpr operator bool() const noexcept { return true; } };

    template<typename T, size_t N>
    struct is_array<T[N]> { constexpr operator bool() const noexcept { return true; } };

}

// Tags
namespace base
{
    template<ULONG TAG>
    struct tag
    {
        void* _cdecl operator new(_In_ size_t size) { return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, TAG); }
        void* _cdecl operator new(_In_ size_t size, _In_ void* ptr) { size; return ptr; }

        void _cdecl operator delete(_In_ void* object) { ExFreePool(object); }
        void _cdecl operator delete(_In_ void* object, _In_ size_t size) { size; ExFreePool(object); }
    };
}


/*********************
*       Utility      *
*********************/

namespace base
{
    template<typename T>
    class result
    {
        static_assert(base::is_array<T>() == false);
    private:
        T* val = nullptr;
        NTSTATUS err = STATUS_SUCCESS;
    
    public:
        result(T* v) : val(v) {}
        result(NTSTATUS e) : err(e) {}
        result(const result&) = delete;
        ~result() { if (val != nullptr) delete val; }

        NTSTATUS error() const noexcept { return err; }
        T* release() { auto ret = val; val = nullptr; return ret; }
    };

    template<typename T, typename... Args>
    result<T> make(const Args&... args)
    {
        static_assert(base::is_array<T>() == false);

        auto val = new T(args...);
        if (val == nullptr)
            return STATUS_NO_MEMORY;
        if (base::is_base_of<failable, T>() == false)
            return val;
        
        auto err = reinterpret_cast<failable*>(val)->status();
        if (err == STATUS_SUCCESS)
            return val;

        delete val;
        return err;
    }

    template<typename T, typename... Args>
    result<T> make(Args&&... args)
    {
        static_assert(base::is_array<T>() == false);

        auto val = new T(std::move(args)...);
        if (val == nullptr)
            return STATUS_NO_MEMORY;
        if (base::is_base_of<failable, T>() == false)
            return val;

        auto err = reinterpret_cast<failable*>(val)->status();
        if (err == STATUS_SUCCESS)
            return val;

        delete val;
        return err;
    }
}


/*********************
*       Defer        *
*********************/

namespace base
{
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
}
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = base::defer_dummy{} *[&]()


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

#define ASSERT_EQ(x, y, msg) { if (x != y) { Log(msg); return STATUS_UNSUCCESSFUL; } }
namespace base
{
    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS test();
}

#endif