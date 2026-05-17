/********************
*     Includes      *
********************/

#include "ProcessMonitor.hpp"

// Logging via tracing
#include "trace.h"
#include "ProcessMonitor.tmh"

// Undocumented APIs for listing existing processes at monitor initialization
#include <undocumented.hpp>

/*********************
*    Declarations    *
*********************/

/*********************
*     Global Vars    *
*********************/
#pragma data_seg("NONPAGED")
static Event::EventNotifyCallback GlobalEventCallback = nullptr;
#pragma data_seg()

/*********************
*   Implementations  *
*********************/
namespace Process
{
    #define PROCESS_TERMINATE                  (0x0001)  
    #define PROCESS_CREATE_THREAD              (0x0002)  
    #define PROCESS_SET_SESSIONID              (0x0004)  
    #define PROCESS_VM_OPERATION               (0x0008)  
    #define PROCESS_VM_READ                    (0x0010)  
    #define PROCESS_VM_WRITE                   (0x0020)  
    #define PROCESS_DUP_HANDLE                 (0x0040)  
    #define PROCESS_CREATE_PROCESS             (0x0080)  
    #define PROCESS_SET_QUOTA                  (0x0100)  
    #define PROCESS_SET_INFORMATION            (0x0200)  
    #define PROCESS_QUERY_INFORMATION          (0x0400)  
    #define PROCESS_SUSPEND_RESUME             (0x0800)  
    #define PROCESS_QUERY_LIMITED_INFORMATION  (0x1000)  
    #define PROCESS_SET_LIMITED_INFORMATION    (0x2000)  

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    OB_PREOP_CALLBACK_STATUS ObPre_ProcessHandleCreation (
        _In_ PVOID RegistrationContext,
        _Inout_ POB_PRE_OPERATION_INFORMATION OperationInformation)
    {
        UNREFERENCED_PARAMETER(RegistrationContext);
        UNREFERENCED_PARAMETER(OperationInformation);

        return OB_PREOP_SUCCESS;
    }

    #pragma data_seg("NONPAGED")
    OB_OPERATION_REGISTRATION OperationRegistration[] = {
        {
            .ObjectType = NULL,                             // Place holder for PsProcessType, cause PsProcessType is exported at runtime
            .Operations = OB_OPERATION_HANDLE_CREATE,       // Register for handle creation
            .PreOperation = ObPre_ProcessHandleCreation,    // No pre-operation callback
            .PostOperation = NULL                           // No post-operation callback, we will filter in the pre-operation callback
        }
    };

    OB_CALLBACK_REGISTRATION CallbackRegistration = {
        .Version = OB_FLT_REGISTRATION_VERSION,             
        .OperationRegistrationCount = sizeof(OperationRegistration) / sizeof(OperationRegistration[0]),
        .Altitude = RTL_CONSTANT_STRING(L"370160"),
        .RegistrationContext = NULL,
        .OperationRegistration = OperationRegistration,
    };
    #pragma data_seg()

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    VOID CreateProcessNotify(
        _Inout_ PEPROCESS Process,
        _In_ HANDLE ProcessId,
        _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo)
    {
        // If process is being created
        if (CreateInfo != NULL)
        {
            ULONG pid = HandleToUlong(ProcessId);
            TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Process: process create: ProcessId: %6lu, ImageName: %wZ", pid, CreateInfo->ImageFileName);

            auto result = krn::make<Event::ProcessCreateEvent>();
            if (result.status() == STATUS_SUCCESS)
            {
                auto& event = result.value();
                event.TimeStamp.QuadPart = PsGetProcessCreateTimeQuadPart(Process);
                event.ProcessId = pid;
                event.ParentProcessId = HandleToUlong(CreateInfo->ParentProcessId);
                event.ImageName = *CreateInfo->ImageFileName;
                if (CreateInfo->CommandLine)
                    event.CommandLine = *CreateInfo->CommandLine;

                krn::unique_ptr<Event::Event> evt(result.release());
                GlobalEventCallback(evt);
            }
        }
        // If process is being terminated
        else
        {
            ULONG pid = HandleToUlong(ProcessId);
            TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Process: process exit: ProcessId: %6lu", pid);
            auto result = krn::make<Event::ProcessExitEvent>();
            if (result.status() == STATUS_SUCCESS)
            {
                auto& event = result.value();
                event.ProcessId = pid;
                event.ProcessCreationTime.QuadPart = PsGetProcessCreateTimeQuadPart(Process);

                krn::unique_ptr<Event::Event> evt(result.release());
                GlobalEventCallback(evt);
            }
        }
    }

