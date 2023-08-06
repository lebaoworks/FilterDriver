#pragma once

#include <fltKernel.h>
#include <Shared.h>

// Forward declarations
namespace MiniFilter
{
    class Filter;
    class CommunicationPort;
}

namespace MiniFilter
{
    class Filter : public failable_object<NTSTATUS>
    {
    private:
        PFLT_FILTER _filter = NULL;
        std::list<std::unique_ptr<CommunicationPort>> _comports;
        std::list<PFLT_PORT> _connections;
        eresource_lock _connections_lock;
    public:
        Filter(_In_ DRIVER_OBJECT* DriverObject);
        ~Filter();

        PFLT_FILTER GetFilter() const;

        NTSTATUS RegisterCommunicationPort(_In_ UNICODE_STRING* PortName);

        NTSTATUS AddConnection(_In_ PFLT_PORT ClientPort);
        void RemoveConnection(_In_ PFLT_PORT ClientPort);
        void ClearConnections();
    };
}

namespace MiniFilter
{
    class CommunicationPort : public failable_object<NTSTATUS>
    {
    private:
        PFLT_PORT _port = NULL;
    public:
        CommunicationPort(_In_ const Filter& Filter, _In_ UNICODE_STRING* PortName);
        ~CommunicationPort();
    };
}
