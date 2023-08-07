#include <fltKernel.h>

#include <Shared.h>
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
    //DriverObject->MajorFunction[IRP_MJ_CREATE] = DefaultDispatch;
    //DriverObject->MajorFunction[IRP_MJ_CLOSE] = DefaultDispatch;

    _Driver = new Driver(DriverObject, RegistryPath, &_ComportName);
    if (_Driver == nullptr)
        return STATUS_NO_MEMORY;

    auto ret = _Driver->error();
    Log("Setup Driver -> Status: %X", ret);
    if (ret != STATUS_SUCCESS)
    {
        delete _Driver;
        return ret;
    }

    // test
    {
        UNICODE_STRING SymbolicPath = RTL_CONSTANT_STRING(LR"(\??\C:\Users\bao\source\repos\BfpDrv\x64\Debug)");
        auto buffer = new WCHAR[1024];
        if (buffer == nullptr)
        {
            Log("Failed to allocate memory");
            return STATUS_NO_MEMORY;
        }
        defer{ delete[] buffer; };
        auto TargetPath = reinterpret_cast<PUNICODE_STRING>(buffer);
        TargetPath->Length = 0;
        TargetPath->MaximumLength = 2000;
        TargetPath->Buffer = buffer + sizeof(UNICODE_STRING) / sizeof(WCHAR);

        Win32::Path::QueryAbsoluteTarget(&SymbolicPath, TargetPath);
        /*if (NT_SUCCESS(status))
            Log("Target: %wZ", TargetPath);
        else
            Log("Failed to query target: %X", status);*/
    }
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

//_Function_class_(DRIVER_DISPATCH)
//_IRQL_requires_max_(DISPATCH_LEVEL)
//_IRQL_requires_same_
//NTSTATUS DefaultDispatch(
//    _In_ struct _DEVICE_OBJECT* DeviceObject,
//    _Inout_ struct _IRP* Irp)
//{
//    UNREFERENCED_PARAMETER(DeviceObject);
//    Irp->IoStatus.Status = STATUS_SUCCESS;
//    Irp->IoStatus.Information = 0;
//    IoCompleteRequest(Irp, IO_NO_INCREMENT);
//    return STATUS_SUCCESS;
//}
