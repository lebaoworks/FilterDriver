/********************
*     Includes      *
********************/
#include "Worker.hpp"

// Logging vie tracing
#include "trace.h"
#include "Worker.tmh"

///*********************
//*    Declarations    *
//*********************/
//
//VOID WorkerRoutine(_In_ PVOID Context);
//
///*********************
//*     Global Vars    *
//*********************/
//
//#pragma data_seg("NONPAGED")

//

//
//// Worker thread
//static HANDLE WorkerHandle = NULL;
//static KEVENT WorkerStopEvent;
//
//#pragma data_seg()
//
///*********************
//*   Implementations  *
//*********************/
//
//NTSTATUS Worker::Initialize() noexcept
//{
//    
//}
//
//_IRQL_requires_(PASSIVE_LEVEL)
//_IRQL_requires_same_
//void Worker::Uninitialize() noexcept
//{
//    // Signal worker to stop and wait for thread to exit
//    KeSetEvent(&WorkerStopEvent, 0, FALSE);
//    if (WorkerHandle != NULL)
//    {
//        ZwWaitForSingleObject(WorkerHandle, FALSE, NULL);
//        ZwClose(WorkerHandle);
//        WorkerHandle = NULL;
//    }
//
//    // Free all events in the queue
//    ExAcquireResourceExclusiveLite(&EventsLock, TRUE);
//    defer{ ExReleaseResourceLite(&EventsLock); };
//    delete Events;
//    Events = nullptr;
//    ExDeleteResourceLite(&EventsLock);
//
//    // Free serialized buffer
//    ExFreePool(SerializedBuffer);
//    SerializedBuffer = NULL;
//}
//

//
//
//VOID WorkerRoutine(_In_ PVOID Context)
//{
//    UNREFERENCED_PARAMETER(Context);
//    defer{ PsTerminateSystemThread(STATUS_SUCCESS); };
//
//    BYTE* ptr = (BYTE*)SerializedBuffer;
//    ULONG remaining_size = 512 * 1024;
//
//    while (true)
//    {
//        PVOID events[] = { &WorkerStopEvent, &ConnectEvent };
//        auto status = KeWaitForMultipleObjects(
//            2,          // Count
//            events,     // Objects to wait on
//            WaitAny,    // Wait type
//            Executive,  // Wait reason
//            KernelMode, // Wait mode
//            FALSE,      // Alertable
//            NULL,       // Timeout
//            NULL        // Wait block array
//        );
//        
//        if (status == STATUS_WAIT_0) // WorkerStopEvent signaled
//            break;
//        if (status == STATUS_WAIT_0 + 1) // ConnectEvent signaled
//        {
//            ExAcquireResourceExclusiveLite(&ConnectionLock, TRUE);
//            std::unique_ptr<MiniFilter::Connection> connection(Connection);
//            Connection = nullptr;
//            ExReleaseResourceLite(&ConnectionLock);
//
//            while (true)
//            {
//                LARGE_INTEGER timeout;
//                timeout.QuadPart = -1 * 1000 * 1000 * 10; // 1 second
//
//                PVOID events2[] = { &WorkerStopEvent, &PushEvent };
//                auto status2 = KeWaitForMultipleObjects(
//                    2,          // Count
//                    events2,     // Objects to wait on
//                    WaitAny,    // Wait type
//                    Executive,  // Wait reason
//                    KernelMode, // Wait mode
//                    FALSE,      // Alertable
//                    &timeout,   // Timeout
//                    NULL        // Wait block array
//                );
//                if (status2 == STATUS_WAIT_0) // WorkerStopEvent signaled
//                    return;
//                if (status2 == STATUS_WAIT_0 + 1) // PushEvent signaled
//                {
//                    KeResetEvent(&PushEvent); // Reset event
//                    // Serialize as many events as possible
//                    ExAcquireResourceExclusiveLite(&EventsLock, TRUE);
//                    while (!Events->empty())
//                    {
//                        auto& event = Events->front();
//                        if (event->SerializedSize() > remaining_size)
//                            break; // No more events can fit in the buffer, process what we have so far
//                        event->Serialize(ptr, remaining_size);
//                        ptr += event->SerializedSize();
//                        remaining_size -= event->SerializedSize();
//                        Events->pop();
//                    }
//                    ExReleaseResourceLite(&EventsLock);
//                }
//                if (remaining_size != 512 * 1024) // We have events to send
//                {
//                    auto send_status = connection->SendMessage(SerializedBuffer, 512 * 1024 - remaining_size);
//                    if (send_status != STATUS_SUCCESS)
//                    {
//                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Worker: Failed to send message to client -> status: %!STATUS!", send_status);
//                        break;
//                    }
//                }
//            }
//        }
//    }
//    
//}

namespace Worker
{
    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Queue::Queue() noexcept
    {
        ExInitializeResourceLite(&_lock);
        KeInitializeEvent(&_push_event, NotificationEvent, FALSE);
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Queue::~Queue()
    {
        ExDeleteResourceLite(&_lock);
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    NTSTATUS Queue::Push(_Inout_ krn::unique_ptr<Event::Event>& event) noexcept
    {
        ExAcquireResourceExclusiveLite(&_lock, TRUE);
        defer{ ExReleaseResourceLite(&_lock); };

        if (_events.size() >= 10000)
        {
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "Worker: Event queue is full");
            return STATUS_TOO_MANY_NODES;
        }

        auto old_size = _events.size();
        _events.push(std::move(event));
        if (_events.size() == old_size)
            return STATUS_NO_MEMORY;

        // Signal worker there is data to process
        KeSetEvent(&_push_event, 0, FALSE);

        return STATUS_SUCCESS;
    }
}

namespace Worker
{
    Worker::Worker() noexcept
    {
        //auto& status = krn::failable::_status;

        // Initialize serialized buffer
        //_serialized_buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, 512*1024, 'EVT1');
        //if (_serialized_buffer == NULL)
        //{
        //    status = STATUS_NO_MEMORY;
        //    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Worker: Failed to allocate serialized buffer -> status: %!STATUS!", status);
        //    return;
        //}
        //defer{ if (status != STATUS_SUCCESS) { ExFreePool( _serialized_buffer); _serialized_buffer = NULL; } };
        
        // Initialize connection
        //ExInitializeResourceLite(&ConnectionLock);
        //KeInitializeEvent(&ConnectEvent, SynchronizationEvent, FALSE);
        //
        //    // Create worker system thread
        //    KeInitializeEvent(&WorkerStopEvent, NotificationEvent, FALSE);
        //    status = PsCreateSystemThread(&WorkerHandle, THREAD_ALL_ACCESS, NULL, NULL, NULL, WorkerRoutine, NULL);
        //    if (status != STATUS_SUCCESS)
        //    {
        //        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Worker: Failed to create system thread -> status: %!STATUS!", status);
        //        return status;
        //    }
        //
        //    return status;
    }
    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Worker::~Worker()
    {

    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    NTSTATUS Worker::ConnectNotify(_Inout_ krn::unique_ptr<MiniFilter::Connection>& connection) noexcept
    {
        _connection = std::move(connection);
        return STATUS_SUCCESS;
    }
}