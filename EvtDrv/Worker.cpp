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

#define MAX_EVENT_QUEUE_SIZE 1000
#define SERIALIZED_BUFFER_SIZE (512 * 1024) // 512 KB

#pragma warning(disable : 4200)
namespace
{
    struct Header
    {
        ULONG TotalSize = sizeof(Header);
        BYTE Data[0]; // Flexible array member for serialized event data
    };
    static_assert(sizeof(Header) < SERIALIZED_BUFFER_SIZE, "Header size must be less than the total serialized buffer size");
}

/*********************
*   Implementations  *
*********************/

namespace Worker
{
    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Queue::Queue() noexcept
    {
        auto& status = krn::failable::_status;
        
        status = ExInitializeResourceLite(&_lock);
        if (status != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Worker: Failed to initialize queue lock -> status: %!STATUS!", status);
            return;
        }
        KeInitializeEvent(&_push_event, NotificationEvent, FALSE);
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Queue::~Queue()
    {
        if (status() != STATUS_SUCCESS)
            return; // If constructor failed, we may be in a partially initialized state, only clean up what was initialized

        ExDeleteResourceLite(&_lock);
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    NTSTATUS Queue::Push(_Inout_ krn::unique_ptr<Event::Event>& event) noexcept
    {
        ExAcquireResourceExclusiveLite(&_lock, TRUE);
        defer{ ExReleaseResourceLite(&_lock); };

        if (_events.size() >= MAX_EVENT_QUEUE_SIZE)
        {
            //TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "Worker: Event queue is full");
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
    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Worker::Worker(Queue& queue) noexcept
        : _queue(queue)
    {
        auto& status = krn::failable::_status;

        // Initialize serialized buffer
        _serialized_buffer = Worker::operator new(SERIALIZED_BUFFER_SIZE);
        if (_serialized_buffer == NULL)
        {
            status = STATUS_NO_MEMORY;
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Worker: Failed to allocate serialized buffer -> status: %!STATUS!", status);
            return;
        }
        defer{ if (status != STATUS_SUCCESS) { Worker::operator delete(_serialized_buffer); _serialized_buffer = NULL; } };
        
        // Initialize connection management
        status = ExInitializeResourceLite(&_lock);
        if (status != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Worker: Failed to initialize connection lock -> status: %!STATUS!", status);
            return;
        }
        defer{ if (status != STATUS_SUCCESS) ExDeleteResourceLite(&_lock); };
        KeInitializeEvent(&_connect_event, SynchronizationEvent, FALSE);
        
        // Create worker system thread
        KeInitializeEvent(&_stop_event, NotificationEvent, FALSE);
        HANDLE hThread;
        status = PsCreateSystemThread(
            &hThread,               // ThreadHandle
            THREAD_ALL_ACCESS,      // DesiredAccess
            NULL,                   // ObjectAttributes
            NULL,                   // ProcessHandle
            NULL,                   // ClientId
            Routine,                // StartRoutine
            this);                  // StartContext  
        if (status != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Worker: Failed to create system thread -> status: %!STATUS!", status);
            return;
        }
        defer{ if (status != STATUS_SUCCESS) { KeSetEvent(&_stop_event, 0, FALSE); ZwWaitForSingleObject(hThread, FALSE, NULL); ZwClose(hThread); } };

        // Get thread object from handle for later synchronization when stopping the worker
        status = ObReferenceObjectByHandle(
            hThread,                        // Handle
            THREAD_ALL_ACCESS,              // DesiredAccess
            *PsThreadType,                  // ObjectType
            KernelMode,                     // AccessMode
            (PVOID*)&_worker_object,        // Object
            NULL);                          // HandleInformation
        if (status != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Worker: Failed to reference thread object -> status: %!STATUS!", status);
            return;
        };
        // We have the thread object, close the handle
        ZwClose(hThread);
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Worker::~Worker()
    {
        if (status() != STATUS_SUCCESS)
            return; // If constructor failed, we may be in a partially initialized state, only clean up what was initialized

        // Signal worker to stop
        KeSetEvent(&_stop_event, 0, FALSE);

        // Wait for thread to exit
        KeWaitForSingleObject(_worker_object, Executive, KernelMode, FALSE, NULL);
        ObDereferenceObject(_worker_object);
        
        ExDeleteResourceLite(&_lock);

        Worker::operator delete(_serialized_buffer);
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    NTSTATUS Worker::ConnectNotify(_Inout_ krn::unique_ptr<MiniFilter::Connection>& connection) noexcept
    {
        ExAcquireResourceExclusiveLite(&_lock, TRUE);
        defer{ ExReleaseResourceLite(&_lock); };

        _connection = std::move(connection);
        KeSetEvent(&_connect_event, 0, FALSE); // Signal worker of new connection
        return STATUS_SUCCESS;
    }

    _IRQL_requires_same_
    _Function_class_(KSTART_ROUTINE)
    VOID Worker::Routine(_In_ PVOID Context)
    {
        auto& worker = *reinterpret_cast<Worker*>(Context);
        auto& queue = worker._queue;
        defer{ PsTerminateSystemThread(STATUS_SUCCESS); };

        Header* header = (Header*)worker._serialized_buffer;
        BYTE* data = header->Data;

        while (true)
        {
            PVOID events[] = { &worker._stop_event, &worker._connect_event };
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
            {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Worker: Stop event signaled, exiting worker thread");
                return;
            }
            if (status == STATUS_WAIT_0 + 1) // _connect_event signaled
            {
                ExAcquireResourceExclusiveLite(&worker._lock, TRUE);
                krn::unique_ptr<MiniFilter::Connection> connection(std::move(worker._connection));
                ExReleaseResourceLite(&worker._lock);

                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Worker: Connect event signaled, processing connection");
                while (true)
                {
                    LARGE_INTEGER timeout;
                    timeout.QuadPart = -1 * 1000 * 1000 * 10; // 1 second
                    PVOID events2[] = { &worker._stop_event, &queue._push_event };
                    auto status2 = KeWaitForMultipleObjects(
                        2,          // Count
                        events2,     // Objects to wait on
                        WaitAny,    // Wait type
                        Executive,  // Wait reason
                        KernelMode, // Wait mode
                        FALSE,      // Alertable
                        &timeout,   // Timeout
                        NULL        // Wait block array
                    );

                    if (status2 == STATUS_WAIT_0) // WorkerStopEvent signaled
                    {
                        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Worker: Stop event signaled, exiting worker thread");
                        return;
                    }

                    if (status2 == STATUS_WAIT_0 + 1) // PushEvent signaled
                    {
                        // Reset event
                        KeResetEvent(&queue._push_event);

                        // Serialize as many events as possible
                        ExAcquireResourceExclusiveLite(&queue._lock, TRUE);
                        defer{ ExReleaseResourceLite(&queue._lock); };

                        while (!queue._events.empty())
                        {
                            auto& event = queue._events.front();
                            if (event->SerializedSize() > SERIALIZED_BUFFER_SIZE - header->TotalSize)
                                break; // No more events can fit in the buffer, process what we have so far

                            event->Serialize(data, SERIALIZED_BUFFER_SIZE - header->TotalSize);
                            data += event->SerializedSize();
                            header->TotalSize += event->SerializedSize();
                            queue._events.pop();
                        }
                    }

                    if (header->TotalSize != sizeof(Header)) // We have events to send
                    {
                        TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Worker: Sending %d bytes to client", header->TotalSize);

                        auto send_status = connection->Send(worker._serialized_buffer, header->TotalSize);
                        if (send_status != STATUS_SUCCESS)
                        {
                            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Worker: Failed to send message to client -> status: %!STATUS!", send_status);
                            break; // Connection likely closed, exit inner loop to wait for next connection
                        }

                        // Reset buffer state after successful send
                        *header = Header();
                        data = header->Data;
                    }
                }
            }
        }

    }
}