# EvtDrv — Filter Driver Utilities

This document summarizes the purpose of the `EvtDrv` component and lists coding rules and conventions followed across the source code.

## Purpose

`EvtDrv` implements a kernel-mode filter driver worker component responsible for collecting events (for example, file open events), serializing them and forwarding them to a user-mode client over a connection.

## Ideas & Decisions Log

### Minifilter Object Management: Comparison of Approaches

| Feature | Global / Static Variable | Filter Context (FltMgr) |
| :--- | :--- | :--- |
| **Performance** | **Extremely Fast (O(1))**: Direct memory access with zero computational overhead. | **Moderate**: Involves lookup overhead and Reference Counting (Atomic Inc/Dec). |
| **Encapsulation** | **Low**: Can lead to "spaghetti code" and makes unit testing or modularity difficult. | **High**: Follows the Minifilter Framework architecture and promotes clean OOP design. |
| **Safety** | **Manual**: Developer must manually handle initialization, destruction, and synchronization. | **Automatic**: The framework manages the lifecycle (Cleanup/Delete) via registered callbacks. |
| **Scalability** | **Poor**: Difficult to manage if the driver needs to handle multiple filter instances independently. | **Excellent**: Easily maps specific objects to corresponding Filter, Instance, or Volume handles. |


**Decision:** This project utilizes a **Global Static Singleton** pattern for object management to eliminate lookup overhead and maximize I/O throughput in high-performance scenarios.

