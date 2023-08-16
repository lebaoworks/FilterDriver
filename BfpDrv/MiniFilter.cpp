#include "MiniFilter.h"

#include <fltKernel.h>
#include <ntstrsafe.h>

#include <Shared.h>
#include <Win32.h>
#include <Hash.h>
#include <Communication.h>

// Filter
namespace MiniFilter
{
    static Filter* g_filter = nullptr;

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
            
            if (g_filter->IsFileProtected(&file_name_info->Name))
            {
                Log("[Denied] FileName: %wZ", &file_name_info->Name);
                Data->IoStatus.Status = STATUS_ACCESS_DENIED;
                return FLT_PREOP_COMPLETE;
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

        g_filter = this;
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

    NTSTATUS Filter::InitAuthenticator()
    {
        auto new_authenticator = std::make_unique<Authenticator>(nullptr);
        if (new_authenticator == nullptr)
            return STATUS_NO_MEMORY;
        if (new_authenticator->error() != STATUS_SUCCESS)
            return new_authenticator->error();
        _authenticator = std::move(new_authenticator);
        return STATUS_SUCCESS;
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

    NTSTATUS Filter::OnConnect(_In_ PFLT_PORT ClientPort, _In_ const Communication::Credential& Credential)
    {
        Log("%p", ClientPort);

        if (_authenticator->Authenticate(Credential) == false)
            return STATUS_ACCESS_DENIED;

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

    NTSTATUS Filter::AddProtectedFile(_In_ const WCHAR* FileName)
    {
        unicode_string input(2000);
        if (input.error())
            return STATUS_NO_MEMORY;
        RtlUnicodeStringCatString(&input.raw(), L"\\??\\");
        RtlUnicodeStringCatString(&input.raw(), FileName);

        unicode_string path(2000);
        if (input.error())
            return STATUS_NO_MEMORY;
        Win32::Path::QueryAbsoluteTarget(&input.raw(), &path.raw());

        {
            auto ws = std::wstring(path.raw().Buffer, path.raw().Length / 2);
            std::lock_guard<eresource_lock> lock(_protected_files_lock);
            if (_protected_files.push_back(std::move(ws)) == _protected_files.end())
                return STATUS_NO_MEMORY;
            Log("Added %wZ", &path.raw());
        }
        return STATUS_SUCCESS;
    }
    NTSTATUS Filter::RemoveProtectedFile(_In_ const WCHAR* FileName)
    {
        unicode_string input(2000);
        if (input.error())
            return STATUS_NO_MEMORY;
        RtlUnicodeStringCatString(&input.raw(), L"\\??\\");
        RtlUnicodeStringCatString(&input.raw(), FileName);

        unicode_string path(2000);
        if (input.error())
            return STATUS_NO_MEMORY;
        Win32::Path::QueryAbsoluteTarget(&input.raw(), &path.raw());
        auto nt_path = std::wstring(path.raw().Buffer, path.raw().Length / 2);
        if (nt_path.error())
            return STATUS_NO_MEMORY;

        {
            std::lock_guard<eresource_lock> lock(_protected_files_lock);
            for (auto it = _protected_files.begin(); it != _protected_files.end(); ++it)
                if (*it == nt_path)
                {
                    _protected_files.erase(it);
                    Log("Removed %ws", nt_path.c_str());
                    return STATUS_SUCCESS;
                }
        }
        return STATUS_NOT_FOUND;
    }
    bool Filter::IsFileProtected(_In_ UNICODE_STRING* FileName)
    {
        auto nt_path = std::wstring(FileName->Buffer, FileName->Length / 2);
        if (nt_path.error())
            return false;

        std::lock_guard<eresource_lock> lock(_protected_files_lock);
        for (auto it = _protected_files.begin(); it != _protected_files.end(); ++it)
            if (*it == nt_path)
                return true;
        return false;
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

        // Setup connection data
        auto& filter = *reinterpret_cast<Filter*>(ServerPortCookie);
        auto cookie = new Cookie(filter, ClientPort);
        if (!cookie)
            return STATUS_NO_MEMORY;
        *ConnectionCookie = cookie;
        
        // Save connection to filter
        auto status = filter.OnConnect(ClientPort, *reinterpret_cast<Communication::Credential*>(ConnectionContext));
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

        if (InputBuffer == NULL ||
            InputBufferSize <  sizeof(Communication::MessageHeader))
        {
            Log("ClientPort: %p -> Invalid message", cookie->ClientPort);
            return STATUS_INVALID_PARAMETER;
        }

        auto header = reinterpret_cast<Communication::MessageHeader*>(InputBuffer);
        if (header->Type == Communication::Type::Protect)
        {
            auto message = reinterpret_cast<Communication::ClientMessage<Communication::MessageProtect>*>(InputBuffer);
            return cookie->Filter.AddProtectedFile(message->Body.DosPath);
        }
        else if (header->Type == Communication::Type::Unprotect)
        {
            auto message = reinterpret_cast<Communication::ClientMessage<Communication::MessageUnprotect>*>(InputBuffer);
            return cookie->Filter.RemoveProtectedFile(message->Body.DosPath);
        }

        /*LARGE_INTEGER interval;
        interval.QuadPart = -10 * 10'000'000;
        KeDelayExecutionThread(KernelMode, FALSE, &interval);*/
        return STATUS_NOT_SUPPORTED;
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
        RtlSetDaclSecurityDescriptor(sd, TRUE, NULL, FALSE);
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

// Authenticator
namespace MiniFilter
{
    Authenticator::Authenticator(_In_opt_ UNICODE_STRING* RegistryPath) noexcept
    {
        UNREFERENCED_PARAMETER(RegistryPath);
        failable_object<NTSTATUS>::_error = InitCredential();
        Log("Object: %p", this);
    }
    Authenticator::~Authenticator() noexcept  {}

    NTSTATUS Authenticator::InitCredential() noexcept
    {
        UNICODE_STRING KeyPath = RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\BfpDrv\\Parameters");
        UNICODE_STRING ValueName = RTL_CONSTANT_STRING(L"Credential");

        const char* data = "bao1340";
        Hash::SHA256::Hasher hasher;
        hasher.feed(reinterpret_cast<const UINT8*>(data), 7);
        auto hash = hasher.digest();

        auto status = Win32::Registry::SetValue(&KeyPath, &ValueName, REG_BINARY, hash.data, 32);
        if (status != STATUS_SUCCESS)
        {
            Log("Registry::SetValue failed -> Status: %X", status);
            return status;
        }
        RtlCopyMemory(_credential, hash.data, 32);
        return STATUS_SUCCESS;
        /*ULONG ret_size = 0;
        auto status = Win32::Registry::GetValue(&KeyPath, &ValueName, data, 32, &ret_size);
        if (status != STATUS_SUCCESS)
        {
            Log("Registry::GetValue failed -> Status: %X", status);
            return;
        }*/
    }

    bool Authenticator::Authenticate(_In_ const Communication::Credential& Credential) const noexcept
    {
        if (error() != STATUS_SUCCESS)
            return true;
        
        CHAR password[256] = { 0 };
        RtlStringCchCopyNA(password, 256, (CHAR*) Credential.Password, sizeof(Credential.Password));
        auto len = strlen(password);
        Log("Got password: %s", password);

        Hash::SHA256::Hasher hasher;
        hasher.feed(reinterpret_cast<UINT8*>(password), len);
        auto hash = hasher.digest();

        if (RtlCompareMemory(hash.data, _credential, 32) != 32)
            return false;
        return true;
    }
}

// Utils
namespace MiniFilter::Utils
{
    _IRQL_requires_(PASSIVE_LEVEL)
    NTSTATUS GetFileId(
        _In_ PFLT_INSTANCE Instance,
        _In_ PFILE_OBJECT FileObject,
        _Out_ FILE_ID& FileId)
    {
        FileId = {};
        FLT_FILESYSTEM_TYPE type;
        auto status = FltGetFileSystemType(Instance, &type);
        if (status != STATUS_SUCCESS)
            return status;
        if (type == FLT_FSTYPE_REFS)
        {
            FILE_ID_INFORMATION fileIdInformation;
            status = FltQueryInformationFile(
                Instance,                           // Instance
                FileObject,                         // FileObject
                &fileIdInformation,                 // FileInformation
                sizeof(FILE_ID_INFORMATION),        // Length
                FileIdInformation,                  // FileInformationClass
                NULL);                              // ReturnLength
            if (status == STATUS_SUCCESS)
                RtlCopyMemory(&FileId.FileId128, &fileIdInformation.FileId, sizeof(FileId.FileId128));
        }
        else
        {
            FILE_INTERNAL_INFORMATION fileInternalInformation;
            status = FltQueryInformationFile(
                Instance,                           // Instance
                FileObject,                         // FileObject
                &fileInternalInformation,           // FileInformation
                sizeof(FILE_INTERNAL_INFORMATION),  // Length
                FileInternalInformation,            // FileInformationClass
                NULL);                              // ReturnLength
            if (status == STATUS_SUCCESS)
            {
                FileId.FileId64.Value = fileInternalInformation.IndexNumber.QuadPart;
                FileId.FileId64.Reserved = 0LL;
            }
        }
        return status;
    }
}