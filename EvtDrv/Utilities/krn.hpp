#pragma once

/********************
*     Includes      *
********************/
#include <fltKernel.h>
#include <wdm.h>
#include <sal.h>
#include <ntddk.h>

#include <std.hpp>

/*********************
*       Traits       *
*********************/

namespace krn
{
    template<ULONG TAG>
    struct tag
    {
        // Allocating new: returns allocated memory or nullptr on failure
        _When_(return != nullptr, __drv_allocatesMem(Mem))
        _Ret_maybenull_
        void* _cdecl operator new(_In_ size_t size) { return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, TAG); }

        // Placement new: returns the same pointer passed in (no allocation)
        _Ret_notnull_
        void* _cdecl operator new(_In_ size_t size, _Inout_ void* ptr) { UNREFERENCED_PARAMETER(size); return ptr; }

		// Delete: destroy object pointed to by the pointer and free memory
        void _cdecl operator delete(_In_opt_ _Post_ptr_invalid_ __drv_freesMem(Mem) void* pointer) { if (pointer) ExFreePool(pointer); }

        // Delete with size: destroy object pointed to by the pointer and free memory
        void _cdecl operator delete(_In_opt_ _Post_ptr_invalid_ __drv_freesMem(Mem) void* pointer, size_t size) { UNREFERENCED_PARAMETER(size); if (pointer) ExFreePool(pointer); }
    };

    struct failable
    {
    protected:
        NTSTATUS _status = STATUS_SUCCESS;

    public:
        _Must_inspect_result_
        inline NTSTATUS status() const noexcept { return _status; };
    };
}

/*********************
*       String       *
*********************/

namespace krn
{
    template<typename alloc_trait>
    struct UnicodeStringBase : public UNICODE_STRING, public krn::failable, public alloc_trait
    {
        /// @brief Constructs an empty string.
        /// @remarks status() will be STATUS_SUCCESS, and the object will be in a default initialized state (empty string).
        UnicodeStringBase() : UNICODE_STRING{ 0, 0, nullptr } {}

        /// @brief Move constructor.
        /// @param other The source UnicodeStringBase object to move from. After the move, this object will be in a default initialized state (empty string).
        UnicodeStringBase(UnicodeStringBase&& other) noexcept
        {
            Length = other.Length;
            MaximumLength = other.MaximumLength;
            Buffer = other.Buffer;
            // Invalidate the moved-from object
            other.Length = 0;
            other.MaximumLength = 0;
            other.Buffer = nullptr;
        }

        /// @brief Copy constructor that creates a new string by copying the contents of another.
        /// @param other The source string to copy from.
        /// @remarks On success, status() will be STATUS_SUCCESS and the object will contain a copy of the provided string.
        /// @remarks On failure, status() will be set to the error code, and the object will be in a default initialized state (empty string).
        UnicodeStringBase(const UnicodeStringBase& other) noexcept
        {
            Length = other.Length;
            MaximumLength = other.MaximumLength;
            if (MaximumLength == 0)
            {
                Buffer = nullptr;
                return;
            }
            Buffer = reinterpret_cast<PWCH>(alloc_trait::operator new(MaximumLength));
            if (Buffer == nullptr)
            {
                Length = MaximumLength = 0;
                _status = STATUS_NO_MEMORY;
                return;
            }
            RtlCopyMemory(Buffer, other.Buffer, Length);
            
        }

        /// @brief Create a copy of provided UNICODE_STRING.
        /// @param other 
        /// @remarks On success, status() will be STATUS_SUCCESS and the object will contain a copy of the provided UNICODE_STRING.
        /// @remarks On failure, status() will be set to the error code, and the object will be in a default initialized state (empty string).
        UnicodeStringBase(const UNICODE_STRING& other)
        {
            Length = other.Length;
            MaximumLength = other.MaximumLength;
            if (MaximumLength == 0)
            {
                Buffer = nullptr;
                return;
            }
            Buffer = reinterpret_cast<PWCH>(alloc_trait::operator new(MaximumLength));
            if (Buffer == nullptr)
            {
                Length = MaximumLength = 0;
                _status = STATUS_NO_MEMORY;
                return;
            }
            RtlCopyMemory(Buffer, other.Buffer, Length);
        }

