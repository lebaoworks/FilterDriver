#pragma once

// Windows Headers
#include <sal.h>
#include <ntddk.h>
#include <wdm.h>

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
*       Traits       *
*********************/

namespace std
{
    struct true_type
    {
        static constexpr bool value = true;
        using value_type = bool;
        using type = true_type;
        constexpr operator value_type() const noexcept { return value; }
    };

    struct false_type
    {
        static constexpr bool value = false;
        using value_type = bool;
        using type = false_type;
        constexpr operator value_type() const noexcept { return value; }
    };

    template <typename T, T v>
    struct integral_constant
    {
        static constexpr T value = v; // Static member to hold the value

        using value_type = T; // Type alias for the value type
        using type = integral_constant; // Type alias for the type itself

        constexpr operator value_type() const noexcept { return value; }

        constexpr value_type operator()() const noexcept { return value; }
    };


    template <typename T, typename U>
    struct is_same : false_type {};

    template <typename T>
    struct is_same<T, T> : true_type {};

    template<typename T>
    struct is_lvalue_reference : false_type {};

    template<typename T>
    struct is_lvalue_reference<T&> : true_type {};

    template <typename T>
    struct is_class {
    private:
        // Helper function to detect class type
        template <typename U>
        static char test(int U::*);

        // Fallback function
        template <typename U>
        static int test(...);

    public:
        // If the first overload is chosen, T is a class
        static constexpr bool value = sizeof(test<T>(0)) == sizeof(char);
    };

    template<typename Base, typename Derived>
    class is_base_of
    {
    private:
        static constexpr bool Test(Base*) noexcept { return true; };
        static constexpr bool Test(...) noexcept { return false; };
    public:
        static constexpr bool value = Test(static_cast<Derived*>(nullptr));
    };



    template<typename T>
    struct is_array : false_type {};

    template<typename T>
    struct is_array<T[]> : true_type {};

    template<typename T, size_t N>
    struct is_array<T[N]> : true_type {};
}


/*********************
*       Utility      *
*********************/

namespace std
{
    // Handle lvalue
    template<typename T>
    inline T&& move(T& obj) noexcept { return static_cast<T&&>(obj); }

    // Handle rvalue
    template<typename T>
    inline T&& move(T&& obj) noexcept { return static_cast<T&&>(obj); }


    template<typename T>
    struct remove_reference { typedef T type; };

    template<typename T>
    struct remove_reference<T&> { typedef T type; };

    template<typename T>
    struct remove_reference<T&&> { typedef T type; };

    template<typename T>
    inline T&& forward(typename remove_reference<T>::type& t) noexcept { return static_cast<T&&>(t); }

    template<typename T>
    inline T&& forward(typename remove_reference<T>::type&& t) noexcept
    {
        static_assert(is_lvalue_reference<T>::value == false, "bad forward call");
        return static_cast<T&&>(t);
    }

    template<typename T>
    inline void swap(T& a, T& b) noexcept { auto x = move(a); a = move(b); b = move(x); }
}


/*********************
*       Memory       *
*********************/

namespace std
{
    template <typename T>
    struct default_delete
    {
        void operator()(T* ptr) const { delete ptr; }
    };

    template<typename T, typename Deleter = default_delete<T>>
    class unique_ptr
    {
        static_assert(is_array<T>::value == false, "unique_ptr does not support arrays");
    private:
        T* _ptr = nullptr;
        Deleter _deleter;
    public:
        // Constructors
        explicit unique_ptr(T* p = nullptr, Deleter d = Deleter()) noexcept : _ptr(p), _deleter(d) {}
        unique_ptr(const unique_ptr& uptr) = delete;
        unique_ptr(unique_ptr&& uptr) noexcept : _ptr(uptr._ptr), _deleter(uptr._deleter) { uptr._ptr = nullptr; }
        ~unique_ptr() { if (_ptr != nullptr) _deleter(_ptr); }

        // Assignments
        inline unique_ptr& operator=(unique_ptr&& uptr) noexcept
        {
            std::swap(_ptr, uptr._ptr);
            std::swap(_deleter, uptr._deleter);
            return *this;
        }
        unique_ptr& operator=(const unique_ptr& uptr) = delete;

        // Modifiers
        inline T* release() { auto p = _ptr; _ptr = nullptr; return p; }
        inline void reset(T* p) { if (_ptr != nullptr) _deleter(_ptr); _ptr = p; }
        inline void swap(unique_ptr& other) { swap(_ptr, other._ptr); }

        // Observers
        inline T* get() const { return _ptr; }
        inline operator bool() const { return _ptr != nullptr; }

        // Dereference
        inline T& operator*() const { return *_ptr; }
        inline T* operator->() const { return _ptr; }

        // Comparisons
        inline bool operator==(const unique_ptr& other) const { return _ptr == other._ptr; }
        inline bool operator!=(const unique_ptr& other) const { return _ptr != other._ptr; }
    };
}


/*********************
*        Defer       *
*********************/

namespace std
{
    // Source: https://stackoverflow.com/a/42060129
    // Modified for rvalue
    struct defer_dummy {};
    template<class F>
    struct deferer
    {
        F _f;
        deferer(F&& f) : _f(std::forward<F>(f)) {}
        ~deferer() { _f(); }
    };
    template<class F>
    inline deferer<F> operator*(defer_dummy&&, F&& f)
    {
        return deferer<F>(std::forward<F>(f));
    }
}
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
#define defer auto DEFER(__LINE__) = std::defer_dummy{} *[&]()


/*********************
*        TEST        *
*********************/

#if _TEST
namespace std
{
    void test();
}
#endif