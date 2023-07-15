#pragma once
#include <fltKernel.h>

namespace Device
{
    _IRQL_requires_same_
    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS Install(_In_ DRIVER_OBJECT* DriverObject);

    _IRQL_requires_same_
    _IRQL_requires_(PASSIVE_LEVEL)
    void Uninstall();
};