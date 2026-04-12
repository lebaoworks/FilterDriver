/********************
*     Includes      *
********************/

#include "krn.hpp"

// Logging vie tracing
#include "trace.h"
#include "Entry.tmh"

// Functional
#include "MiniFilter.hpp"
#include "Worker.hpp"

/*********************
*    Declarations    *
*********************/
#ifndef ALLOC_PRAGMA
#error "ALLOC_PRAGMA must be defined to compile this driver."
#endif

#ifdef __cplusplus
extern "C" { DRIVER_INITIALIZE DriverEntry; }
#endif

static DRIVER_UNLOAD DriverUnload;

struct Driver;

// Free up the memory taken by DriverEntry after initialization
#pragma alloc_text (INIT, DriverEntry)

/*********************
*     Global Vars    *
*********************/

#pragma data_seg("NONPAGED")
static Driver* GlobalDriver = nullptr;
#pragma data_seg()

/*********************
*   Implementations  *
*********************/

struct Driver : public krn::failable, public krn::tag<'EVT0'>
{
private:
    MiniFilter::Filter* _filter = nullptr;
    MiniFilter::Port*   _port   = nullptr;
    Worker::Queue*      _queue  = nullptr;
    Worker::Worker*     _worker = nullptr;
    

public:
    Driver(DRIVER_OBJECT* DriverObject) noexcept
    {
        NTSTATUS& status = krn::failable::_status;

        // Initialize Queue
        {
            auto result = krn::make<Worker::Queue>();
            status = result.status();
            if (status != STATUS_SUCCESS)
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Initialized Queue -> status: %!STATUS!", status);
                return;
            }
            _queue = result.release();
        }
        defer{ if (status != STATUS_SUCCESS) { delete _queue; _queue = nullptr; } };
        
        // Initialize Worker
        {
            auto result = krn::make<Worker::Worker>();
            status = result.status();
            if (status != STATUS_SUCCESS)
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Initialized Worker -> status: %!STATUS!", status);
                return;
            }
            _worker = result.release();
        }
        defer{ if (status != STATUS_SUCCESS) { delete _worker; _worker = nullptr; } };

        // Initialize MiniFilter
        {
            auto result = krn::make<MiniFilter::Filter>(DriverObject, [](krn::unique_ptr<Event::Event>& event) -> NTSTATUS {
                // [UPGRADABLE] _queue pointer can be stored in global to reduce the number of deferences when invoking callback from 2 to 1.
                return GlobalDriver->_queue->Push(event);
            });
            status = result.status();
            if (status != STATUS_SUCCESS)
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Initialized MiniFilter -> status: %!STATUS!", status);
                return;
            }
            _filter = result.release();
        }
        defer{ if (status != STATUS_SUCCESS) { delete _filter; _filter = nullptr; } };

        // Create MiniPort object
        {
   	        UNICODE_STRING port_name = RTL_CONSTANT_STRING(L"\\EvtDrvPort");
   	        auto result = krn::make<MiniFilter::Port>(*_filter, &port_name, [](krn::unique_ptr<MiniFilter::Connection>& conn) -> NTSTATUS {
                // [UPGRADABLE] _worker pointer can be stored in global to reduce the number of deferences when invoking callback from 2 to 1.
                return GlobalDriver->_worker->ConnectNotify(conn);
            });
   	        status = result.status();
            if (status != STATUS_SUCCESS)
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Initialized MiniPort -> status: %!STATUS!", status);
                return;
            };
            _port = result.release();
        }
        defer{ if (status != STATUS_SUCCESS) { delete _port; _port = nullptr; } };
    }

    ~Driver()
    {
        if (failable::status() != STATUS_SUCCESS)
            return;

        delete _port;   // Stop accepting new connections
        delete _worker; // Stop worker thread and cleanup existing connection
        delete _filter; // Close the filter => stop accepting new events
        delete _queue;  // Cleanup event queue
    }
};

_Function_class_(DRIVER_INITIALIZE)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS DriverEntry(
    _In_ DRIVER_OBJECT* DriverObject,
    _In_ UNICODE_STRING* RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

    // Declare status variable for the initialization process
    NTSTATUS status = STATUS_SUCCESS;

    //
	// Initialize essential driver components
    //

    // Initialize WPP Tracing
    WPP_INIT_TRACING(DriverObject, RegistryPath);
    defer{ if (status != STATUS_SUCCESS) { WPP_CLEANUP(DriverObject); } };

    // Allow driver unload
    DriverObject->DriverUnload = DriverUnload;

    //
	// Initialize functional components
    //

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Initializing");

    auto result = krn::make<Driver>(DriverObject);
    if (result.status() != STATUS_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Initialized Driver -> status: %!STATUS!", result.status());
        return result.status();
    }
    GlobalDriver = result.release();

    return STATUS_SUCCESS;
}

_Function_class_(DRIVER_UNLOAD)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID DriverUnload(_In_ DRIVER_OBJECT* DriverObject)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Unloading");

    // Clean up functional components
    delete GlobalDriver;

	// Clean up WPP Tracing
    WPP_CLEANUP(DriverObject);
}