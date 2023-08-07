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
    class Authenticator;
}

namespace MiniFilter
{
    class Filter : public failable_object<NTSTATUS>
    {
    private:
        PFLT_FILTER _filter = NULL;
        
        std::unique_ptr<Authenticator> _authenticator;

        std::list<Port> _ports;
        std::list<Connection> _connections;
        eresource_lock _connections_lock;

    public:
        Filter(_In_ DRIVER_OBJECT* DriverObject);
        ~Filter();

        NTSTATUS InitAuthenticator();

        NTSTATUS OpenPort(_In_ UNICODE_STRING* PortName);
        NTSTATUS OnConnect(_In_ PFLT_PORT ClientPort, _In_ const Communication::Credential& Credential);
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
        Port(_In_ PFLT_FILTER Filter, _In_ UNICODE_STRING* PortName, _In_ MiniFilter::Filter* Cookie) noexcept;
        Port(Port&& Other) noexcept;
        ~Port() noexcept;
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
        Connection(_In_ PFLT_FILTER Filter,_In_ PFLT_PORT Port) noexcept;
        Connection(Connection&& Other) noexcept;
        ~Connection() noexcept;

        inline bool operator==(PFLT_PORT Port) const noexcept { return _port == Port; }
    };
}

namespace MiniFilter
{
    class Authenticator : public failable_object<NTSTATUS>
    {
    private:
        unsigned char _credential[32];

        NTSTATUS InitCredential() noexcept;
    public:
        Authenticator(_In_opt_ UNICODE_STRING* RegistryPath) noexcept;
        ~Authenticator() noexcept;

        bool Authenticate(_In_ const Communication::Credential& Credential) const noexcept;
    };
}

namespace MiniFilter::Utils
{
    union FILE_ID
    {
        struct
        {
            UINT64 Value;
            UINT64 Reserved;
        } FileId64;
        FILE_ID_128 FileId128;
    };

    // Get the file ID for the given file object. Does not work in PRE_CREATE callback.
    _IRQL_requires_(PASSIVE_LEVEL) NTSTATUS GetFileId(_In_ PFLT_INSTANCE Instance, _In_ PFILE_OBJECT FileObject, _Out_ FILE_ID& FileId);
}
