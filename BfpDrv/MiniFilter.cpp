#include "MiniFilter.h"

#include <fltKernel.h>
#include <Shared.h>

// Filter
namespace MiniFilter
{
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

        PFLT_FILE_NAME_INFORMATION file_name_info;
        auto status = FltGetFileNameInformation(
            Data,
            FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
            &file_name_info);
        if (NT_SUCCESS(status))
        {
            defer{ FltReleaseFileNameInformation(file_name_info); };
            //Log("FileName: %wZ", file_name_info->Name);

            auto file_name = std::wstring(file_name_info->Name.Buffer, file_name_info->Name.Length / 2);
            if (!file_name.error())
            {
                if (file_name.find(L"BAO1340") != std::wstring::npos)
                {
                    Log("[Denied] FileName: %ws", file_name.c_str());
                    Data->IoStatus.Status = STATUS_ACCESS_DENIED;
                    return FLT_PREOP_COMPLETE;
                }
            }

        }
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

        /*PFLT_FILE_NAME_INFORMATION file_name_info;
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
        }*/
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    /* Static data declarations */
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

    Filter::Filter(_In_ DRIVER_OBJECT* DriverObject)
    {
        Log("Setup DriverObject: %X", DriverObject);
        auto status = STATUS_SUCCESS;
        defer{ failable_object<NTSTATUS>::_error = status; }; // set error code

        status = ::FltRegisterFilter(DriverObject, &FilterRegistration, &_filter);
        if (status != STATUS_SUCCESS)
        {
            Log("FltRegisterFilter failed -> Status: %X", status);
            return;
        }
        defer{ if (status != STATUS_SUCCESS) ::FltUnregisterFilter(_filter); }; // Rollback

        status = FltStartFiltering(_filter);
        if (status != STATUS_SUCCESS)
            Log("FltStartFiltering failed -> Status: %X", status);
    }

    Filter::~Filter()
    {
        Log("Initialized: %s", error() == STATUS_SUCCESS ? "true" : "false");
        if (error() == STATUS_SUCCESS)
            FltUnregisterFilter(_filter);
    }
}

// Comport
namespace MiniFilter
{

    //// Open Communication Port
    //PSECURITY_DESCRIPTOR sd;
    //status = ::FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);
    //if (status != STATUS_SUCCESS)
    //{
    //    Log("FltBuildDefaultSecurityDescriptor failed -> Status: %X", status);
    //    return;
    //}
    //defer{ ::FltFreeSecurityDescriptor(sd); };

    //OBJECT_ATTRIBUTES oa;
    //InitializeObjectAttributes(&oa, ComportName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, sd);

    //status = ::FltCreateCommunicationPort(
    //    _filter,                    // Filter
    //    &_port,                     // ServerPort
    //    &oa,                        // ObjectAttributes
    //    this,                       // ServerPortCookie
    //    FilterConnectNotify,        // ConnectNotifyCallback
    //    FilterDisconnectNotify,     // DisconnectNotifyCallback
    //    FilterMessageNotify,        // MessageNotifyCallback
    //    2);                         // MaxConnections
    //if (status != STATUS_SUCCESS)
    //{
    //    Log("FltCreateCommunicationPort failed -> Status: %X", status);
    //    return;
    //}
    //defer{ if (status != STATUS_SUCCESS) FltCloseCommunicationPort(_port); }; // Rollback

}