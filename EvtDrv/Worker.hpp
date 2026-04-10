#pragma once

/********************
*     Includes      *
********************/

#include <fltKernel.h>
#include <krn.hpp>
#include <c++/queue.hpp>

#include "Event.hpp"
#include "MiniFilter.hpp"

/*********************
*    Declarations    *
*********************/

namespace Worker
{
    /// @brief Initializes the worker components
    /// @return STATUS_SUCCESS if initialization was successful, otherwise an error code.
    /// @remarks Calling in DriverEntry only.
    /// @remarks If successful, should be paired with a call to Uninitialize() in DriverUnload.
    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    NTSTATUS Initialize() noexcept;

    /// @brief Uninitializes the worker components
    /// @remarks Calling in DriverUnload only.
    /// @remarks Only call after a successful call to Initialize() in DriverEntry.
    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    void Uninitialize() noexcept;


    /// @brief Push an event to the worker.
    /// @param event The event to push.
    /// @return STATUS_SUCCESS if the event was successfully pushed, otherwise an error code.
    NTSTATUS Push(_Inout_ std::unique_ptr<Event>& event) noexcept;


    /// @brief Notifies the worker of a new connection.
    /// @param connection The connection to notify the worker about.
    /// @return STATUS_SUCCESS if the notification was successful, otherwise an error code.
    NTSTATUS ConnectNotify(_Inout_ std::unique_ptr<MiniFilter::Connection>& connection);
}