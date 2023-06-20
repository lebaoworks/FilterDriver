#pragma once


namespace MiniFilter
{
    bool Install(_In_ DRIVER_OBJECT* DriverObject);
    void Uninstall();
};