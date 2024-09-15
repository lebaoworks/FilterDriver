#include "Driver.h"

Driver::Driver(
    _Inout_ DRIVER_OBJECT*  DriverObject,
    _In_    UNICODE_STRING* RegistryPath)
{
    Log("Init Driver: [DriverObject: %p] [RegistryPath: %wZ]", DriverObject, RegistryPath);

    auto& status = failable::_status;

    auto result = krn::make<MiniFilter::Filter>(DriverObject);
    if (result.error() == true)
    {
        status = result.status();
        LogError("Make Filter -> status: %X", status);
        return;
    }
    _filter = result.release();

    Log("Done!");
}

Driver::~Driver()
{
    LogDebug("Cleanup Driver...");

    if (this->status() != STATUS_SUCCESS)
    {
        Log("Skip!");
        return;
    }

    delete _filter;

    Log("Done!");
}