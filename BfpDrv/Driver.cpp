#include <fltKernel.h>
#include <Shared.h>

#include "Device.h"
#include "MiniFilter.h"
#include <Control.h>


#ifdef __cplusplus
extern "C" { DRIVER_INITIALIZE DriverEntry; }
#endif
DRIVER_UNLOAD DriverUnload;
DRIVER_DISPATCH DefaultDispatch;
DRIVER_DISPATCH DeviceControlDispatch;

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

_Function_class_(DRIVER_INITIALIZE)
_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS DriverEntry(
    _In_ DRIVER_OBJECT* DriverObject,
    _In_ UNICODE_STRING* RegistryPath)
{
    Log("Initializing... %wZ", RegistryPath);
    NTSTATUS status = STATUS_SUCCESS;

    // Set up the unload routine
    DriverObject->DriverUnload = DriverUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = DefaultDispatch;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DefaultDispatch;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControlDispatch;

    // Install device
    status = Device::Install(DriverObject);
    Log("Install Device -> Status: %X", status);
    if (status != STATUS_SUCCESS)
        return status;
    defer{ if (status != STATUS_SUCCESS) Device::Uninstall(); }; // Rollback

    // Install the mini-filter
    status = MiniFilter::Install(DriverObject);
    Log("Install MiniFilter -> Status: %X", status);
    if (status != STATUS_SUCCESS)
        return status;
    defer{ if (status != STATUS_SUCCESS) MiniFilter::Uninstall(); }; // Rollback

    return STATUS_SUCCESS;
}

_Function_class_(DRIVER_UNLOAD)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID DriverUnload(_In_ DRIVER_OBJECT* DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    Log("Uninstalling...");

    // Uninstall device
    Device::Uninstall();
    Log("Uninstalled Device");

    // Uninstall the mini-filter
    MiniFilter::Uninstall();
    Log("Uninstalled MiniFilter");
    
    Log("Done");
}

_Function_class_(DRIVER_DISPATCH)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
NTSTATUS DefaultDispatch(
    _In_ struct _DEVICE_OBJECT* DeviceObject,
    _Inout_ struct _IRP* Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

_Function_class_(DRIVER_DISPATCH)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
NTSTATUS DeviceControlDispatch(
    _In_ struct _DEVICE_OBJECT* DeviceObject,
    _Inout_ struct _IRP* Irp)
{
    UNREFERENCED_PARAMETER(DeviceObject);
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode)
    {
    case IOCTL_BFP_CONTROL:
    {
        Log("IOCTL_BFP_CONTROL");

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength != 1)
        {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        //auto InputBuffer = Irp->AssociatedIrp.SystemBuffer;
        break;
    }
    default:
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}