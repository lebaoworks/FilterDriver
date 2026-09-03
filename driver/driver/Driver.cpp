// Traditional WDM driver: exposes \\.\KFilter so kfilter-compiler can push
// a freshly-built ruleset via IOCTL_KFILTER_LOAD_RULES. No event capture
// (ObRegisterCallbacks / minifilter / process-notify routines) yet -- that's
// the next step once rule delivery is proven end to end.
#include <ntddk.h>
#include <wdmsec.h>
#include "kfilter_core.h"

DRIVER_UNLOAD KfilterUnload;
DRIVER_DISPATCH KfilterCreateClose;
DRIVER_DISPATCH KfilterDeviceControl;

VOID
KfilterUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    UNICODE_STRING symlinkName;
    RtlInitUnicodeString(&symlinkName, KFILTER_SYMLINK_NAME);
    IoDeleteSymbolicLink(&symlinkName);

    if (DriverObject->DeviceObject != nullptr) {
        IoDeleteDevice(DriverObject->DeviceObject);
    }

    kfilter_unload();

    DbgPrint("kfilterdrv: unloading\n");
}

_Use_decl_annotations_
NTSTATUS
KfilterCreateClose(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
KfilterDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

    switch (stack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_KFILTER_LOAD_RULES: {
        const UCHAR* buf = static_cast<const UCHAR*>(Irp->AssociatedIrp.SystemBuffer);
        SIZE_T len = stack->Parameters.DeviceIoControl.InputBufferLength;

        LONG rc = kfilter_load(buf, len);
        status = (rc == 0) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
        DbgPrint("kfilterdrv: kfilter_load(len=%Iu) rc=%ld\n", len, rc);
        break;
    }
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

extern "C" NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    kfilter_init();

    DriverObject->DriverUnload = KfilterUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = KfilterCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = KfilterCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KfilterDeviceControl;

    UNICODE_STRING deviceName;
    RtlInitUnicodeString(&deviceName, KFILTER_DEVICE_NAME);

    PDEVICE_OBJECT deviceObject = nullptr;
    // Restrict the device to SYSTEM + Administrators: this IOCTL replaces
    // the active filter ruleset, so it must not be reachable by an
    // unprivileged process.
    NTSTATUS status = IoCreateDeviceSecure(
        DriverObject,
        0,
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &SDDL_DEVOBJ_SYS_ALL_ADM_ALL,
        nullptr,
        &deviceObject
    );
    if (!NT_SUCCESS(status)) {
        DbgPrint("kfilterdrv: IoCreateDeviceSecure failed 0x%x\n", status);
        return status;
    }

    UNICODE_STRING symlinkName;
    RtlInitUnicodeString(&symlinkName, KFILTER_SYMLINK_NAME);
    status = IoCreateSymbolicLink(&symlinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("kfilterdrv: IoCreateSymbolicLink failed 0x%x\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }

    DbgPrint("kfilterdrv: loaded, device \\\\.\\KFilter ready\n");
    return STATUS_SUCCESS;
}
