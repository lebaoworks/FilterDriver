#pragma once

/********************
*     Includes      *
********************/
#include <sal.h>
#include <ntddk.h>
#include <wdm.h>

#include <std.hpp>

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
        void* _cdecl operator new(_In_ size_t size, _In_ void* ptr) { UNREFERENCED_PARAMETER(size); return ptr; }

		// Delete: destroy object and free memory
        __drv_freesMem(Mem)
        void _cdecl operator delete(_In_ void* object) { ExFreePool(object); }

        // Delete with size: destroy object and free memory
        void operator delete(_Notnull_ __drv_freesMem(Mem) void* ptr, size_t size) { UNREFERENCED_PARAMETER(size); ExFreePool(ptr); }
    };

    struct failable
    {
    protected:
        NTSTATUS _status = STATUS_SUCCESS;
    public:
        // Status should be inspected by the caller
        _Must_inspect_result_
        inline NTSTATUS status() const noexcept { return _status; };
    };
}

/*********************
*       Utility      *
*********************/

namespace krn
{
    template<typename T>
    struct make_result : std::unique_ptr<T>
    {
        template<typename T, typename... Args>
        friend make_result<T> make(Args&&...);
    private:
        NTSTATUS _status = STATUS_SUCCESS;

        make_result(T* ptr, NTSTATUS status) : std::unique_ptr<T>(ptr), _status(status) {}
        make_result(NTSTATUS status) : _status(status) {}

    public:
        ~make_result() = default;

        inline NTSTATUS status() const noexcept { return _status; }
        inline T* release() noexcept { return std::unique_ptr<T>::release(); }
        inline T& value() noexcept { return *std::unique_ptr<T>::get(); }
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


