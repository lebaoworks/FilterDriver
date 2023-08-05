#pragma once

#include <fltKernel.h>
#include <Shared.h>

// Forward declarations
namespace MiniFilter
{
    struct Context;
    class Filter;
    class Comport;
    class Connection;
}

namespace MiniFilter
{
    class Filter : public failable_object<NTSTATUS>
    {
    private:
        PFLT_FILTER _filter = NULL;
        //std::map<UNICODE_STRING, Comport> Comports;
    public:
        Filter(_In_ DRIVER_OBJECT* DriverObject);
        ~Filter();
    };
}
