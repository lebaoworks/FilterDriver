#include <Windows.h>
#include <Fltuser.h>

#include <iostream>
#include <string>
#include <vector>
#include <cstring>

#include <thread>

#include <utility>
#include <functional>
#include <chrono>

#include "utility.hpp"
#include "Event.hpp"

#include <fstream>

static const LPCWSTR COMPORT_NAME = L"\\EvtDrvPort";

constexpr ULONG SERIALIZED_BUFFER_SIZE = 512 * 1024; // must match kernel serialized buffer size

#pragma warning(disable : 4200)
#pragma warning(disable : 4996)
namespace
{
    struct PackageHeader
    {
        UINT32 TotalSize = sizeof(PackageHeader);
        BYTE Data[0]; // Flexible array member for serialized event data
    };
}


class EvtConsumer
{
private:
    std::wstring _port_name;
    std::function<void(const byte*, size_t)> _callback;
    std::vector<byte> _buffer;
    HANDLE _cancel;
    OVERLAPPED _ov;
    std::thread _worker;

    void work() noexcept
    {
        HANDLE port;
        while (true)        // [TODO] Add proper cancellation support instead of infinite loop with sleep
        {
            HRESULT result = FilterConnectCommunicationPort(
                _port_name.c_str(),             // PortName
                0,                              // Options
                NULL,                           // Context
                0,                              // ContextSize
                NULL,                           // SecurityAttributes
                &port);                         // ServerPort
            if (result != S_OK)
            {
                printf("Connect port failed -> HRESULT: %08X", result);
                std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait before retrying
                continue;
            }
            defer{ CloseHandle(port); };

            // Successfully connected to port, start receiving messages
            bool cancelled = serve(port);

            if (cancelled)
                return;

            printf("Something went wrong from port, retrying connection...\n");
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    bool serve(HANDLE port) noexcept
    {
        while (true)
        {
            HRESULT result = FilterGetMessage(
                port,                                                           // Port handle
                reinterpret_cast<PFILTER_MESSAGE_HEADER>(_buffer.data()),       // Buffer
                static_cast<DWORD>(_buffer.size()),                             // Buffer size
                &_ov);                                                          // Overlapped structure for cancellation

            if (result != HRESULT_FROM_WIN32(ERROR_IO_PENDING))
            {
                printf("Unexpected result from FilterGetMessage -> HRESULT: %08X", result);
                break;
            }

            // Wait for either a message to be received or cancellation, cancel first to avoid race condition
            HANDLE waitHandles[2] = { _cancel, _ov.hEvent};
            result = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);


            // Cancellation event
            if (result == WAIT_OBJECT_0) 
            {
                printf("Cancellation requested, exiting serve loop.\n");

                // Cancel any pending I/O
                CancelIoEx(port, &_ov); 

                // Wait for the I/O operation to complete
                DWORD bytesTransferred = 0;
                GetOverlappedResult(port, &_ov, &bytesTransferred, TRUE);

                return true;
            }
            // Message received, process it
            else if (result == WAIT_OBJECT_0 + 1)
            {
                DWORD bytesTransferred = 0;
                if (!GetOverlappedResult(port, &_ov, &bytesTransferred, FALSE))
                {
                    printf("GetOverlappedResult failed -> HRESULT: %08X", GetLastError());
                    return false;
                }
                printf("Received message of size %d bytes\n", bytesTransferred);
                
                try
                {
                    // Invoke the callback with the data from kernel only
                    _callback(_buffer.data() + sizeof(FILTER_MESSAGE_HEADER), bytesTransferred - sizeof(FILTER_MESSAGE_HEADER));
                }
                catch (const std::exception& ex)
                {
                    printf("Failed to process message, error: %s\n", ex.what());
                }
            }
            // Unexpected result
            else
            {
                printf("WaitForMultipleObjects failed -> HRESULT: %08X", GetLastError());
                return false;
            }
        }
    }

public:
    EvtConsumer(const std::wstring& port_name, std::function<void(const byte*, size_t)> callback) :
        _port_name(port_name),
        _callback(std::move(callback)),
        _buffer(sizeof(FILTER_MESSAGE_HEADER) + SERIALIZED_BUFFER_SIZE)
    {
        bool success = false;

        _cancel = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (_cancel == NULL)
            throw std::runtime_error("Failed to create cancellation event, error: " + std::to_string(GetLastError()));
        defer{ if (!success) CloseHandle(_cancel); };

        _ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (_ov.hEvent == NULL)
            throw std::runtime_error("Failed to create overlapped event, error: " + std::to_string(GetLastError()));
        defer{ if (!success) CloseHandle(_ov.hEvent); };

        _worker = std::thread([&]() { work(); });

        success = true;
    }

    ~EvtConsumer()
    {
        SetEvent(_cancel); // Signal cancellation
        _worker.join();

        CloseHandle(_cancel);
        CloseHandle(_ov.hEvent);
    }
};

int wmain(int argc, wchar_t* argv[])
{
    try
    {
        EvtConsumer consumer(COMPORT_NAME, [](const byte* data, size_t size) {
            auto header = reinterpret_cast<const PackageHeader*>(data);
            //printf("Received package: %lu bytes\n", header->TotalSize);

            const byte* ptr = header->Data;
            const byte* end_ptr = data + size;

            while (ptr < end_ptr)
            {
                BYTE event_type = *ptr;
                ptr += sizeof(BYTE);
                //printf("[Offset: %6lld] Event Type: %d\n", ptr - data - sizeof(BYTE), event_type);

                // Extract event-specific data based on event_type
                switch (event_type)
                {
                case Event::FileOpen:
                {
                    Event::FileOpenEvent event;
                    ptr += event.Deserialize(ptr, end_ptr - ptr);
                    time_t event_time = std::chrono::system_clock::to_time_t(event.TimeStamp);
                    printf("FileOpen: Time: %.24s, ProcessId: %6lu, FileName: %ls\n", ctime(&event_time), event.ProcessId, event.FileName.c_str());
                    break;
                }
                case Event::ProcessCreate:
                {
                    Event::ProcessCreateEvent event;
                    ptr += event.Deserialize(ptr, end_ptr - ptr);
                    time_t event_time = std::chrono::system_clock::to_time_t(event.TimeStamp);
                    printf("ProcessCreate: Time: %.24s, ProcessId: %6lu, ParentProcessId: %6lu, ImageName: %ls, CommandLine: %ls\n", ctime(&event_time), event.ProcessId, event.ParentProcessId, event.ImageName.c_str(), event.CommandLine.c_str());
                    break;
                }
                case Event::ProcessExit:
                {
                    Event::ProcessExitEvent event;
                    ptr += event.Deserialize(ptr, end_ptr - ptr);
                    time_t event_time = std::chrono::system_clock::to_time_t(event.TimeStamp);
                    time_t creation_time = std::chrono::system_clock::to_time_t(event.ProcessCreationTime);
                    printf("ProcessExit: Time: %.24s, ProcessId: %6lu, ProcessCreationTime: %.24s\n", ctime(&event_time), event.ProcessId, ctime(&creation_time));
                    break;
                }
                case Event::ProcessOpen:
                {
                    Event::ProcessOpenEvent event;
                    ptr += event.Deserialize(ptr, end_ptr - ptr);
                    time_t event_time = std::chrono::system_clock::to_time_t(event.TimeStamp);
                    printf("ProcessOpen: Time: %.24s, ProcessId: %6lu, TargetProcessId: %6lu, DesiredAccess: 0x%08X\n", ctime(&event_time), event.ProcessId, event.TargetProcessId, event.DesiredAccess);
                    break;
                }
                
                default:
                    printf("unknown event type %d at offset %lld, skip package\n", event_type, ptr - header->Data);
                    return;
                }
            }
            });
        std::cin.get();
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}