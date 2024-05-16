#pragma once

#include <fltKernel.h>

#include <base.h>
//#include "MiniFilter.h"

class Driver : public base::failable
{
private:
    //std::unique_ptr<MiniFilter::Filter> _filter;
public:
    Driver(
        _Inout_ DRIVER_OBJECT*  DriverObject,
        _In_    UNICODE_STRING* RegistryPath,
        _In_    UNICODE_STRING* ComportName);
    ~Driver();
};

