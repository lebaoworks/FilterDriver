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
        Log("");
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
        if (error() == STATUS_SUCCESS)
        {
            Log("Filter: %p", _filter);

            // Close ports to stop accepting new connection
            _ports.clear();
            
            // Close connections before FltUnregisterFilter() waiting for all connections to be closed
            {
                std::lock_guard<eresource_lock> lock(_connections_lock);
                _connections.clear();
            }

            // Unregister filter
            FltUnregisterFilter(_filter);
        }
    }

    void Filter::SetCredential(_In_ Communication::SavedCredential* Credential) noexcept { _credential = Credential; }

    bool Filter::Authenticate(_In_ Communication::Credential& Credential) noexcept
    {
        if (_credential == nullptr)
            return true;
        return _credential->Authenticate(Credential);
    }

    NTSTATUS Filter::OpenPort(_In_ UNICODE_STRING* PortName)
    {
        Log("OpenPort: %wZ", PortName);
        auto port = Port(_filter, PortName, this);
        if (port.error() != STATUS_SUCCESS)
            return port.error();
        
        if (_ports.push_back(std::move(port)) == _ports.end())
            return STATUS_NO_MEMORY;

        return STATUS_SUCCESS;
    }

    NTSTATUS Filter::OnConnect(_In_ PFLT_PORT ClientPort)
    {
        Log("%p", ClientPort);
        std::lock_guard<eresource_lock> lock(_connections_lock);
        if (_connections.push_back(Connection(_filter, ClientPort)) == _connections.end())
            return STATUS_NO_MEMORY;
        return STATUS_SUCCESS;
    }
    
    void Filter::OnDisconnect(_In_ PFLT_PORT ClientPort)
    {
        Log("%p", ClientPort);
        std::lock_guard<eresource_lock> lock(_connections_lock);
        for (auto it = _connections.begin(); it != _connections.end(); ++it)
            if (*it == ClientPort)
            {
                _connections.erase(it);
                break;
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
        if (SizeOfContext != sizeof(Communication::Credential))
        {
            Log("ClientPort: %p -> Invalid credential", ClientPort);
            return STATUS_INVALID_PARAMETER;
        }

        // Authenicate connection
        auto& filter = *reinterpret_cast<Filter*>(ServerPortCookie);
        if (!filter.Authenticate(*reinterpret_cast<Communication::Credential*>(ConnectionContext)))
        {
            Log("ClientPort: %p -> Authen failed", ClientPort);
            return STATUS_ACCESS_DENIED;
        }

        // Setup connection data
        auto cookie = new Cookie(filter, ClientPort);
        if (!cookie)
            return STATUS_NO_MEMORY;
        *ConnectionCookie = cookie;
        
        // Save connection to filter
        auto status = filter.OnConnect(ClientPort);
        if (status != STATUS_SUCCESS)
            delete cookie;
        
        return status;
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    VOID FLTAPI FilterDisconnectNotify(_In_ PVOID ConnectionCookie)
    {
        auto cookie = reinterpret_cast<Cookie*>(ConnectionCookie);
        Log("Disconnect ClientPort: %p", cookie->ClientPort);
        cookie->Filter.OnDisconnect(cookie->ClientPort);
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

    Port::Port(
        _In_ PFLT_FILTER Filter,
        _In_ UNICODE_STRING* PortName,
        _In_ MiniFilter::Filter* Cookie) noexcept
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
            Filter,                     // Filter
            &_port,                     // ServerPort
            &oa,                        // ObjectAttributes
            (PVOID)Cookie,              // ServerPortCookie
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

    Port::Port(Port&& Other) noexcept : _port(Other._port) { Other._error = STATUS_NO_MEMORY; }

    Port::~Port() noexcept
    {
        if (error() == STATUS_SUCCESS)
        {
            Log("Port: %p", _port);
            FltCloseCommunicationPort(_port);
        }
    }
}

// Connection
namespace MiniFilter
{
    Connection::Connection(
        _In_ PFLT_FILTER Filter,
        _In_ PFLT_PORT Port) noexcept : _filter(Filter), _port(Port)
    {}

    Connection::Connection(Connection&& Other) noexcept : _filter(Other._filter), _port(Other._port)
    {
        Other._filter = nullptr;
        Other._port = nullptr;
    }

    Connection::~Connection() noexcept
    {
        if (_filter && _port)
            FltCloseClientPort(_filter, &_port);
    }
}