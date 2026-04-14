/********************
*     Includes      *
********************/

#include "ProcessMonitor.hpp"

// Logging vie tracing
#include "trace.h"
#include "ProcessMonitor.tmh"

/*********************
*     Global Vars    *
*********************/
//#pragma data_seg("NONPAGED")
//static Event::EventNotifyCallback GlobalEventCallback = nullptr;
//#pragma data_seg()


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
        ULONG pid = HandleToUlong(PsGetCurrentProcessId());
        ULONG target_pid = HandleToUlong(PsGetProcessId((PEPROCESS)OperationInformation->Object));
        ULONG access = OperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess;

        constexpr ULONG sensitive_access = PROCESS_TERMINATE |
            PROCESS_CREATE_THREAD |
            PROCESS_VM_OPERATION |
            PROCESS_VM_READ |
            PROCESS_VM_WRITE |
            PROCESS_DUP_HANDLE |
            PROCESS_CREATE_PROCESS |
            PROCESS_SET_QUOTA |
            PROCESS_SET_INFORMATION |
            PROCESS_SUSPEND_RESUME |
            PROCESS_SET_LIMITED_INFORMATION;

        // Filter out handle creations to reduce noise
        if ((pid == target_pid) ||                  // Skip handle creations to self
            (access & sensitive_access) == 0)       // Skip handle creations that do not request any sensitive access rights
            return OB_PREOP_SUCCESS;

        //TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Process: process handle creation: ProcessId: %6lu, TargetProcessId: %6lu, DesiredAccess: 0x%08X",pid, target_pid, access);

        auto result = krn::make<Event::ProcessOpenEvent>();
        if (result.status() == STATUS_SUCCESS)
        {
            auto& event = result.value();
            event.ProcessId = pid;
            event.TargetProcessId = target_pid;
            event.DesiredAccess = access;
            krn::unique_ptr<Event::Event> evt(result.release());
            reinterpret_cast<Event::EventNotifyCallback>(RegistrationContext)(evt);
        }
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
}
namespace Process
{
    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Monitor::Monitor(_In_ Event::EventNotifyCallback Callback) noexcept
    {
        auto& status = failable::_status;

        {
            OperationRegistration[0].ObjectType = PsProcessType;   // Set the ObjectType to PsProcessType
            CallbackRegistration.RegistrationContext = Callback;   // Set the RegistrationContext to the callback

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
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Monitor::~Monitor()
    {
        if (this->status() != STATUS_SUCCESS)
            return;

        ObUnRegisterCallbacks(_handle);
    }
}