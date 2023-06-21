#include <wdm.h>
#include <Shared.h>

#ifdef __cplusplus
extern "C" {
    NTSTATUS DriverEntry(
        _In_ DRIVER_OBJECT* DriverObject,
        _In_ UNICODE_STRING* RegistryPath);
    
    _Function_class_(DRIVER_UNLOAD)
    VOID DriverUnload(_In_ DRIVER_OBJECT* DriverObject);
}

#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

NTSTATUS DriverEntry(
    _In_ DRIVER_OBJECT* DriverObject,
    _In_ UNICODE_STRING* RegistryPath)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    Log(__FUNCTION__"() Hello");
    DriverObject->DriverUnload = DriverUnload;

    return STATUS_SUCCESS;
}

_Function_class_(DRIVER_UNLOAD)
VOID DriverUnload(_In_ DRIVER_OBJECT* DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    Log(__FUNCTION__"() Bye");
}
