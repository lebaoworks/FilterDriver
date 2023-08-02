#include "Driver.h"

#include <Shared.h>
#include <Communication.h>

Driver::Driver(
    _Inout_ DRIVER_OBJECT*  DriverObject,
    _In_    UNICODE_STRING* RegistryPath,
    _In_    UNICODE_STRING* ComportName) :
    _minifilter(DriverObject, ComportName)
{
    Log("Setup DriverObject: %p, RegistryPath: %wZ, ComportName: %wZ", DriverObject, RegistryPath, ComportName);

    auto status = STATUS_SUCCESS;
    defer{ failable_object<NTSTATUS>::_error = status; }; // set error code
    
    if (_minifilter.error() != STATUS_SUCCESS)
    {
        status = _minifilter.error();
        Log("Setup MiniFilter failed: %X", status);
        return;
    }
}

Driver::~Driver()
{
    Log("Initialized: %s", error() == STATUS_SUCCESS ? "true" : "false");
}