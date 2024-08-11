#pragma once

#include <fltKernel.h>

#include <base.h>

// Forward declarations
namespace MiniFilter
{
    class Filter;
    class Port;
    class Connection;
    class Authenticator;
}

namespace MiniFilter
{
    class Filter : public failable
    {
    private:
        PFLT_FILTER _filter = NULL;

    public:
        Filter(_In_ DRIVER_OBJECT* DriverObject);
        ~Filter();
    };
}