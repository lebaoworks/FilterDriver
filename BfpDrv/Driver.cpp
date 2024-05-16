#include "Driver.h"

Driver::Driver(
    _Inout_ DRIVER_OBJECT*  DriverObject,
    _In_    UNICODE_STRING* RegistryPath,
    _In_    UNICODE_STRING* ComportName)
{
    Log("Setup DriverObject: %p, RegistryPath: %wZ, ComportName: %wZ", DriverObject, RegistryPath, ComportName);

    //auto status = STATUS_SUCCESS;
    //defer{ failable_object<NTSTATUS>::_error = status; }; // set error code

    //_filter = std::make_unique<MiniFilter::Filter>(DriverObject);
    //if (_filter->error() != STATUS_SUCCESS)
    //{
    //    status = _filter->error();
    //    Log("Setup MiniFilter failed: %X", status);
    //    return;
    //}
    //defer{ if (status != STATUS_SUCCESS) _filter.reset(nullptr); }; // Rollback

    //status = _filter->OpenPort(ComportName);
    //if (status != STATUS_SUCCESS)
    //{
    //    Log("OpenPort failed: %X", status);
    //    return;
    //}

    //status = _filter->InitAuthenticator();
    //if (status != STATUS_SUCCESS)
    //{
    //    Log("InitAuthenticator failed: %X", status);
    //    return;
    //}
}

Driver::~Driver()
{
    if (failable::status() != STATUS_SUCCESS)
        return;

    //_filter.reset(nullptr);
}