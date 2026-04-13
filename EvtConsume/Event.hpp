#pragma once

#include <chrono>

namespace Event
{
    enum Types
    {
        Invalid = 0,
        FileOpen = 1,
    };

    struct Event
    {
        std::chrono::system_clock::time_point TimeStamp;
    
        size_t Deserialize(
            const byte* Buffer,
            size_t      BufferSize)
        {
            if (BufferSize < 8)
                throw std::runtime_error("Buffer too small for deserialization");

            // Get TimeStamp (INT64 ticks since 1601-01-01)
            std::chrono::duration<long long, std::ratio<1, 10000000>> ticks(*(INT64*)Buffer);
            // Convert into Unix Epoch (1970)
            auto unix_ticks = ticks - std::chrono::duration<long long, std::ratio<1, 10000000>>(116444736000000000LL);
            // Convert to system_clock to get the current system time
            TimeStamp = std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(unix_ticks));

            return 8;
        }
    };

    struct FileOpenEvent : public Event
    {
        ULONG ProcessId = 0;
        std::wstring FileName;

        size_t Deserialize(
            const byte*  Buffer,
            size_t       BufferSize,
            bool         SkipBase = false)
        {
            const byte* ptr = Buffer;

            if (!SkipBase)
                ptr += Event::Deserialize(Buffer, BufferSize);


            if (BufferSize < ptr + 6 - Buffer) // ProcessId (4 bytes) + FileName.Length (2 bytes)
                throw std::runtime_error("Buffer too small for FileOpenEvent deserialization");

            // ProcessId
            ProcessId = *(UINT32*)(ptr);
            ptr += sizeof(UINT32);

            // FileName Length
            UINT16 file_name_length = *(UINT16*)(ptr);
            ptr += sizeof(UINT16);
            if (Buffer + BufferSize < ptr + file_name_length)
                throw std::runtime_error("Buffer too small for FileName deserialization, got " + std::to_string(BufferSize - (ptr - Buffer)) + ", expected: " + std::to_string(file_name_length));
            // FileName Buffer
            FileName.assign((wchar_t*)(ptr), file_name_length / sizeof(wchar_t));
            ptr += file_name_length;

            return ptr - Buffer;
        }
    };
}