#pragma once

/********************
*     Includes      *
********************/

#include <krn.hpp>

// Events
#include "Event.hpp"

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
    class Filter : public krn::failable, public krn::tag<'EVT0'>
    {
        friend class MiniFilter::Port;
    public:

    private:
        PFLT_FILTER _filter = NULL;
        

    public:

        /// @brief Sets up the filter and starts filtering.
        /// @param DriverObject 
        /// @param Callback The callback function that will be called when an event is captured by the filter.
        /// @remarks On success, status() will be STATUS_SUCCESS.
        /// @remarks On failure, status() will be set to the error code, rewind any partial initialization.
        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        Filter(
            _In_ DRIVER_OBJECT* DriverObject,
            _In_ Event::EventNotifyCallback Callback
        );
        
        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        ~Filter();
    };
}

namespace MiniFilter
{
    class Port : public krn::failable, public krn::tag<'EVT0'>
    {
    public:
        using ConnectNotifyCallback = NTSTATUS(*)(krn::unique_ptr<Connection>&);

    private:
        PFLT_PORT _port = NULL;
        PVOID     _cookie = nullptr;

    public:
        /// @brief Creates a communication port for the filter.
        /// @param Filter The filter to which the port will be attached.
        /// @param PortName The name of the port, which will be used by user-mode applications to connect to it.
        /// @remarks On success, status will be STATUS_SUCCESS.
        /// @remarks On failure, status will be set to the error code, rewind any partial initialization.
        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        Port(
            _In_ const Filter&          Filter,
            _In_ UNICODE_STRING*        PortName,
            _In_ ConnectNotifyCallback  ConnectNotifyCallback
        ) noexcept;

        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        ~Port();
    };

    class Connection : public krn::tag<'EVT0'>
    {
    private:
        PFLT_FILTER _filter = NULL;
        PFLT_PORT   _port   = NULL;

    public:
        /// @brief Wraps a MiniPort connection
        /// @param Filter The filter to which the port is attached.
        /// @param ClientPort The port representing the connection to the user-mode application.
        Connection(
            _In_ PFLT_FILTER Filter,
            _In_ PFLT_PORT ClientPort
        ) noexcept;
        ~Connection();

        /// @brief Send data to the connected user-mode application.
        /// @param Buffer The buffer containing the data to send. 
        /// @param BufferSize The size of the buffer in bytes.
        /// @return [TODO]
        NTSTATUS Send(
            _In_reads_bytes_(BufferSize) PVOID Buffer,
            _In_ ULONG BufferSize
        ) noexcept;
    };
}
