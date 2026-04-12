# FilterDriver Solution Overview

This repository contains a kernel-mode filter driver solution. The primary project in this workspace is `EvtDrv`, a minifilter-based kernel component that captures file open events, serializes them and forwards them to a user-mode client via a communication port.

---

## Solution overview

The solution contains a single driver project `EvtDrv/` and a set of kernel-friendly utility headers under `EvtDrv/Utilities/`.

Key source files and responsibilities:
- `Entry.cpp` / `Driver` — driver initialization, lifetime management and ownership of core objects.
- `MiniFilter.cpp` / `MiniFilter.hpp` — minifilter registration, pre-op callbacks (e.g., `IRP_MJ_CREATE`), and communication port creation (`FltCreateCommunicationPort`).
- `Worker.cpp` / `Worker.hpp` — kernel worker thread that batches, serializes and sends events to a user-mode client via `FltSendMessage`.
- `Event.hpp` — event object types and serialization logic.
- `Utilities/` — lightweight containers and helpers (e.g., `queue.hpp`, `list.hpp`, `mutex.hpp`, `krn.hpp`).

---

## Runtime data path

1. A file-open occurs and the minifilter pre-operation (`IRP_MJ_CREATE`) callback executes.
2. The minifilter collects file information, constructs an `Event::FileOpenEvent` and calls the global event callback.
3. The global callback pushes the event into `Worker::Queue` and signals the worker thread.
4. `Worker::Worker` pops events, serializes them into a fixed buffer and sends them to the connected user-mode client via `MiniFilter::Connection` (`FltSendMessage`).

---

## Solution-level architecture (mermaid)

```mermaid
flowchart LR
    subgraph Solution[FilterDriver Solution]
        direction TB
        EvtDrv[EvtDrv Project]
    end

    subgraph EvtDrvProj[EvtDrv]
        direction LR
        Entry["Entry.cpp<br/>Driver"]
        Filter["MiniFilter.cpp/h<br/>Filter"]
        WorkerComp["Worker.cpp/h<br/>Worker"]
        QueueComp["Worker::Queue<br/>queue.hpp"]
        Events["Event.hpp<br/>Event types"]
        Utilities["Utilities/ - queue list mutex krn"]
        PortConn["Port / Connection<br/>FltCreateCommunicationPort"]
    end

    Kernel["Kernel / File System"]
    User["User-mode client"]

    Kernel -->|IRP_MJ_CREATE| Filter
    Filter -->|create event| Events
    Events -->|push| QueueComp
    QueueComp -->|signal| WorkerComp
    WorkerComp -->|use| Utilities
    WorkerComp -->|send via| PortConn
    PortConn -->|FltSendMessage| User
    User -->|connect| PortConn

    classDef comp fill:#eef7ff,stroke:#2b6cb0,stroke-width:1px;
    class EvtDrvProj,Entry,Filter,WorkerComp,QueueComp,Events,Utilities,PortConn comp;
```

---

Build & test notes
- Requires Visual Studio + Windows Driver Kit (WDK).
- Build configurations are in the `EvtDrv` project files. Test in a VM with test-signing enabled.

Tested platforms
- Windows 11 (verified in a test VM)

For component-level diagrams and sequence flows see `EvtDrv/readme.md`.
