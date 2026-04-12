#include <Windows.h>
#include <Fltuser.h>

#include <iostream>
#include <string>
#include <vector>
#include <cstring>

#include <utility>
#include <functional>

static const LPCWSTR COMPORT_NAME = L"\\EvtDrvPort";

constexpr ULONG SERIALIZED_BUFFER_SIZE = 512 * 1024; // must match kernel serialized buffer size

#pragma warning(disable : 4200)
namespace
{
    struct Header
    {
        FILTER_MESSAGE_HEADER FilterHeader;

        ULONG TotalSize = sizeof(Header);
        BYTE Data[0]; // Flexible array member for serialized event data
    };
    static_assert(sizeof(Header) < SERIALIZED_BUFFER_SIZE, "Header size must be less than the total serialized buffer size");
}


class FilterPort
{
private:
    HANDLE _port = NULL;
    std::vector<byte> _buffer;

public:
    FilterPort(const std::wstring& comport_name, size_t max_buffer_size)
    {
        _buffer.resize(sizeof(FILTER_MESSAGE_HEADER) + max_buffer_size);

        HRESULT result = FilterConnectCommunicationPort(
            comport_name.c_str(),           // PortName
            0,                              // Options
            NULL,                           // Context
            0,                              // ContextSize
            NULL,                           // SecurityAttributes
            &_port);                        // ServerPort
        if (result != S_OK)
            throw std::runtime_error("FilterConnectCommunicationPort failed -> HRESULT: " + std::to_string(result));
    }
    ~FilterPort()
    {
        CloseHandle(_port);
    }

    void GetMessage(std::function<void(const std::vector<byte>& data)> callback)
    {
        HRESULT result = FilterGetMessage(
            _port,
            reinterpret_cast<PFILTER_MESSAGE_HEADER>(_buffer.data()),
            static_cast<DWORD>(_buffer.size()),
            NULL);
        if (result != S_OK)
            throw std::runtime_error("FilterGetMessage failed -> HRESULT: " + std::to_string(result));
        callback(_buffer);
    }
};

namespace Event
{
    enum Types
    {
        Invalid = 0,
        FileOpen = 1,
    };
}

int wmain(int argc, wchar_t* argv[])
{
    try
    {
        FilterPort port(COMPORT_NAME, SERIALIZED_BUFFER_SIZE);

        while (true)
            port.GetMessage([](const std::vector<byte>& data) {
                auto header = reinterpret_cast<const Header*>(data.data());
                std::cout << "Received message with total size: " << header->TotalSize << " bytes" << std::endl;

                auto ptr = header->Data;
                while (ptr < data.data() + header->TotalSize)
                {
                    // Extract base 
                    BYTE event_type = *ptr;
                    ptr += sizeof(BYTE);
                    INT64 timestamp = *reinterpret_cast<const INT64*>(ptr);
                    ptr += sizeof(INT64);

                    std::cout << "[+] Event Type: " << static_cast<int>(event_type) << ", Timestamp: " << timestamp << std::endl;
                    // Extract event-specific data based on event_type
                    switch (event_type)
                    {
                        case Event::FileOpen:
                        {
                            UINT32 process_id = *reinterpret_cast<const UINT32*>(ptr);
                            ptr += sizeof(UINT32);
                            UINT16 path_length = *reinterpret_cast<const UINT16*>(ptr);
                            ptr += sizeof(UINT16);
                            std::wstring file_path(reinterpret_cast<const wchar_t*>(ptr), path_length / sizeof(wchar_t));
                            ptr += path_length;
                            std::wcout << L"\tFileOpen Event - Process ID: " << process_id << L", File Path: " << file_path << std::endl;
                        }
                        break;
                    }
                    // Process event-specific data based on event_type and advance ptr accordingly
                    // For example, if event_type == 1 (FileOpen), you might read additional fields specific to that event
                }
            });
    }
    catch (const std::runtime_error& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
    }

    return 0;
}

