#pragma once
#include <sal.h>
#include <ntddk.h>
#include <wdm.h>
#include <ntstrsafe.h>

/*********************
*       Memory       *
*********************/

struct nothrow_t{};
constexpr nothrow_t nothrow;

__drv_allocatesMem(Mem)
_Post_writable_byte_size_(size)
_Check_return_
_Ret_maybenull_
void* _cdecl operator new(_In_ size_t size);

_Ret_notnull_
void* _cdecl operator new(_In_ size_t size, _Inout_updates_(size) void* buffer);

void _cdecl operator delete(_Pre_notnull_ __drv_freesMem(Mem) void* object);

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
    inline void swap(T& a, T& b) noexcept { auto x = std::move(a); a = std::move(b); b = std::move(x); }
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
#define defer auto defer_at_##__LINE__ = base::defer_dummy{} *[&]()

/*********************
*     Base Types     *
*********************/

// Type traits

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

// Type holder

template<typename T>
struct object
{
    // Do not support array
    static_assert(base::is_array<T>() == false);

private:
    __drv_aliasesMem T* _t = nullptr;
    NTSTATUS _status = STATUS_UNSUCCESSFUL;

    static inline NTSTATUS _get_status(failable& obj) noexcept { return obj.status(); }
    static inline NTSTATUS _get_status(...) noexcept { return STATUS_SUCCESS; }
public:
    // Constructors
    object() noexcept {}
    object(T*&& t) noexcept : _t(t) { t = nullptr;}
    object(const object<T>& obj) = delete;
    object(object<T>&& obj) noexcept : _t(obj._t), _status(obj._status) { obj._t = nullptr; obj._status = STATUS_UNSUCCESSFUL; }
    ~object() { if (_t != nullptr) delete _t; }

    template<typename... Args>
    object(const Args&... args)
    {
        T* t = new T(args...);
        if (t == nullptr)
        {
            _status = STATUS_NO_MEMORY;
            return;
        }
        _status = _get_status(*t);
        if (_status != STATUS_SUCCESS)
        {
            delete t;
            return;
        }
        _t = t;
    }

    // Assignments
    inline object<T>& operator=(object<T>&& obj) noexcept
    {
        if (_t != nullptr)
            delete _t;
        _t = obj._t;
        _status = obj._status;
        obj._t = nullptr;
        obj._status = STATUS_UNSUCCESSFUL;
        return *this;
    }
    inline object<T>& operator=(const object<T>& obj) = delete;

    // Modifiers
    inline T* release() noexcept { auto t = _t; _t = nullptr; return t; }
    inline void reset(T* p) noexcept { if (_t != nullptr) delete _t; _t = p; }
    inline void swap(object<T>& other) noexcept { std::swap(_t, other._t); std::swap(_status, other._status); }

    // Observers
    inline T& get() const noexcept { return *_t; }
    inline NTSTATUS status() { return _status; };

    // Dereferences
    inline T* operator->() const { return _t; }

    // Comparisons
    inline bool operator==(const object<T>& other) const { return *_t == *other._t; }
    inline bool operator!=(const object<T>& other) const { return *_t != *other._t; }
    inline bool operator< (const object<T>& other) const { return *_t <  *other._t; }
    inline bool operator> (const object<T>& other) const { return *_t >  *other._t; }
    inline bool operator<=(const object<T>& other) const { return *_t <= *other._t; }
    inline bool operator>=(const object<T>& other) const { return *_t >= *other._t; }

    inline bool operator==(const T& other) const { return *_t == other; }
    inline bool operator!=(const T& other) const { return *_t != other; }
    inline bool operator< (const T& other) const { return *_t <  other; }
    inline bool operator> (const T& other) const { return *_t >  other; }
    inline bool operator<=(const T& other) const { return *_t <= other; }
    inline bool operator>=(const T& other) const { return *_t >= other; }
};


/*********************
*      Logging       *
*********************/

namespace logging
{
    template<typename... T>
    void log_caller(
        _In_z_ const char* level,
        _In_z_ const char* caller,
        _In_z_ const char* format,
        _In_z_ T&&... args)
    {
        CHAR* buffer = reinterpret_cast<CHAR*>(ExAllocatePool2(POOL_FLAG_NON_PAGED, 512, 'gol0'));
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

#define Log(format, ...)        logging::log_caller("INFO",    __FUNCTION__"() ", format"\n", __VA_ARGS__)
#define LogDebug(format, ...)   logging::log_caller("DEBUG",   __FUNCTION__"() ", format"\n", __VA_ARGS__)
#define LogWarning(format, ...) logging::log_caller("WARNING", __FUNCTION__"() ", format"\n", __VA_ARGS__)
#define LogError(format, ...)   logging::log_caller("ERROR",   __FUNCTION__"() ", format"\n", __VA_ARGS__)
