#pragma once
#include <fltKernel.h>

namespace MiniFilter
{
    NTSTATUS Install(_In_ DRIVER_OBJECT* DriverObject);
    void Uninstall();
};