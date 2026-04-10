/********************
*     Includes      *
********************/

#include "MiniFilter.hpp"

// Logging vie tracing
#include "trace.h"
#include "MiniFilter.tmh"


/*********************
*     Global Vars    *
*********************/

static MiniFilter::Filter* g_filter = nullptr;


/*********************
*   Implementations  *
*********************/

// Filter
namespace MiniFilter
{
    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    NTSTATUS FLTAPI FilterUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Unload with flags: %X", Flags);

        // Allow filter unload
        return STATUS_SUCCESS;
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    NTSTATUS FLTAPI FilterInstanceSetupCallback(
        _In_ PCFLT_RELATED_OBJECTS FltObjects,
        _In_ FLT_INSTANCE_SETUP_FLAGS Flags,
        _In_ DEVICE_TYPE VolumeDeviceType,
        _In_ FLT_FILESYSTEM_TYPE VolumeFilesystemType)
    {
        UNREFERENCED_PARAMETER(Flags);
        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Instance Setup: %p, VolumeType: %X, FileSystemType: %X",
            FltObjects->Instance,
            VolumeDeviceType,
            VolumeFilesystemType);

        // Let's instance attach to volume device
        return STATUS_SUCCESS;
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    NTSTATUS FLTAPI FilterInstanceQueryTeardown(
        _In_ PCFLT_RELATED_OBJECTS FltObjects,
        _In_ FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Instance Query Teardown: %p, Flags: %X, Volume: %p",
            FltObjects,
            Flags,
            FltObjects->Volume);

        // Allow detach from volume
        return STATUS_SUCCESS;
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    void FLTAPI FilterInstanceTeardownCompleteCallback(
        _In_ PCFLT_RELATED_OBJECTS FltObjects,
        _In_ FLT_INSTANCE_TEARDOWN_FLAGS Reason)
    {
        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Instance Teardown Complete: %p, Reason: %X",
            FltObjects->Instance,
            Reason);
    }

    // IRQL Reference: https://community.osr.com/t/irp-mj-create-irql-of-pre-and-post-handlers/53349
    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    FLT_PREOP_CALLBACK_STATUS FLTAPI FilterOperation_Pre_Create(
        _Inout_ PFLT_CALLBACK_DATA Data,
        _In_    PCFLT_RELATED_OBJECTS FltObjects,
        _Out_   PVOID* CompletionContext)
    {
        UNREFERENCED_PARAMETER(FltObjects);
        UNREFERENCED_PARAMETER(CompletionContext);

        PFLT_FILE_NAME_INFORMATION file_name_info;
        auto status = FltGetFileNameInformation(
            Data,
            FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT,
            &file_name_info);
        if (status == STATUS_SUCCESS)
        {
            defer{ FltReleaseFileNameInformation(file_name_info); };
            TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "[Instance: %p] File: %wZ", FltObjects->Instance, &file_name_info->Name);
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
            'EVT0',                         //  PoolTag
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

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Filter::Filter(_In_ DRIVER_OBJECT* DriverObject)
    {
        auto& status = failable::_status;

        status = ::FltRegisterFilter(DriverObject, &FilterRegistration, &_filter);
        if (status != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "FltRegisterFilter() -> status: %!STATUS!", status);
            return;
        }
        defer{ if (status != STATUS_SUCCESS) ::FltUnregisterFilter(_filter); };

        status = FltStartFiltering(_filter);
        if (status != STATUS_SUCCESS)
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "FltStartFiltering() -> status: %!STATUS!", status);
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Filter::~Filter()
    {
        if (this->status() != STATUS_SUCCESS)
            return;

        FltUnregisterFilter(_filter);
    }

    NTSTATUS Filter::OnClientConnect(_In_ PFLT_PORT ClientPort)
    {
        UNREFERENCED_PARAMETER(ClientPort);
        return STATUS_SUCCESS;
	}

    void Filter::OnClientDisconnect(_Inout_ PFLT_PORT ClientPort)
    {
        FltCloseClientPort(_filter, &ClientPort);
    }
}