        /// @brief Assigns a UNICODE_STRING to this object by copying its contents.
        /// @param other The UNICODE_STRING to copy from.
        /// @return A reference to this UnicodeStringBase object after assignment.
        /// @remarks On success, status() will be STATUS_SUCCESS and the object will contain a copy of the provided UNICODE_STRING.
        /// @remarks On failure, status() will be set to the error code, and the object will be in a default initialized state (empty string).
        UnicodeStringBase& operator=(const UNICODE_STRING& other)
        {
            if (Buffer) alloc_trait::operator delete(Buffer);
            Length = other.Length;
            MaximumLength = other.MaximumLength;
            if (MaximumLength == 0)
            {
                Buffer = nullptr;
                return *this;
            }
            Buffer = reinterpret_cast<PWCH>(alloc_trait::operator new(MaximumLength));
            if (Buffer == nullptr)
            {
                Length = MaximumLength = 0;
                _status = STATUS_NO_MEMORY;
                return *this;
            }
            RtlCopyMemory(Buffer, other.Buffer, Length);
            return *this;
        }

        ~UnicodeStringBase()
        {
            if (Buffer) alloc_trait::operator delete(Buffer);
        }
    };

    using UnicodeString = UnicodeStringBase<tag<'STR0'>>;
}

/*********************
*       Utility      *
*********************/

namespace krn
{
    template<typename T>
    class unique_ptr
    {
        static_assert(std::is_array<T>::value == false, "unique_ptr does not support arrays");
    private:
        T* _ptr = nullptr;
    public:
        // Constructors
        explicit unique_ptr(T* p = nullptr) noexcept : _ptr(p) {}
        unique_ptr(const unique_ptr& uptr) = delete;
        unique_ptr(unique_ptr&& uptr) noexcept : _ptr(uptr._ptr) { uptr._ptr = nullptr; }
        ~unique_ptr() { if (_ptr != nullptr) delete _ptr; }

        // Assignments
        inline unique_ptr& operator=(unique_ptr&& uptr) noexcept
        {
            if (_ptr) delete _ptr;
            _ptr = uptr._ptr;
            uptr._ptr = nullptr;
            return *this;
        }
        unique_ptr& operator=(const unique_ptr& uptr) = delete;

        // Modifiers
        inline T* release() { auto p = _ptr; _ptr = nullptr; return p; }
        inline void reset(T* p) { if (_ptr != nullptr) delete _ptr; _ptr = p; }
        inline void swap(unique_ptr& other) { auto t = _ptr; _ptr = other._ptr; other._ptr = t; }

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


    template<typename T>
    struct make_result : unique_ptr<T>
    {
        template<typename T, typename... Args>
        friend make_result<T> make(Args&&...);
    private:
        NTSTATUS _status = STATUS_SUCCESS;

        make_result(T* ptr, NTSTATUS status) : unique_ptr<T>(ptr), _status(status) {}
        make_result(NTSTATUS status) : _status(status) {}

    public:
        ~make_result() = default;

        inline NTSTATUS status() const noexcept { return _status; }
        inline T* release() noexcept { return unique_ptr<T>::release(); }
        inline T& value() noexcept { return *unique_ptr<T>::get(); }
    };


	/// @brief Create a new object of type T.
    /// @tparam T 
    /// @tparam ...Args 
	/// @param ...args Parameters to forward to T's constructor
	/// @return A make_result object containing the status of the operation, and the created object if NT_SUCCESS(status).
    template<typename T, typename... Args>
    make_result<T> make(Args&&... args)
    {
        auto val = new T(std::forward<Args>(args)...);
        if (val == nullptr)
            return STATUS_NO_MEMORY;
		NTSTATUS status = STATUS_SUCCESS;
        if constexpr (std::is_base_of<krn::failable, T>::value)
        {
            status = val->status();
            if (!NT_SUCCESS(status))
            {
                delete val;
                return status;
            }
        }

        return { val, status };
    }
}

/*********************
*        Defer       *
*********************/

namespace krn
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
#define defer auto DEFER(__LINE__) = krn::defer_dummy{} *[&]()


