#pragma once

/********************
*     Includes      *
********************/

#include <krn.hpp>
#include <ntintsafe.h>



namespace Event
{
    //
    //    Enum
    //

    enum Types
    {
        Invalid = 0,

        // File related events 1-99
        FileOpen = 1,

        // Process related events 100-199
        ProcessOpen = 100,
    };

    struct Event;

    using EventNotifyCallback = NTSTATUS(*)(krn::unique_ptr<Event>&);
    using String = krn::UnicodeStringBase<krn::tag<'EVT1'>>;
}

namespace Event
{
    /* Serialized buffer layout for all events (packed, byte offsets)

     +--------+------+----------------------+-----------------------------------+
     | Offset | Size | Field                | Description                       |
     +--------+------+----------------------+-----------------------------------+
     | 0      | 1    | Type (BYTE)          | Event::Type                       |
     | 1      | 8    | TimeStamp (I64)      | 100-ns intervals since 1601-01-01 |
     +--------------------------------------------------------------------------+
     | 9      | n    | Event-specific data  | Varies based on event type        |
     +--------+------+----------------------+-----------------------------------+

     Total serialized size = 1 + 8 + event-specific data size
    */
    struct Event : public krn::tag<'EVT1'>
    {
        BYTE Type = Invalid;
        LARGE_INTEGER TimeStamp;

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
            RtlCopyMemory(ptr, &Type, sizeof(Type));
            ptr += sizeof(Type);

            // TimeStamp
            INT64 ts = TimeStamp.QuadPart;
            RtlCopyMemory(ptr, &ts, sizeof(ts));
            ptr += sizeof(ts);

            // Event-specific data
            return Serialize_(ptr, BufferSize - sizeof(Type) - sizeof(ts));
        }

        ULONG SerializedSize() const
        {
            return sizeof(BYTE) + sizeof(INT64) + SerializedSize_();
        }

    protected:
        virtual NTSTATUS Serialize_(_Inout_ PVOID Buffer, _In_ ULONG BufferSize) const = 0;
        virtual ULONG    SerializedSize_() const = 0;
    };
}

// File related events
namespace Event
{
    /* Serialized buffer layout (packed, byte offsets)

     +--------+------+----------------------+-----------------------------+
     | Offset | Size | Field                | Description                 |
     +--------+------+----------------------+-----------------------------+
     |     Event Base Fields (9 bytes) - see Event struct layout above    |
     +--------------------------------------------------------------------+
     | 9      | 4    | ProcessId (U32)      | FileOpenEvent-specific      |
     | 13     | 2    | FileName.Length (U16)| Length in bytes (Unicode)   |
     | 15     | n    | FileName.Buffer      | FileName.Length bytes       |
     +--------+------+----------------------+-----------------------------+

     Total serialized size = 1 + 8 + event-specific (e.g. 4 + 2 + FileName.Length)
    */
    struct FileOpenEvent : public Event
    {
        ULONG ProcessId = 0;
        String FileName;

        FileOpenEvent() noexcept { Type = Types::FileOpen; }

        virtual ~FileOpenEvent() {}

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


// Process related events
namespace Event
{
    /* Serialized buffer layout (packed, byte offsets)

     +--------+------+----------------------+---------------------------------+
     | Offset | Size | Field                | Description                     |
     +--------+------+----------------------+---------------------------------+
     |     Event Base Fields (9 bytes) - see Event struct layout above            |
     +-------------------------------------------------------------------------+
     | 9      | 4    | ProcessId (U32)      | Originating process id          |
     | 13     | 4    | TargetProcessId (U32)| Process id being opened         |
     | 17     | 4    | DesiredAccess (U32)  | Requested access mask           |
     +--------+------+----------------------+---------------------------------+

     Total serialized size = 1 + 8 + (4 + 4 + 4)
    */
    struct ProcessOpenEvent : public Event
    {
        ULONG ProcessId = 0;
        ULONG TargetProcessId = 0;
        ULONG DesiredAccess = 0;

        ProcessOpenEvent() noexcept { Type = Types::ProcessOpen; }

        virtual ~ProcessOpenEvent() {}

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

            // TargetProcessId
            *(UINT32*)ptr = TargetProcessId;
            ptr += sizeof(UINT32);

            // DesiredAccess
            *(UINT32*)ptr = DesiredAccess;
            ptr += sizeof(UINT32);

            return STATUS_SUCCESS;
        }

        virtual ULONG SerializedSize_() const override
        {
            return sizeof(UINT32) * 3;
        }   
    };
}