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
        ProcessCreate = 100,
        ProcessExit = 101,
        ProcessOpen = 102,
        RemoteThreadCreate = 103,

    };

    struct Event;

    _IRQL_requires_max_(APC_LEVEL)
    _IRQL_requires_same_
    using EventNotifyCallback = NTSTATUS(*)(krn::unique_ptr<Event>&);

    using String = krn::UnicodeStringBase<krn::tag<'EVT1'>>;
}

namespace Event
{
    /* Serialized buffer layout for all events (packed, byte offsets)

    +--------+------+------------------------------+-----------------------------------------------+
    | Offset | Size | Field                        | Description                                   |
    +--------+------+------------------------------+-----------------------------------------------+
    | 0      | 1    | Type (BYTE)                  | Event::Type                                   |
    | 1      | 8    | TimeStamp (I64)              | 100-ns intervals since 1601-01-01             |
    +----------------------------------------------------------------------------------------------+
    | 9      | n    | Event-specific data          | Varies based on event type                    |
    +--------+------+------------------------------+-----------------------------------------------+
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

    +--------+------+------------------------------+-----------------------------------------------+
    | Offset | Size | Field                        | Description                                   |
    +--------+------+------------------------------+-----------------------------------------------+
    |     Event Base Fields (9 bytes) - see Event struct layout above                              |
    +----------------------------------------------------------------------------------------------+
    | 9      | 4    | ProcessId (U32)              | FileOpenEvent-specific                        |
    | 13     | 2    | FileName.Length (U16)        | Length in bytes (Unicode)                     |
    | 15     | n    | FileName.Buffer              | FileName.Length bytes                         |
    +--------+------+------------------------------+-----------------------------------------------+
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
    /* Serialized buffer layout for ProcessCreateEvent (packed, byte offsets)

    +--------+------+------------------------------+-----------------------------------------------+
    | Offset | Size | Field                        | Description                                   |
    +--------+------+------------------------------+-----------------------------------------------+
    |     Event Base Fields (9 bytes) - see Event struct layout above                              |
    +----------------------------------------------------------------------------------------------+
    | 9      | 4    | ProcessId (U32)              | Originating process id                        |
    | 13     | 4    | ParentProcessId (U32)        | Parent process id                             |
    | 17     | 2    | ImageName.Length (U16)       | Length in bytes (Unicode)                     |
    | 19     | n    | ImageName.Buffer             | Image name string                             |
    | 19+n   | 2    | CommandLine.Length (U16)     | Length in bytes (Unicode)                     |
    | 21+n   | m    | CommandLine.Buffer           | Command line string                           |
    +--------+------+------------------------------+-----------------------------------------------+
    */
    struct ProcessCreateEvent : public Event
    {
        ULONG ProcessId = 0;
        ULONG ParentProcessId = 0;
        String ImageName;
        String CommandLine;
        
        ProcessCreateEvent() noexcept { Type = Types::ProcessCreate; }

        virtual ~ProcessCreateEvent() {}

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

            // ParentProcessId
            *(UINT32*)ptr = ParentProcessId;
            ptr += sizeof(UINT32);

            // ImageName Length (bytes)
            *(UINT16*)ptr = ImageName.Length;
            ptr += sizeof(UINT16);

            // ImageName Buffer
            RtlCopyMemory(ptr, ImageName.Buffer, ImageName.Length);
            ptr += ImageName.Length;

            // CommandLine Length (bytes)
            *(UINT16*)ptr = CommandLine.Length;
            ptr += sizeof(UINT16);

            // CommandLine Buffer
            RtlCopyMemory(ptr, CommandLine.Buffer, CommandLine.Length);

            return STATUS_SUCCESS;
        }

        virtual ULONG SerializedSize_() const override
        {
            return sizeof(UINT32) * 2 + /* ProcessId + ParentProcessId */
                sizeof(UINT16) * 2 + /* ImageName.Length + CommandLine.Length */
                ImageName.Length + CommandLine.Length;
        }

    };


    /* Serialized buffer layout for ProcessExitEvent (packed, byte offsets)
    
    +--------+------+------------------------------+-----------------------------------------------+
    | Offset | Size | Field                        | Description                                   |
    +--------+------+------------------------------+-----------------------------------------------+
    |     Event Base Fields (9 bytes) - see Event struct layout above                              |
    +----------------------------------------------------------------------------------------------+
    | 9      | 4    | ProcessId (U32)              | Originating process id                        |
    | 13     | 8    | ProcessCreationTime (I64)    | 100-ns intervals since 1601-01-01             |
    +--------+------+------------------------------+-----------------------------------------------+
    */
    struct ProcessExitEvent : public Event
    {
        ULONG ProcessId = 0;
        LARGE_INTEGER ProcessCreationTime;
       
        ProcessExitEvent() noexcept { Type = Types::ProcessExit; }

        virtual ~ProcessExitEvent() {}

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

            // ProcessCreationTime
            INT64 pct = ProcessCreationTime.QuadPart;
            RtlCopyMemory(ptr, &pct, sizeof(pct));

            return STATUS_SUCCESS;
        }
        virtual ULONG SerializedSize_() const override
        {
            return sizeof(UINT32) + sizeof(INT64);
        }
    };

    /* Serialized buffer layout (packed, byte offsets)

    +--------+------+------------------------------+-----------------------------------------------+
    | Offset | Size | Field                        | Description                                   |
    +--------+------+------------------------------+-----------------------------------------------+
    |     Event Base Fields (9 bytes) - see Event struct layout above                              |
    +----------------------------------------------------------------------------------------------+
    | 9      | 4    | ProcessId (U32)              | Originating process id                        |
    | 13     | 4    | TargetProcessId (U32)        | Process id being opened                       |
    | 17     | 4    | DesiredAccess (U32)          | Requested access mask                         |
    +--------+------+------------------------------+-----------------------------------------------+
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


    /* Serialized buffer layout (packed, byte offsets)
    
    +--------+------+------------------------------+-----------------------------------------------+
    | Offset | Size | Field                        | Description                                   |
    +--------+------+------------------------------+-----------------------------------------------+
    |     Event Base Fields (9 bytes) - see Event struct layout above                              |
    +----------------------------------------------------------------------------------------------+
    | 9      | 4    | ProcessId (U32)              | Originating process id                        |
    | 13     | 4    | TargetProcessId (U32)        | Process id in which thread is being created   |
    | 17     | 4    | ThreadId (U32)               | Created thread id                             |
    +--------+------+------------------------------+-----------------------------------------------+
    */
    struct RemoteThreadCreateEvent : public Event
    {
        ULONG ProcessId = 0;
        ULONG TargetProcessId = 0;
        ULONG ThreadId = 0;

        RemoteThreadCreateEvent() noexcept { Type = Types::RemoteThreadCreate; }
        
        virtual ~RemoteThreadCreateEvent() {}
        
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

            // ThreadId
            *(UINT32*)ptr = ThreadId;
            ptr += sizeof(UINT32);

            return STATUS_SUCCESS;
        }
        
        virtual ULONG SerializedSize_() const override
        {
            return sizeof(UINT32) * 3;
        }
    };

}