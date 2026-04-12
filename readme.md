# FilterDriver Solution Overview

This repository contains a kernel-mode filter driver solution. The primary project in this workspace is `EvtDrv`, a minifilter-based kernel component that captures file open events, serializes them and forwards them to a user-mode client via a communication port.

---

## High-level components

* **`Entry.cpp` / `Driver`**: Driver entry point and lifetime management. Initializes and owns the main components at boot (queue, worker, filter, port).
* **`MiniFilter.cpp` / `MiniFilter.hpp`**: Minifilter registration, pre-operation callback for `IRP_MJ_CREATE`, creation of `Event::FileOpenEvent`, and communication port management.
* **`Worker.cpp` / `Worker.hpp`**: Kernel worker thread that batches and serializes events and sends them to a connected user-mode client using `MiniFilter::Connection`.
* **`Event.hpp`**: Event object hierarchy and serialization (`Event::Event`, `Event::FileOpenEvent`).
* **`Utilities/`**: Small kernel-friendly utility containers and helpers (queues, pointer, etc.).

---

## Runtime data path (concise)

1. **File open** occurs in the kernel $\rightarrow$ minifilter pre-op callback runs.
2. **Minifilter** builds an `Event::Event` and forwards it to `Worker::Queue` (via a global callback).
3. **`Worker::Worker`** wakes, pops events from `Worker::Queue`, serializes them into a fixed buffer and sends the buffer to the connected user-mode client through `MiniFilter::Connection` (calls `FltSendMessage`).

---

## General architecture diagram

```mermaid
flowchart LR
    Driver[Driver]
    subgraph Core
        Driver --> Queue[Worker::Queue]
        Driver --> Worker[Worker::Worker]
        Driver --> Filter[MiniFilter::Filter]
        Driver --> Port[MiniFilter::Port]
    end

    Filter -->|creates| Event[Event::Event]
    Event -->|pushes to| Queue
    Queue -->|signals| Worker
    Port -->|on connect creates| Connection[MiniFilter::Connection]
    Worker -->|sends via| Connection
    Connection -->|FltSendMessage| User[User-mode client]

    classDef kernel fill:#f2f9ff,stroke:#333,stroke-width:1px;
    class Driver,Filter,kernel;
```

---

Tested platforms
- Windows 11 (verified in a test VM)

More detailed component documentation and diagrams are available in `EvtDrv/readme.md`.
