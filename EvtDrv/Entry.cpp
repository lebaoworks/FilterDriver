/********************
*     Includes      *
********************/

#include <fltKernel.h>

// Logging vie tracing
#include "trace.h"
#include "Entry.tmh"

// Utilities
#include "krn.hpp"

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

// Free up the memory taken by DriverEntry after initialization
#pragma alloc_text (INIT, DriverEntry)

/*********************
*     Global Vars    *
*********************/

#pragma data_seg("NONPAGED")
static MiniFilter::Filter* g_MiniFilter = nullptr;
static MiniFilter::Port* g_MiniPort = nullptr;
#pragma data_seg()

/*********************
*   Implementations  *
*********************/

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

    // Create Worker object
    {
        status = Worker::Initialize();
        if (status != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Initialized Worker -> status: %!STATUS!", status);
            return status;
        };
    }
    defer{ if (status != STATUS_SUCCESS) { Worker::Uninitialize(); } };
	
    // Create the MiniFilter object
    {
        auto result = krn::make<MiniFilter::Filter>(DriverObject);
        status = result.status();
        if (status != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Initialized MiniFilter -> status: %!STATUS!", status);
            WPP_CLEANUP(DriverObject);
            return status;
        };
        g_MiniFilter = result.release();
    }
	defer{ if (status != STATUS_SUCCESS) { delete g_MiniFilter; g_MiniFilter = nullptr; } };

	// Create MiniPort object
    {
		UNICODE_STRING port_name = RTL_CONSTANT_STRING(L"\\EvtDrvPort");
		auto result = krn::make<MiniFilter::Port>(*g_MiniFilter, &port_name, Worker::ConnectNotify);
		status = result.status();
        if (status != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Initialized MiniPort -> status: %!STATUS!", status);
            WPP_CLEANUP(DriverObject);
            return status;
        };
        g_MiniPort = result.release();
    }
    defer{ if (status != STATUS_SUCCESS) { delete g_MiniPort; g_MiniPort = nullptr; } };


    return STATUS_SUCCESS;
}

_Function_class_(DRIVER_UNLOAD)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID DriverUnload(_In_ DRIVER_OBJECT* DriverObject)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Unloading");

    // Stop worker thread and cleanup
    Worker::Uninitialize();

    // Close the MiniPort
    delete g_MiniPort;

    // Delete the MiniFilter object
    delete g_MiniFilter;

	// Clean up WPP Tracing
    WPP_CLEANUP(DriverObject);
}