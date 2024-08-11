
#include <fltKernel.h>

#include <base.h>

#include "Driver.h"

/*********************
*     Declaration    *
*********************/

#ifdef __cplusplus
extern "C" { DRIVER_INITIALIZE DriverEntry; }
#endif
static DRIVER_UNLOAD DriverUnload;

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

/*********************
*     Global Vars    *
*********************/

static Driver* _Driver;


/*********************
*   Implementation   *
*********************/

#ifndef _TEST

_Function_class_(DRIVER_INITIALIZE)
_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS DriverEntry(
    _In_ DRIVER_OBJECT* DriverObject,
    _In_ UNICODE_STRING* RegistryPath)
{
    Log("Loading %wZ", RegistryPath);

    // Set up the unload routine
    DriverObject->DriverUnload = DriverUnload;

    auto result = base::make<Driver>(DriverObject, RegistryPath);
    auto status = result.error();
    if (status != STATUS_SUCCESS)
    {
        LogError("Load driver -> status: %X", status);
        return status;
    };
    _Driver = result.release();

    Log("Done!");
    return STATUS_SUCCESS;
}

_Function_class_(DRIVER_UNLOAD)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID DriverUnload(_In_ DRIVER_OBJECT* DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    Log("Unloading...");

    delete _Driver;

    Log("Done!");
}

#endif

/*********************
*        Test        *
*********************/

#ifdef _TEST

#include <krn.h>

_Function_class_(DRIVER_INITIALIZE)
_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS DriverEntry(
    _In_ DRIVER_OBJECT* DriverObject,
    _In_ UNICODE_STRING* RegistryPath)
{
    Log("Test %wZ", RegistryPath);

    // Set up the unload routine
    DriverObject->DriverUnload = DriverUnload;

    if (auto status = base::test(); status != STATUS_SUCCESS)
        LogError("[!] Test base error: %X", status);
    else
        Log("[+] Test base success");
    //krn::test();

    return STATUS_SUCCESS;
}

_Function_class_(DRIVER_UNLOAD)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID DriverUnload(_In_ DRIVER_OBJECT* DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
}

#endif