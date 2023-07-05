#include <fltKernel.h>
#include <Shared.h>

#include "MiniFilter.h"

#ifdef __cplusplus
extern "C" {
    NTSTATUS DriverEntry(
        _In_ DRIVER_OBJECT* DriverObject,
        _In_ UNICODE_STRING* RegistryPath);
    
    _Function_class_(DRIVER_UNLOAD)
    VOID DriverUnload(_In_ DRIVER_OBJECT* DriverObject);
}

#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

NTSTATUS DriverEntry(
    _In_ DRIVER_OBJECT* DriverObject,
    _In_ UNICODE_STRING* RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    Log("Initializing...");

    // Set up the unload routine
    DriverObject->DriverUnload = DriverUnload;

    // Install the mini-filter
    auto status = MiniFilter::Install(DriverObject);
    Log("Install MiniFilter -> Status: %X", status);

    return status;
}

_Function_class_(DRIVER_UNLOAD)
VOID DriverUnload(_In_ DRIVER_OBJECT* DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    Log("Uninstalling");

    // Uninstall the mini-filter
    MiniFilter::Uninstall();
    
    Log("Done");
}
