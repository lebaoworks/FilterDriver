#include <fltKernel.h>

#include "Shared.h"
#include "MiniFilter.h"
#include "Trace.h"
#include "driver.tmh"


/* Forward declarations */
NTSTATUS FLTAPI FilterUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags);
NTSTATUS FLTAPI FilterQueryTeardown(_In_ PCFLT_RELATED_OBJECTS FltObjects, _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags);

/* Static data declarations */
static PFLT_FILTER FilterHandle = NULL;

CONST FLT_REGISTRATION FilterRegistration = {

    sizeof(FLT_REGISTRATION),           //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    NULL,                               //  Context
    NULL,                               //  Operation callbacks

    FilterUnload,                       //  FilterUnload

    NULL,                               //  InstanceSetup
    FilterQueryTeardown,                  //  InstanceQueryTeardown
    NULL,                               //  InstanceTeardownStart
    NULL,                               //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  GenerateDestinationFileName
    NULL                                //  NormalizeNameComponent

};

bool MiniFilter::Install(_In_ DRIVER_OBJECT* DriverObject)
{
    if (FilterHandle != NULL)
    {
        //TraceLog(TRACE_LEVEL_ERROR, "%!FUNC! Filter already installed");
        return false;
    }

    auto status = FltRegisterFilter(DriverObject, &FilterRegistration, &FilterHandle);
    if (status != STATUS_SUCCESS)
    {
        //TraceLog(TRACE_LEVEL_ERROR, "%!FUNC! Filter installing failed %X", status);
        return false;
    }
    status = FltStartFiltering(FilterHandle);
    if (status != STATUS_SUCCESS)
    {
        //TraceLog(TRACE_LEVEL_ERROR, "%!FUNC! Filter starting failed %X", status);
        FltUnregisterFilter(FilterHandle);
        return false;
    }
    return true;
}

void MiniFilter::Uninstall()
{
    if (FilterHandle == NULL)
    {
        //TraceLog(TRACE_LEVEL_ERROR, "%!FUNC! Filter is not installed");
        return;
    }
    FltUnregisterFilter(FilterHandle);
    FilterHandle = NULL;
}

NTSTATUS FLTAPI FilterUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Flags);
    PAGED_CODE();
    return STATUS_SUCCESS;
}

NTSTATUS FLTAPI FilterQueryTeardown(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);

    PAGED_CODE();

    return STATUS_SUCCESS;
}