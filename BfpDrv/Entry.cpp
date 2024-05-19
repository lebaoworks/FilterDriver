
#include <fltKernel.h>

#include <base.h>
#include <krn.h>
#include <Communication.h>

#include "Driver.h"

/*********************
*     Declaration    *
*********************/

#ifdef __cplusplus
extern "C" { DRIVER_INITIALIZE DriverEntry; }
#endif
static DRIVER_UNLOAD DriverUnload;

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

/*********************
*     Global Vars    *
*********************/

static Driver* _Driver;
static UNICODE_STRING _ComportName = RTL_CONSTANT_STRING(COMPORT_NAME);


/*********************
*   Implementation   *
*********************/
#define _TEST

_Function_class_(DRIVER_INITIALIZE)
_IRQL_requires_same_
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS DriverEntry(
    _In_ DRIVER_OBJECT* DriverObject,
    _In_ UNICODE_STRING* RegistryPath)
{
#ifndef _TEST
    Log("Loading... %wZ", RegistryPath);

    // Set up the unload routine
    DriverObject->DriverUnload = DriverUnload;

    NTSTATUS status = STATUS_SUCCESS;

    _Driver = new Driver(DriverObject, RegistryPath, &_ComportName);
    if (_Driver == nullptr)
        return STATUS_NO_MEMORY;
    defer { if (status != STATUS_SUCCESS) delete _Driver; };

    status = _Driver->status();
    if (status != STATUS_SUCCESS)
    {
        LogError("Load driver status -> %X", status);
        return status;
    };

    Log("Done!");
    return STATUS_SUCCESS;
#else
    Log("Testing... %wZ", RegistryPath);

    // Set up the unload routine
    DriverObject->DriverUnload = DriverUnload;

    Log("is_array<int>() -> %d", (bool) base::is_array<int>());
    Log("is_array<int[]>() -> %d", (bool) base::is_array<int[]>());
    Log("is_array<int[5]>() -> %d", (bool) base::is_array<int[5]>());

    Log("is_base_of<failable, Driver>() -> %d", (bool) base::is_base_of<failable, Driver>());
    Log("is_base_of<Driver, failable>() -> %d", (bool) base::is_base_of<Driver, failable>());
    Log("is_base_of<int, double>() -> %d", (bool) base::is_base_of<int, double>());

    int* p = new int(5);
    object<int> x(std::move(p));
    object<int> y(5);
    Log("x == 5 -> %d", x == 5);
    Log("x == y -> %d", x == y);

    // Construct unicode
    {
        krn::string::unicode unicode(L"bao");
        Log("unicode -> %wZ", &unicode.raw());
    }
    // Construct with UNICODE_STRING
    {
        UNICODE_STRING str = { 0 };
        krn::string::unicode unicode(str);
        Log("unicode + empty UNICODE_STRING -> %wZ", &unicode.raw());
    }
    // Append string: valid + valid UNICODE_STRING
    {
        krn::string::unicode unicode(L"bao");
        UNICODE_STRING str = RTL_CONSTANT_STRING(L"_test");
        unicode += str;
        Log("unicode + valid UNICODE_STRING -> %wZ", &unicode.raw());
    }
    // Append string: valid + invalid UNICODE_STRING
    {
        krn::string::unicode unicode(L"bao");
        UNICODE_STRING str = {0};
        str.Buffer = reinterpret_cast<WCHAR*>(1);
        unicode += str;
        Log("unicode + invalid UNICODE_STRING -> %wZ", &unicode.raw());
    }

    return STATUS_SUCCESS;
#endif
}

_Function_class_(DRIVER_UNLOAD)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID DriverUnload(_In_ DRIVER_OBJECT* DriverObject)
{
#ifndef _TEST
    UNREFERENCED_PARAMETER(DriverObject);

    Log("Unloading...");

    delete _Driver;

    Log("Done!");
#else
    UNREFERENCED_PARAMETER(DriverObject);
#endif
}