// Filter Communication Port
namespace MiniFilter
{
    struct PortCookie
    {
        Port::ConnectNotifyCallback ConnectNotify;
        PFLT_FILTER Filter;
        PortCookie(Port::ConnectNotifyCallback ConnectNotify, PFLT_FILTER Filter) : ConnectNotify(ConnectNotify), Filter(Filter) {}
    };
    struct Cookie
    {
        PFLT_FILTER Filter;
        PFLT_PORT   ClientPort = NULL;
        Cookie(PFLT_FILTER Filter, PFLT_PORT Port) : Filter(Filter), ClientPort(Port) {}
    };

    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS FLTAPI FilterConnectNotify(
        _In_ PFLT_PORT ClientPort,
        _In_ PVOID ServerPortCookie,
        _In_reads_bytes_(SizeOfContext) PVOID ConnectionContext,
        _In_ ULONG SizeOfContext,
        _Outptr_ PVOID* ConnectionCookie)
    {
        UNREFERENCED_PARAMETER(ConnectionContext);
        UNREFERENCED_PARAMETER(SizeOfContext);

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "MiniPort: Client connected: %p", ClientPort);

        auto portCookie = reinterpret_cast<PortCookie*>(ServerPortCookie);
        defer{ delete portCookie; };

        NTSTATUS status = STATUS_SUCCESS;

        // Create a cookie to keep track of the connection, which will be freed on disconnect.
        auto cookie = new Cookie(portCookie->Filter, ClientPort);
        if (cookie == nullptr)
        {
            status = STATUS_NO_MEMORY;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "MiniPort: Create Connection Cookie failed -> Status: %!STATUS!", status);
            return status;
        }
        *ConnectionCookie = cookie;
        defer{ if (status != STATUS_SUCCESS) { delete cookie; } };

        // Create a connection object to represent this connection, which will be freed on disconnect.
        auto connection =  krn::make<Connection>(portCookie->Filter, ClientPort);
        if (connection.status() != STATUS_SUCCESS)
        {
            status = connection.status();
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "MiniPort: Create Connection failed -> Status: %!STATUS!", status);
            return status;
        }
        status = portCookie->ConnectNotify(connection);
        return status;
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    VOID FLTAPI FilterDisconnectNotify(_In_ PVOID ConnectionCookie)
    {
        auto cookie = reinterpret_cast<Cookie*>(ConnectionCookie);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "MiniPort: Client disconnected: %p", cookie->ClientPort);
        delete cookie;
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Port::Port(
        _Inout_ MiniFilter::Filter& Filter,
        _In_    UNICODE_STRING* PortName,
        _In_    ConnectNotifyCallback ConnectNotifyCallback
    ) noexcept
    {
        auto status = STATUS_SUCCESS;
        defer{ krn::failable::_status = status; }; // set error code

        // Build Security Descriptor
        PSECURITY_DESCRIPTOR sd;
        status = ::FltBuildDefaultSecurityDescriptor(&sd, FLT_PORT_ALL_ACCESS);
        if (status != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "MiniPort: Build SecurityDescriptor failed -> Status: %X", status);
            return;
        }
        RtlSetDaclSecurityDescriptor(sd, TRUE, NULL, FALSE);
        defer{ ::FltFreeSecurityDescriptor(sd); };

        // Initialize Object Attributes
        OBJECT_ATTRIBUTES oa;
        InitializeObjectAttributes(&oa, PortName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, sd);

        // Create a cookie to pass the connect
        auto cookie = new PortCookie(ConnectNotifyCallback, Filter._filter);
        if (cookie == nullptr)
        {
            status = STATUS_NO_MEMORY;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "MiniPort: Create Port Cookie failed -> Status: %X", status);
            return;
        }

        // Open Communication Port
        status = ::FltCreateCommunicationPort(
            Filter._filter,             // Filter
            &_port,                     // ServerPort
            &oa,                        // ObjectAttributes
            cookie,                     // ServerPortCookie
            FilterConnectNotify,        // ConnectNotifyCallback
            FilterDisconnectNotify,     // DisconnectNotifyCallback
            NULL,                       // MessageNotifyCallback
            1);                         // MaxConnections
        if (status != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "MiniPort: Create Port failed -> Status: %X", status);
            return;
        }
    }

    Port::~Port()
    {
        if (this->status() != STATUS_SUCCESS)
            return;
        FltCloseCommunicationPort(_port);
    }
}

// Connection
namespace MiniFilter
{
    Connection::Connection(
        _In_ PFLT_FILTER Filter,
        _In_ PFLT_PORT   Port) noexcept : _filter(Filter), _port(Port) {}

    Connection::~Connection()
    {
        FltCloseClientPort(_filter, &_port);
    }

    NTSTATUS Connection::SendMessage(
        _In_reads_bytes_(InputBufferLength) PVOID InputBuffer,
        _In_ ULONG InputBufferLength) noexcept
    {
        return FltSendMessage(_filter, &_port, InputBuffer, InputBufferLength, NULL, 0, NULL);
    }
}