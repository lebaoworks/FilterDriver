#include <fltKernel.h>

#include "Shared.h"
#include "MiniFilter.h"
#include "Trace.h"
#include "driver.tmh"


/* Forward declarations */
NTSTATUS FLTAPI FilterUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags);
NTSTATUS FLTAPI FilterQueryTeardown(_In_ PCFLT_RELATED_OBJECTS FltObjects, _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags);
VOID     FLTAPI FilterContextCleanup(_In_ PFLT_CONTEXT Context, _In_ FLT_CONTEXT_TYPE ContextType);
FLT_PREOP_CALLBACK_STATUS  FLTAPI FilterOperation_Pre_Create(_Inout_ PFLT_CALLBACK_DATA Data, _In_ PCFLT_RELATED_OBJECTS FltObjects, _In_ PVOID* CompletionContext);
FLT_POSTOP_CALLBACK_STATUS FLTAPI FilterOperation_Post_Create(_Inout_ PFLT_CALLBACK_DATA Data, _In_ PCFLT_RELATED_OBJECTS FltObjects, _In_opt_ PVOID CompletionContext, _In_ FLT_POST_OPERATION_FLAGS Flags);


/* Structures */
struct FILE_CONTEXT
{
};

/* Static data declarations */
static PFLT_FILTER FilterHandle = NULL;

static const FLT_CONTEXT_REGISTRATION FilterContextRegistration[] = {
    {
        FLT_FILE_CONTEXT,               //  ContextType
        0,                              //  Flags
        FilterContextCleanup,           //  ContextCleanupCallback
        FLT_VARIABLE_SIZED_CONTEXTS,    //  SizeOfContext
        'TLF0',                         //  PoolTag
        NULL,                           //  ContextAllocateCallback
        NULL,                           //  ContextFreeCallback
        NULL                            //  Reserved
    },
    { FLT_CONTEXT_END }
};

static FLT_OPERATION_REGISTRATION FilterCallbacks[] = {

    {
        IRP_MJ_CREATE,                  //  MajorFunction
        0,                              //  Flags
        FilterOperation_Pre_Create,     //  PreOperation
        FilterOperation_Post_Create,    //  PostOperation
    },
    { IRP_MJ_OPERATION_END }
};

static const FLT_REGISTRATION FilterRegistration = {

    sizeof(FLT_REGISTRATION),           //  Size
    FLT_REGISTRATION_VERSION,           //  Version
    0,                                  //  Flags

    FilterContextRegistration,          //  Context Registration
    FilterCallbacks,                    //  Operation Registration

    FilterUnload,                       //  FilterUnload

    NULL,                               //  InstanceSetup
    FilterQueryTeardown,                //  InstanceQueryTeardown
    NULL,                               //  InstanceTeardownStart
    NULL,                               //  InstanceTeardownComplete

    NULL,                               //  GenerateFileName
    NULL,                               //  NormalizeNameComponentCallback
    NULL,                               //  NormalizeContextCleanupCallback
    NULL,                               //  TransactionNotificationCallback
    NULL,                               //  NormalizeNameComponentExCallback
    NULL,                               //  SectionNotificationCallback
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
    return STATUS_SUCCESS;
}

NTSTATUS FLTAPI FilterQueryTeardown(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);
    return STATUS_SUCCESS;
}

VOID FLTAPI FilterContextCleanup(
    _In_ PFLT_CONTEXT Context,
    _In_ FLT_CONTEXT_TYPE ContextType)
{
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(ContextType);
}

FLT_PREOP_CALLBACK_STATUS FLTAPI FilterOperation_Pre_Create(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ PVOID* CompletionContext)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS FLTAPI FilterOperation_Post_Create(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);
    return FLT_POSTOP_FINISHED_PROCESSING;
}