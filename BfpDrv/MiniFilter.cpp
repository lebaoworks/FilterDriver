#include "MiniFilter.h"

// Callbacks
namespace MiniFilter
{
    static Filter* g_filter = nullptr;

    NTSTATUS FLTAPI FilterUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags)
    {
        Log("Flags: %X", Flags);

        // Allow filter unload
        return STATUS_SUCCESS;
    }

    NTSTATUS FLTAPI FilterInstanceSetupCallback(
        _In_ PCFLT_RELATED_OBJECTS FltObjects,
        _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
        _In_ DEVICE_TYPE VolumeDeviceType,
        _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType)
    {
        UNREFERENCED_PARAMETER(Flags);
        Log("Instance: %p, VolumeType: %X, FileSystemType: %X",
            FltObjects->Instance,
            VolumeDeviceType,
            VolumeFilesystemType);

        // Let's instance attach to volume device
        return STATUS_SUCCESS;
    }

    NTSTATUS FLTAPI FilterInstanceQueryTeardown(
        _In_ PCFLT_RELATED_OBJECTS FltObjects,
        _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags)
    {
        Log("Instance: %p, Flags: %X, Volume: %p",
            FltObjects,
            Flags,
            FltObjects->Volume);

        // Allow detach from volume
        return STATUS_SUCCESS;
    }

    void FLTAPI FilterInstanceTeardownCompleteCallback(
        _In_ PCFLT_RELATED_OBJECTS FltObjects,
        _In_ FLT_INSTANCE_TEARDOWN_FLAGS Reason)
    {
        Log("Instance: %p, Reason: %X",
            FltObjects->Instance,
            Reason);
    }

    FLT_PREOP_CALLBACK_STATUS FLTAPI FilterOperation_Pre_Create(
        _Inout_ PFLT_CALLBACK_DATA Data,
        _In_ PCFLT_RELATED_OBJECTS FltObjects,
        _In_ PVOID* CompletionContext)
    {
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
            LogDebug("[Instance: %p] File: %wZ", FltObjects->Instance, &file_name_info->Name);
        }

        // No post operator needed
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    /* Static data declarations */
    static const FLT_CONTEXT_REGISTRATION FilterContextRegistration[] = {
        {
            FLT_FILE_CONTEXT,               //  ContextType
            0,                              //  Flags
            NULL,                           //  ContextCleanupCallback
            FLT_VARIABLE_SIZED_CONTEXTS,    //  SizeOfContext
            'TLF1',                         //  PoolTag
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
            NULL,                           //  PostOperation
        },
        { IRP_MJ_OPERATION_END }
    };

    static const FLT_REGISTRATION FilterRegistration = {
        sizeof(FLT_REGISTRATION),               //  Size
        FLT_REGISTRATION_VERSION,               //  Version
        0,                                      //  Flags

        FilterContextRegistration,              //  Context Registration
        FilterCallbacks,                        //  Operation Registration

        FilterUnload,                           //  FilterUnload

        FilterInstanceSetupCallback,            //  InstanceSetup
        FilterInstanceQueryTeardown,            //  InstanceQueryTeardown
        NULL,                                   //  InstanceTeardownStart
        FilterInstanceTeardownCompleteCallback, //  InstanceTeardownComplete

        NULL,                                   //  GenerateFileName
        NULL,                                   //  NormalizeNameComponentCallback
        NULL,                                   //  NormalizeContextCleanupCallback
        NULL,                                   //  TransactionNotificationCallback
        NULL,                                   //  NormalizeNameComponentExCallback
        NULL,                                   //  SectionNotificationCallback
    };

}

namespace MiniFilter
{
    Filter::Filter(_In_ DRIVER_OBJECT* DriverObject)
    {
        Log("Init Filter");
        auto& status = failable::_status;

        status = ::FltRegisterFilter(DriverObject, &FilterRegistration, &_filter);
        if (status != STATUS_SUCCESS)
        {
            LogError("FltRegisterFilter() -> status: %X", status);
            return;
        }
        defer{ if (status != STATUS_SUCCESS) ::FltUnregisterFilter(_filter); };

        status = FltStartFiltering(_filter);
        if (status != STATUS_SUCCESS)
            LogError("FltStartFiltering() -> status: %X", status);

        Log("Done!");
    }

    Filter::~Filter()
    {
        Log("Cleanup Filter...");
        
        if (this->status() != STATUS_SUCCESS)
        {
            Log("Skip!");
            return;
        }

        FltUnregisterFilter(_filter);

        Log("Done!");
    }
}