#include "MiniFilter.h"

#include <fltKernel.h>
#include <ntstrsafe.h>

#include <Shared.h>
#include <Communication.h>

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
        {
            // Close ports to stop accepting new connection
            _comports.clear();
            
            // Close connections before FltUnregisterFilter() waiting for all connections to be closed
            ClearConnections();

            // Unregister filter
            FltUnregisterFilter(_filter);
        }
    }

    PFLT_FILTER Filter::GetFilter() const { return _filter; }

    NTSTATUS Filter::RegisterCommunicationPort(_In_ UNICODE_STRING* PortName)
    {
        Log("Register PortName: %wZ", PortName);
        auto port = std::make_unique<CommunicationPort>(*this, PortName);
        if (port == nullptr)
            return STATUS_NO_MEMORY;
        if (port->error() != STATUS_SUCCESS)
            return port->error();
        if (_comports.push_back(std::move(port)) == _comports.end())
            return STATUS_NO_MEMORY;
        return STATUS_SUCCESS;
    }

    NTSTATUS Filter::AddConnection(_In_ PFLT_PORT ClientPort)
    {
        Log("%p", ClientPort);
        std::lock_guard<eresource_lock> lock(_connections_lock);
        if (_connections.push_back(ClientPort) == _connections.end())
            return STATUS_NO_MEMORY;
        return STATUS_SUCCESS;
    }
    
    void Filter::RemoveConnection(_In_ PFLT_PORT ClientPort)
    {
        std::lock_guard<eresource_lock> lock(_connections_lock);
        Log("%p", ClientPort);
        for (auto it = _connections.begin(); it != _connections.end(); ++it)
            if (*it == ClientPort)
            {
                FltCloseClientPort(_filter, &*it);
                _connections.erase(it);
                break;
            }
    }

    void Filter::ClearConnections()
    {
        std::lock_guard<eresource_lock> lock(_connections_lock);
        for (auto& connection : _connections)
        {
            Log("Force close connection: %p", connection);
            FltCloseClientPort(_filter, &connection);
        }
    }
}

// Comport
namespace MiniFilter
{
    struct Cookie
    {
        Filter& Filter;
        PFLT_PORT ClientPort = NULL;
        Cookie(MiniFilter::Filter& Filter, PFLT_PORT Port) : Filter(Filter), ClientPort(Port) {}
    };

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS FLTAPI FilterConnectNotify(
        _In_ PFLT_PORT ClientPort,
        _In_ PVOID ServerPortCookie,
        _In_reads_bytes_(SizeOfContext) PVOID ConnectionContext,
        _In_ ULONG SizeOfContext,
        _Outptr_ PVOID* ConnectionCookie)
    {
        Log("ClientPort: %p", ClientPort);
        if (SizeOfContext != sizeof(Communication::Credential))
            return STATUS_INVALID_PARAMETER;

        // Authenicate connection
        Communication::Credential credential;
        RtlStringCchCopyNA(
            reinterpret_cast<NTSTRSAFE_PSTR>(credential.Password), sizeof(credential.Password),
            reinterpret_cast<NTSTRSAFE_PSTR>(reinterpret_cast<Communication::Credential*>(ConnectionContext)->Password), SizeOfContext);
        Log("ClientCredential: %s", credential.Password);

        // Setup connection data
        auto& filter = *reinterpret_cast<Filter*>(ServerPortCookie);
        auto cookie = new Cookie(filter, ClientPort);
        if (!cookie)
            return STATUS_NO_MEMORY;
        *ConnectionCookie = cookie;

        auto status = filter.AddConnection(ClientPort);
        if (status != STATUS_SUCCESS)
        {
            delete cookie;
            return status;
        }

        return STATUS_SUCCESS;
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    VOID FLTAPI FilterDisconnectNotify(_In_ PVOID ConnectionCookie)
    {
        auto cookie = reinterpret_cast<Cookie*>(ConnectionCookie);
        Log("Disconnect ClientPort: %p", cookie->ClientPort);
        cookie->Filter.RemoveConnection(cookie->ClientPort);
        delete cookie;
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS FLTAPI FilterMessageNotify(
        _In_ PVOID ConnectionCookie,
        _In_reads_bytes_opt_(InputBufferSize) PVOID InputBuffer,
        _In_ ULONG InputBufferSize,
        _Out_writes_bytes_to_opt_(OutputBufferSize, *ReturnOutputBufferLength) PVOID OutputBuffer,
        _In_ ULONG OutputBufferSize,
        _Out_ PULONG ReturnOutputBufferLength)
    {
        auto cookie = reinterpret_cast<Cookie*>(ConnectionCookie);
        Log("ClientPort: %X", cookie->ClientPort);
        UNREFERENCED_PARAMETER(InputBuffer);
        UNREFERENCED_PARAMETER(InputBufferSize);
        UNREFERENCED_PARAMETER(OutputBuffer);
        UNREFERENCED_PARAMETER(OutputBufferSize);
        *ReturnOutputBufferLength = 0;

        LARGE_INTEGER interval;
        interval.QuadPart = -10 * 10'000'000;
        KeDelayExecutionThread(KernelMode, FALSE, &interval);
        return STATUS_SUCCESS;
    }

    CommunicationPort::CommunicationPort(
        _In_ const Filter& Filter,
        _In_ UNICODE_STRING* PortName)
    {
        Log("Setup Comport: %wZ", PortName);
        auto status = STATUS_SUCCESS;
        defer{ failable_object<NTSTATUS>::_error = status; }; // set error code

        // Open Communication Port
        PSECURITY_DESCRIPTOR sd;
        status = ::FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);
        if (status != STATUS_SUCCESS)
        {
            Log("FltBuildDefaultSecurityDescriptor failed -> Status: %X", status);
            return;
        }
        defer{ ::FltFreeSecurityDescriptor(sd); };

        OBJECT_ATTRIBUTES oa;
        InitializeObjectAttributes(&oa, PortName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, sd);

        status = ::FltCreateCommunicationPort(
            Filter.GetFilter(),         // Filter
            &_port,                     // ServerPort
            &oa,                        // ObjectAttributes
            (PVOID)&Filter,             // ServerPortCookie
            FilterConnectNotify,        // ConnectNotifyCallback
            FilterDisconnectNotify,     // DisconnectNotifyCallback
            FilterMessageNotify,        // MessageNotifyCallback
            2);                         // MaxConnections
        if (status != STATUS_SUCCESS)
        {
            Log("FltCreateCommunicationPort failed -> Status: %X", status);
            return;
        }
        defer{ if (status != STATUS_SUCCESS) FltCloseCommunicationPort(_port); }; // Rollback
    }

    CommunicationPort::~CommunicationPort()
    {
        Log("Initialized: %s", error() == STATUS_SUCCESS ? "true" : "false");
        if (error() == STATUS_SUCCESS)
            FltCloseCommunicationPort(_port);
    }
}