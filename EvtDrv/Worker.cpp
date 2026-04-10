/********************
*     Includes      *
********************/
#include "Worker.hpp"

// Logging vie tracing
#include "trace.h"
#include "Worker.tmh"

/*********************
*    Declarations    *
*********************/

VOID WorkerRoutine(_In_ PVOID Context);

/*********************
*     Global Vars    *
*********************/

#pragma data_seg("NONPAGED")
// Queue to hold events for the worker to process
static std::queue<std::unique_ptr<Event>>* Events;
static ERESOURCE EventsLock;
static KEVENT PushEvent;

// Serialized buffer for the worker thread
static PVOID SerializedBuffer = NULL;

// Current active connection to the filter (if any)
static MiniFilter::Connection* Connection = nullptr;
static ERESOURCE ConnectionLock;
static KEVENT ConnectEvent;

// Worker thread
static HANDLE WorkerHandle = NULL;
static KEVENT WorkerStopEvent;

#pragma data_seg()

/*********************
*   Implementations  *
*********************/

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
NTSTATUS Worker::Initialize() noexcept
{
    NTSTATUS status = STATUS_SUCCESS;

    // Initialize queue
    Events = new std::queue<std::unique_ptr<Event>>();
    if (Events == nullptr)
    {
        status = STATUS_NO_MEMORY;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Worker: Failed to allocate events queue -> status: %!STATUS!", status);
        return status;
    }
    defer{ if (status != STATUS_SUCCESS) { delete Events; Events = nullptr; } };
    ExInitializeResourceLite(&EventsLock);
    KeInitializeEvent(&PushEvent, NotificationEvent, FALSE);

    // Initialize serialized buffer
    SerializedBuffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, 512*1024, 'bufT');
    if (SerializedBuffer == NULL)
    {
        status = STATUS_NO_MEMORY;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Worker: Failed to allocate serialized buffer -> status: %!STATUS!", status);
        return status;
    }
    defer{ if (status != STATUS_SUCCESS) { ExFreePool(SerializedBuffer); SerializedBuffer = NULL; } };

    // Initialize connection
    ExInitializeResourceLite(&ConnectionLock);
    KeInitializeEvent(&ConnectEvent, SynchronizationEvent, FALSE);

    // Create worker system thread
    KeInitializeEvent(&WorkerStopEvent, NotificationEvent, FALSE);
    status = PsCreateSystemThread(&WorkerHandle, THREAD_ALL_ACCESS, NULL, NULL, NULL, WorkerRoutine, NULL);
    if (status != STATUS_SUCCESS)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Worker: Failed to create system thread -> status: %!STATUS!", status);
        return status;
    }

    return status;
}

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
void Worker::Uninitialize() noexcept
{
    // Signal worker to stop and wait for thread to exit
    KeSetEvent(&WorkerStopEvent, 0, FALSE);
    if (WorkerHandle != NULL)
    {
        ZwWaitForSingleObject(WorkerHandle, FALSE, NULL);
        ZwClose(WorkerHandle);
        WorkerHandle = NULL;
    }

    // Free all events in the queue
    ExAcquireResourceExclusiveLite(&EventsLock, TRUE);
    defer{ ExReleaseResourceLite(&EventsLock); };
    delete Events;
    Events = nullptr;
    ExDeleteResourceLite(&EventsLock);

    // Free serialized buffer
    ExFreePool(SerializedBuffer);
    SerializedBuffer = NULL;
}

NTSTATUS Worker::Push(_Inout_ std::unique_ptr<Event>& event) noexcept
{
    ExAcquireResourceExclusiveLite(&EventsLock, TRUE);
    defer{ ExReleaseResourceLite(&EventsLock); };

    if (Events->size() >= 10000)
        return STATUS_TOO_MANY_NODES;

    auto old_size = Events->size();
    Events->push(std::move(event));
    if (Events->size() == old_size)
        return STATUS_NO_MEMORY;

    // Signal worker there is data to process
    KeSetEvent(&PushEvent, 0, FALSE);

    return STATUS_SUCCESS;
}

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS Worker::ConnectNotify(_Inout_ std::unique_ptr<MiniFilter::Connection>& connection)
{
    ExAcquireResourceExclusiveLite(&ConnectionLock, TRUE);
    defer{ ExReleaseResourceLite(&ConnectionLock); };

    // Close previous connection if it exists
    if (Connection != nullptr) delete Connection; 

    Connection = connection.release();
    KeSetEvent(&PushEvent, 0, FALSE); // Signal worker to send any pending events to the new connection

    return STATUS_SUCCESS;
}

VOID WorkerRoutine(_In_ PVOID Context)
{
    UNREFERENCED_PARAMETER(Context);
    defer{ PsTerminateSystemThread(STATUS_SUCCESS); };

    BYTE* ptr = (BYTE*)SerializedBuffer;
    ULONG remaining_size = 512 * 1024;

    while (true)
    {
        PVOID events[] = { &WorkerStopEvent, &ConnectEvent };
        auto status = KeWaitForMultipleObjects(
            2,          // Count
            events,     // Objects to wait on
            WaitAny,    // Wait type
            Executive,  // Wait reason
            KernelMode, // Wait mode
            FALSE,      // Alertable
            NULL,       // Timeout
            NULL        // Wait block array
        );
        
        if (status == STATUS_WAIT_0) // WorkerStopEvent signaled
            break;
        if (status == STATUS_WAIT_0 + 1) // ConnectEvent signaled
        {
            ExAcquireResourceExclusiveLite(&ConnectionLock, TRUE);
            std::unique_ptr<MiniFilter::Connection> connection(Connection);
            Connection = nullptr;
            ExReleaseResourceLite(&ConnectionLock);

            while (true)
            {
                LARGE_INTEGER timeout;
                timeout.QuadPart = -1 * 1000 * 1000 * 10; // 1 second

                PVOID events2[] = { &WorkerStopEvent, &PushEvent };
                auto status2 = KeWaitForMultipleObjects(
                    2,          // Count
                    events,     // Objects to wait on
                    WaitAny,    // Wait type
                    Executive,  // Wait reason
                    KernelMode, // Wait mode
                    FALSE,      // Alertable
                    &timeout,   // Timeout
                    NULL        // Wait block array
                );
                if (status2 == STATUS_WAIT_0) // WorkerStopEvent signaled
                    return;
                if (status2 == STATUS_WAIT_0 + 1) // PushEvent signaled
                {
                    // Serialize as many events as possible
                    ExAcquireResourceExclusiveLite(&EventsLock, TRUE);
                    while (!Events->empty())
                    {
                        auto& event = Events->front();
                        if (event->SerializedSize() > remaining_size)
                            break; // No more events can fit in the buffer, process what we have so far
                        event->Serialize(ptr, remaining_size);
                        ptr += event->SerializedSize();
                        remaining_size -= event->SerializedSize();
                        Events->pop();
                    }
                    ExReleaseResourceLite(&EventsLock);
                }
                if (remaining_size != 512 * 1024) // We have events to send
                {
                    auto send_status = connection->SendMessage(SerializedBuffer, 512 * 1024 - remaining_size);
                    if (send_status != STATUS_SUCCESS)
                    {
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Worker: Failed to send message to client -> status: %!STATUS!", send_status);
                        break;
                    }
                }
            }
        }
    }
    
}