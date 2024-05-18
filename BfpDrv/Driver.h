#pragma once

#include <fltKernel.h>

#include <base.h>
#include "MiniFilter.h"

class Driver : public failable
{
private:
    object<MiniFilter::Filter> _filter;
public:
    Driver(
        _Inout_ DRIVER_OBJECT*  DriverObject,
        _In_    UNICODE_STRING* RegistryPath,
        _In_    UNICODE_STRING* ComportName);
    ~Driver();
};

