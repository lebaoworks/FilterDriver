// References: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/fltkernel/ns-fltkernel-_flt_parameters
#include "MiniFilter.h"
#include <fltKernel.h>

#include <Shared.h>

#include "MiniFilterUtils.h"
using namespace MiniFilterUtils;


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

NTSTATUS MiniFilter::Install(_In_ DRIVER_OBJECT* DriverObject)
{
    auto status = FltRegisterFilter(DriverObject, &FilterRegistration, &FilterHandle);
    if (status != STATUS_SUCCESS)
    {
        Log("Register filter failed: %X", status);
        return status;
    }
    status = FltStartFiltering(FilterHandle);
    if (status != STATUS_SUCCESS)
    {
        Log("Start filter failed: %X", status);
        FltUnregisterFilter(FilterHandle);
        return status;
    }
    return STATUS_SUCCESS;
}

void MiniFilter::Uninstall()
{
    FltUnregisterFilter(FilterHandle);
}

NTSTATUS FLTAPI FilterUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags)
{
    Log("Uninstalling");
    UNREFERENCED_PARAMETER(Flags);
    return STATUS_SUCCESS;
}

NTSTATUS FLTAPI FilterQueryTeardown(
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags)
{
    Log("");
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
    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
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

    PFLT_FILE_NAME_INFORMATION file_name_info;
    auto status = FltGetFileNameInformation(
        Data,
        FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
        &file_name_info);
    if (NT_SUCCESS(status))
    {
        defer{ FltReleaseFileNameInformation(file_name_info); };
        
        FILE_ID fileId;
        status = GetFileId(Data->Iopb->TargetInstance, Data->Iopb->TargetFileObject, fileId);
        if (status == STATUS_SUCCESS)
            Log("[FileId: %I64X] FileName: %wZ", fileId.FileId64.Value, file_name_info->Name);
        else
            Log("[FileId: ERROR: %X] FileName: %wZ", status, file_name_info->Name);
    }
    return FLT_POSTOP_FINISHED_PROCESSING;
}