#pragma once

/********************
*     Includes      *
********************/

#include <fltKernel.h>
#include <krn.hpp>
#include <c++/list.hpp>

/*********************
*    Declarations    *
*********************/

namespace MiniFilter
{
    class Filter;
    class Port;
    class Connection;
}

namespace MiniFilter
{
    class Filter : public krn::failable
    {
        friend class MiniFilter::Port;

    private:
        PFLT_FILTER _filter = NULL;

        // Keeps track of active connections to the filter here.
        //      as they may persist even after the port is closed.
        // FltUnregisterFilter() will wait for all connections to be closed before it returns.
        //std::list<MiniFilter::Connection> _connections;

    public:
        /// @brief Sets up the filter and starts filtering.
        /// @param DriverObject 
        /// @remarks On success, status will be STATUS_SUCCESS.
        /// @remarks On failure, status will be set to the error code, rewind any partial initialization.
        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        Filter(_In_ DRIVER_OBJECT* DriverObject);
        
        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        ~Filter();

        NTSTATUS OnClientConnect(_In_ PFLT_PORT ClientPort);
        void     OnClientDisconnect(_Inout_ PFLT_PORT ClientPort);
    };
}

namespace MiniFilter
{
    class Port : public krn::failable
    {
    private:
        PFLT_PORT _port = NULL;

    public:
        /// @brief Creates a communication port for the filter.
        /// @param Filter The filter to which the port will be attached.
        /// @param PortName The name of the port, which will be used by user-mode applications to connect to it.
        /// @remarks On success, status will be STATUS_SUCCESS.
        /// @remarks On failure, status will be set to the error code, rewind any partial initialization.
        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        Port(
            _Inout_ MiniFilter::Filter& Filter,
            _In_    UNICODE_STRING* PortName
        ) noexcept;

        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        ~Port() noexcept;
    };

    /*class Connection
    {
    private:
        PFLT_FILTER _filter = NULL;
        PFLT_PORT _port = NULL;

    public:
        Connection(_In_ PFLT_FILTER Filter, _In_ PFLT_PORT Port) noexcept;
        Connection(Connection&& Other) noexcept;
        ~Connection() noexcept;

        inline bool operator==(PFLT_PORT Port) const noexcept { return _port == Port; }
    };*/
}
