#pragma once

/********************
*     Includes      *
********************/

#include <krn.hpp>

// Events
#include "Event.hpp"

namespace WPF
{
    class Monitor : public krn::failable, public krn::tag<'EVT0'>
    {
    public:
        /// @brief Sets up the filter and starts filtering.
        /// @param Callback The callback that will be called for each event captured by the monitor.
        /// @remarks On success, status() will be STATUS_SUCCESS.
        /// @remarks On failure, status() will be set to the error code, rewind any partial initialization.
        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        Monitor(
            _In_ DRIVER_OBJECT* DriverObject,
            _In_ Event::EventNotifyCallback Callback
        ) noexcept;

        _IRQL_requires_(PASSIVE_LEVEL)
        _IRQL_requires_same_
        ~Monitor();
    };
}