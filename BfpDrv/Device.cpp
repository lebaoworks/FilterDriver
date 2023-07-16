#include "Device.h"
#include <fltKernel.h>

#include <Shared.h>

/* Static data declarations */
static UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\DosDevices\\BfpDevice");
static PDEVICE_OBJECT DeviceObject = NULL;

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS Device::Install(_In_ DRIVER_OBJECT* DriverObject)
{
    return IoCreateDevice(
        DriverObject,           // DriverObject
        0,                      // DeviceExtensionSize
        &DeviceName,            // DeviceName
        FILE_DEVICE_UNKNOWN,    // DeviceType
        0,                      // DeviceCharacteristics
        FALSE,                  // Exclusive
        &DeviceObject);         // DeviceObject
}

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
void Device::Uninstall()
{
    IoDeleteDevice(DeviceObject);
}