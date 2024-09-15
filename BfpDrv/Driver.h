#pragma once

#include <fltKernel.h>

#include <krn.hpp>
#include "MiniFilter.h"

class Driver : public krn::failable
{
private:
    MiniFilter::Filter* _filter;
public:
    Driver(
        _Inout_ DRIVER_OBJECT*  DriverObject,
        _In_    UNICODE_STRING* RegistryPath);
    ~Driver();
};

