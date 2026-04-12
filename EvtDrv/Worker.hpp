#pragma once

/********************
*     Includes      *
********************/

#include <krn.hpp>
#include <queue.hpp>

#include "Event.hpp"
#include "MiniFilter.hpp"

/*********************
*    Declarations    *
*********************/

namespace Worker
{
    class Worker;
    class Queue;
}

namespace Worker
{
    class Queue : public krn::failable, public krn::tag<'EVT0'>
    {
        friend class Worker;

    private:
        krn::queue<krn::unique_ptr<Event::Event>, krn::tag<'EVT0'>> _events;
        ERESOURCE _lock;
        KEVENT _push_event;

    public:
        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        Queue() noexcept;
            
        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        ~Queue();

        /// @brief Push an event to the queue.
        /// @param event The event to push.
        /// @return STATUS_SUCCESS if the event was successfully pushed, otherwise an error code.
        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        NTSTATUS Push(_Inout_ krn::unique_ptr<Event::Event>& event) noexcept;
    };
}

namespace Worker
{
    class Worker : public krn::failable, public krn::tag<'EVT0'>
    {
    private:
        // Buffer for serializing events before sending to user-mode
        //PVOID _serialized_buffer;

        // Current active connection to the filter (if any)
        krn::unique_ptr<MiniFilter::Connection> _connection;
        //static ERESOURCE ConnectionLock;
        //static KEVENT ConnectEvent;

    public:

        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        Worker() noexcept;
        
        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        ~Worker();
        
        /// @brief Notifies the worker of a new connection.
        /// @param connection The connection to notify the worker about.
        /// @return STATUS_SUCCESS if the notification was successful, otherwise an error code.
        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        NTSTATUS ConnectNotify(_Inout_ krn::unique_ptr<MiniFilter::Connection>& connection) noexcept;
    };
}