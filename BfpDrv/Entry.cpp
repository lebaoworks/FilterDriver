#include <fltKernel.h>

#include <base.h>
#include <Win32.h>
#include <Communication.h>

#include "Driver.h"

/*********************
*     Declaration    *
*********************/

#ifdef __cplusplus
extern "C" { DRIVER_INITIALIZE DriverEntry; }
#endif
DRIVER_UNLOAD DriverUnload;
//DRIVER_DISPATCH DefaultDispatch;

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

/*********************
*     Global Vars    *
*********************/

static Driver* _Driver = nullptr;
static UNICODE_STRING _ComportName = RTL_CONSTANT_STRING(COMPORT_NAME);


/*********************
*   Implementation   *
*********************/

_Function_class_(DRIVER_INITIALIZE)
_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS DriverEntry(
    _In_ DRIVER_OBJECT* DriverObject,
    _In_ UNICODE_STRING* RegistryPath)
{
    Log("Initializing... %wZ", RegistryPath);

    // Set up the unload routine
    DriverObject->DriverUnload = DriverUnload;

    _Driver = new Driver(DriverObject, RegistryPath, &_ComportName);
    if (_Driver == nullptr)
        return STATUS_NO_MEMORY;

    auto ret = _Driver->status();
    Log("Setup Driver -> Status: %X", ret);
    if (ret != STATUS_SUCCESS)
    {
        delete _Driver;
        return ret;
    }

    DriverObject->DriverUnload = nullptr;

    base::object_failable<Driver> x;

    return STATUS_SUCCESS;
}

_Function_class_(DRIVER_UNLOAD)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID DriverUnload(_In_ DRIVER_OBJECT* DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    Log("Uninstalling...");

    if (_Driver != nullptr)
        delete _Driver;

    Log("Done");
}