    _IRQL_requires_max_(APC_LEVEL)
    _IRQL_requires_same_
    VOID CreateThreadNotify(
        _In_ HANDLE ProcessId,
        _In_ HANDLE ThreadId,
        _In_ BOOLEAN Create)
    {
        // Creation only
        if (Create == FALSE)
            return;

        // Routine run in the context of the thread that created the new thread
        
        // Skip safe thread creations to reduce noise
        ULONG current_pid = HandleToUlong(PsGetCurrentProcessId());
        if (current_pid == 4 ||                         // System thread or process's inital thread,
            current_pid == HandleToULong(ProcessId))    // Skip thread creations to self
            return;

        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Process: remote thread create: ProcessId: %6lu, TargetProcessId: %6lu, ThreadId: %6lu", HandleToUlong(PsGetCurrentProcessId()), HandleToUlong(ProcessId), HandleToUlong(ThreadId));

        auto result = krn::make<Event::RemoteThreadCreateEvent>();
        if (result.status() == STATUS_SUCCESS)
        {
            auto& event = result.value();
            event.ProcessId = current_pid;
            event.TargetProcessId = HandleToUlong(ProcessId);
            event.ThreadId = HandleToUlong(ThreadId);

            krn::unique_ptr<Event::Event> evt(result.release());
            GlobalEventCallback(evt);
        }
    }
}
namespace Process
{
    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Monitor::Monitor(_In_ Event::EventNotifyCallback Callback) noexcept
    {
        GlobalEventCallback = Callback;
        auto& status = failable::_status;

        {
            OperationRegistration[0].ObjectType = PsProcessType;   // Set the ObjectType to PsProcessType

            // Register the callbacks
            // [NOTE] If ObRegisterCallbacks return STATUS_ACCESS_DEINED, add flag /INTEGRITYCHECK in linker settings.
            //      Refer: https://stackoverflow.com/a/78987826
            status = ObRegisterCallbacks(&CallbackRegistration, &_handle);
            if (status != STATUS_SUCCESS)
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Register handle callbacks failed -> status: %!STATUS!", status);
                return;
            }
        }
        defer{ if (status != STATUS_SUCCESS) ObUnRegisterCallbacks(_handle); };

        {
            status = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotify, FALSE);
            if (status != STATUS_SUCCESS)
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Register process notify routine failed -> status: %!STATUS!", status);
                return;
            }
        }
        defer{ if (status != STATUS_SUCCESS) PsSetCreateProcessNotifyRoutineEx(CreateProcessNotify, TRUE); };

        {
            status = PsSetCreateThreadNotifyRoutine(CreateThreadNotify);
            if (status != STATUS_SUCCESS)
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Register thread notify routine failed -> status: %!STATUS!", status);
                return;
            }
        }
        defer{ if (status != STATUS_SUCCESS) PsRemoveCreateThreadNotifyRoutine(CreateThreadNotify); };
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Monitor::~Monitor()
    {
        if (this->status() != STATUS_SUCCESS)
            return;

        PsRemoveCreateThreadNotifyRoutine(CreateThreadNotify);
        PsSetCreateProcessNotifyRoutineEx(CreateProcessNotify, TRUE);
        ObUnRegisterCallbacks(_handle);
    }
}
