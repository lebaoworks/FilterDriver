#pragma once

#include <fltKernel.h>

#include <Shared.h>
#include <Communication.h>

// Forward declarations
namespace MiniFilter
{
    class Filter;
    class Port;
    class Connection;
}

namespace MiniFilter
{
    class Filter : public failable_object<NTSTATUS>
    {
    private:
        PFLT_FILTER _filter = NULL;
        
        std::unique_ptr<Communication::SavedCredential> _credential;
        std::list<Port> _ports;
        std::list<Connection> _connections;
        eresource_lock _connections_lock;

    public:
        Filter(_In_ DRIVER_OBJECT* DriverObject);
        ~Filter();

        void SetCredential(_In_ Communication::SavedCredential* Credential) noexcept;
        bool Authenticate(_In_ Communication::Credential& Credential) noexcept;

        NTSTATUS OpenPort(_In_ UNICODE_STRING* PortName);

        NTSTATUS OnConnect(_In_ PFLT_PORT ClientPort);
        void OnDisconnect(_In_ PFLT_PORT ClientPort);
    };
}

namespace MiniFilter
{
    class Port : public failable_object<NTSTATUS>
    {
    private:
        PFLT_PORT _port = NULL;
    public:
        Port(_In_ PFLT_FILTER Filter, _In_ UNICODE_STRING* PortName, _In_ MiniFilter::Filter* Cookie);
        Port(Port&& Other);
        ~Port();
    };
}

namespace MiniFilter
{
    class Connection
    {
    private:
        PFLT_FILTER _filter = NULL;
        PFLT_PORT _port = NULL;
    public:
        Connection(_In_ PFLT_FILTER Filter,_In_ PFLT_PORT Port);
        Connection(Connection&& Other) noexcept;
        ~Connection();

        inline bool operator==(PFLT_PORT Port) const noexcept { return _port == Port; }
    };
}