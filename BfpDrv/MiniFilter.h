#pragma once
#include <fltKernel.h>

#include <Shared.h>

class MiniFilter : public failable_object<NTSTATUS>
{
private:
    PFLT_FILTER _filter = NULL;
    PFLT_PORT _port = NULL;

public:
    MiniFilter(
        _In_ DRIVER_OBJECT* DriverObject,
        _In_ UNICODE_STRING* ComportName);
    ~MiniFilter();

    const PFLT_FILTER& GetFilter() const;
};