#pragma once

#include <ntdef.h>
#include <ntstatus.h>

// Reference: https://stackoverflow.com/questions/37031844/logic-of-stdis-base-of-in-c-11
namespace base
{
    namespace detail
    {
        template<typename D, typename B>
        class derived_helper
        {
            class No { };
            class Yes { No no[3]; };

            static Yes Test(B*);
            static No Test(...);
        public:
            enum { Is = sizeof(Test(static_cast<D*>(0))) == sizeof(Yes) };

        };

    }
    
    template <class C, class P>
    constexpr bool is_derived_from() { return detail::derived_helper<C, P>::Is; }
}

namespace base
{
    struct failable
    {
    private:
        NTSTATUS _status = STATUS_SUCCESS;
    protected:
        inline void fail(NTSTATUS status) { _status = status; }
    public:
        inline NTSTATUS status() const { return _status; };
    };
    
    template<typename T>
    class object_failable
    {
        static_assert(is_derived_from<T, failable>());
    private:
        T* _ptr = nullptr;
        NTSTATUS _status = STATUS_SUCCESS;
    public:
        // Constructors
        //unique_ptr() noexcept {}
        //unique_ptr(T* p) noexcept : _ptr(p) {}
        //unique_ptr(const unique_ptr<T>& uptr) = delete;
        //unique_ptr(unique_ptr<T>&& uptr) noexcept : _ptr(uptr._ptr) { uptr._ptr = nullptr; }
        //~unique_ptr() { if (_ptr != nullptr) delete _ptr; }

        //// Assignments
        //inline unique_ptr<T>& operator=(unique_ptr<T>&& uptr) noexcept { std::swap(_ptr, uptr._ptr); return *this; }
        //unique_ptr<T>& operator=(const unique_ptr<T>& uptr) = delete;

        //// Modifiers
        //inline T* release() { auto p = _ptr; _ptr = nullptr; return p; }
        //inline void reset(T* p) { if (_ptr != nullptr) delete _ptr; _ptr = p; }
        //inline void swap(unique_ptr<T>& other) { swap(_ptr, other._ptr); }

        //// Observers
        //inline T* get() const { return _ptr; }
        //inline operator bool() const { return _ptr != nullptr; }

        //// Dereference
        //inline T& operator*() const { return *_ptr; }
        //inline T* operator->() const { return _ptr; }

        //// Comparisons
        //inline bool operator==(const unique_ptr& other) const { return _ptr == other._ptr; }
        //inline bool operator!=(const unique_ptr& other) const { return _ptr != other._ptr; }
        //inline bool operator<(const unique_ptr& other) const { return _ptr < other._ptr; }
        //inline bool operator>(const unique_ptr& other) const { return _ptr > other._ptr; }
        //inline bool operator<=(const unique_ptr& other) const { return _ptr <= other._ptr; }
        //inline bool operator>=(const unique_ptr& other) const { return _ptr >= other._ptr; }
    };
}
