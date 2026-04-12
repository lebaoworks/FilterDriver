# EvtDrv — Filter Driver Utilities

This document summarizes the purpose of the `EvtDrv` component and lists coding rules and conventions followed across the source code.

## Purpose

`EvtDrv` implements a kernel-mode filter driver worker component responsible for collecting events (for example, file open events), serializing them and forwarding them to a user-mode client over a connection.

## Document Structure

This section describes the component structure and the main control flow of `EvtDrv` (a minifilter + worker) in a concise text form with accompanying `mermaid` diagrams.

Short overview:
- `DriverEntry` (defined in `Entry.cpp`) initializes and manages a `Driver` object that is responsible for creating and owning:
  - `Queue` (`Worker::Queue`) — a kernel event queue holding `Event::Event` instances (for example, `Event::FileOpenEvent`).
  - `Worker` (`Worker::Worker`) — a kernel thread that batches, serializes and sends events to a user-mode client when a `Connection` is available.
  - `MiniFilter::Filter` — registers minifilter callbacks (e.g., for `IRP_MJ_CREATE`) and installs a `GlobalEventCallback` to forward events into the `Queue`.
  - `MiniFilter::Port` — a communication port used by user-mode applications to connect; a `MiniFilter::Connection` object is created per client connection.

Main components and their relationships (structure diagram):

```mermaid
classDiagram
    class Driver {
      +_filter: MiniFilter::Filter*
      +_port: MiniFilter::Port*
      +_queue: Worker::Queue*
      +_worker: Worker::Worker*
    }

    class Queue {
      +_events: krn::queue<unique_ptr<Event::Event>>
      +_lock: ERESOURCE
      +_push_event: KEVENT
      +Push(event)
    }

    class Worker {
      +_serialized_buffer: PVOID
      +_connection: unique_ptr<MiniFilter::Connection>
      +_worker_object: PETHREAD
      +_stop_event: KEVENT
      +Routine()
    }

    class MiniFilterFilter {
      +FltRegisterFilter()
      +PreOperation callbacks (IRP_MJ_CREATE)
    }

    class Port {
      +FltCreateCommunicationPort()
      +ConnectNotify -> creates Connection
    }

    class Connection {
      +Send(Buffer, Size)
    }

    class Event {
      +Type
      +TimeStamp
      +Serialize()
    }

    Driver --> Queue : owns
    Driver --> Worker : owns
    Driver --> MiniFilterFilter : owns
    Driver --> Port : owns
    Queue --> Event : stores
    Worker --> Connection : holds (when connected)
    Port --> Connection : creates
    MiniFilterFilter --> Queue : pushes events via GlobalEventCallback
    Worker --> Queue : pops events
```

Primary runtime sequence when a file open occurs and a user-mode client is connected:

```mermaid
sequenceDiagram
    participant Kernel as Kernel (File Open)
    participant MiniFilter as `MiniFilter::Filter`
    participant Queue as `Worker::Queue`
    participant Worker as `Worker::Worker`
    participant Port as `MiniFilter::Port`
    participant Conn as `MiniFilter::Connection`
    participant User as User-mode client

    Kernel->>MiniFilter: IRP_MJ_CREATE (PreCreate)
    MiniFilter->>MiniFilter: FltGetFileNameInformation()
    MiniFilter->>Event: create `Event::FileOpenEvent`
    MiniFilter->>Queue: GlobalEventCallback -> `Queue::Push(evt)`
    Queue->>Worker: signal `_push_event`

    note over User,Port: (separately) User-mode connects to `\\EvtDrvPort`
    User->>Port: Connect
    Port->>Conn: create `MiniFilter::Connection`
    Port->>Worker: ConnectNotify -> `Worker::ConnectNotify(conn)`
    Worker->>Worker: set `_connect_event`

    Worker->>Queue: wait for `_push_event` or timeout
    Worker->>Queue: pop events, serialize into `_serialized_buffer`
    Worker->>Conn: `Connection::Send(buffer, size)` (FltSendMessage)
    Conn->>User: deliver message to user-mode client

    alt send fails
      Worker->>Worker: assume connection closed, break to wait for next connection
    end
```


## Ideas & Decisions Log

### Minifilter Object Management: Comparison of Approaches

| Feature | Global / Static Variable | Filter Context (FltMgr) |
| :--- | :--- | :--- |
| **Performance** | **Extremely Fast (O(1))**: Direct memory access with zero computational overhead. | **Moderate**: Involves lookup overhead and Reference Counting (Atomic Inc/Dec). |
| **Encapsulation** | **Low**: Can lead to "spaghetti code" and makes unit testing or modularity difficult. | **High**: Follows the Minifilter Framework architecture and promotes clean OOP design. |
| **Safety** | **Manual**: Developer must manually handle initialization, destruction, and synchronization. | **Automatic**: The framework manages the lifecycle (Cleanup/Delete) via registered callbacks. |
| **Scalability** | **Poor**: Difficult to manage if the driver needs to handle multiple filter instances independently. | **Excellent**: Easily maps specific objects to corresponding Filter, Instance, or Volume handles. |


**Decision:** This project utilizes a **Global Static Singleton** pattern for object management to eliminate lookup overhead and maximize I/O throughput in high-performance scenarios.

