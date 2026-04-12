#pragma once

/********************
*     Includes      *
********************/

#include <krn.hpp>
#include <ntintsafe.h>

namespace Event
{
    using String = krn::UnicodeStringBase<krn::tag<'EVT1'>>;

    struct Event : public krn::tag<'EVT1'>
    {
        //
        //    Enum
        //

        enum Types
        {
            Invalid = 0,
            FileOpen = 1,
        };

        //
        // Fields
        //

        BYTE Type = Invalid;
        LARGE_INTEGER TimeStamp;

        //
        // Constructors & Methods
        //

        Event() noexcept { KeQuerySystemTime(&TimeStamp); }

        virtual ~Event() {}

        NTSTATUS Serialize(
            _Inout_ PVOID Buffer,
            _In_ ULONG BufferSize) const
        {
            if (BufferSize < sizeof(BYTE) + sizeof(INT64))
                return STATUS_BUFFER_TOO_SMALL;

            BYTE* ptr = (BYTE*)Buffer;

            // Type
            *(BYTE*)ptr = Type;
            ptr += sizeof(Type);

            // TimeStamp
            *(INT64*)ptr = TimeStamp.QuadPart;
            ptr += sizeof(TimeStamp);

            // Event-specific data
            return Serialize_(ptr, BufferSize - sizeof(Type) - sizeof(TimeStamp));
        }

        ULONG SerializedSize() const
        {
            return sizeof(BYTE) + sizeof(INT64) + SerializedSize_();
        }

    protected:
        virtual NTSTATUS Serialize_(_Inout_ PVOID Buffer, _In_ ULONG BufferSize) const = 0;
        virtual ULONG    SerializedSize_() const = 0;
    };

    struct FileOpenEvent : public Event
    {
        ULONG ProcessId = 0;
        String FileName;

        FileOpenEvent() noexcept { Type = Types::FileOpen; }

        virtual ~FileOpenEvent() {};

        virtual NTSTATUS Serialize_(
            _Inout_ PVOID Buffer,
            _In_ ULONG BufferSize) const override
        {
            if (BufferSize < SerializedSize_())
                return STATUS_BUFFER_TOO_SMALL;

            BYTE* ptr = (BYTE*)Buffer;

            // ProcessId
            *(UINT32*)ptr = ProcessId;
            ptr += sizeof(UINT32);

            // FileName Lengths
            *(UINT16*)ptr = FileName.Length;
            ptr += sizeof(UINT16);

            // FileName Buffer
            RtlCopyMemory(ptr, FileName.Buffer, FileName.Length);
            return STATUS_SUCCESS;
        }

        virtual ULONG SerializedSize_() const override
        {
            return sizeof(UINT32) + sizeof(UINT16) + FileName.Length;
        }
    };
}