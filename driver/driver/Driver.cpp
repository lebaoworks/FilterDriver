// Traditional WDM driver: exposes \\.\KFilter so kfilter-compiler can push
// a ruleset via IOCTL_KFILTER_LOAD_RULES. No event capture wired up yet --
// KfilterMatchEvent() below is what a real hook would call.
//
// This file owns all resource management for the active ruleset: pool
// allocation and the EX_RUNDOWN_REF that protects it. kfilter_lib (Rust)
// is pure logic and never touches memory lifetime.
//
// Each load is one KFILTER_GENERATION, swapped into g_ActiveGeneration on
// success (g_LoadMutex only serializes concurrent loads against each
// other; rundown protection handles reader-vs-writer). A reader acquires
// the active generation's rundown ref before touching it; the writer
// waits for the old generation's rundown to drain before freeing it.
#include <ntddk.h>
#include <wdmsec.h>
#include "kfilter_core.h"

#define TAG_RAW     'RwfK'  // "KfwR"
#define TAG_ENTRIES 'tEfK'  // "KfEt"
#define TAG_GEN     'nGfK'  // "KfGn"

typedef struct _KFILTER_GENERATION {
    EX_RUNDOWN_REF RundownRef;
    PVOID Raw;
    SIZE_T RawLen;
    PVOID Entries; // kfilter_data_size() bytes, opaque
} KFILTER_GENERATION, *PKFILTER_GENERATION;

static PKFILTER_GENERATION volatile g_ActiveGeneration = nullptr;
static FAST_MUTEX g_LoadMutex;

DRIVER_UNLOAD KfilterUnload;
DRIVER_DISPATCH KfilterCreateClose;
DRIVER_DISPATCH KfilterDeviceControl;

static VOID
FreeGeneration(
    _In_ PKFILTER_GENERATION Generation
)
{
    if (Generation->Entries != nullptr) {
        ExFreePoolWithTag(Generation->Entries, TAG_ENTRIES);
    }
    if (Generation->Raw != nullptr) {
        ExFreePoolWithTag(Generation->Raw, TAG_RAW);
    }
    ExFreePoolWithTag(Generation, TAG_GEN);
}

// Blocks until every in-flight KfilterMatchEvent call on `Generation`
// finishes, then frees it. PASSIVE_LEVEL only.
static VOID
RetireGeneration(
    _In_ PKFILTER_GENERATION Generation
)
{
    ExWaitForRundownProtectionRelease(&Generation->RundownRef);
    FreeGeneration(Generation);
}

// Per-field entry point a real event hook would call. Safe from any
// IRQL <= DISPATCH_LEVEL, any number of CPUs concurrently.
static LONG
KfilterMatchEvent(
    _In_ KFILTER_OP op,
    _In_ KFILTER_FIELD field,
    _In_reads_bytes_(dataLen) const UCHAR* data,
    _In_ SIZE_T dataLen,
    _Out_opt_ ULONG* matchedState
)
{
    PKFILTER_GENERATION generation = g_ActiveGeneration;
    if (generation == nullptr) {
        return 0; // no ruleset loaded yet
    }
    if (!ExAcquireRundownProtection(&generation->RundownRef)) {
        return 0; // this generation is being retired right now
    }

    LONG rc = kfilter_match(
        generation->Entries,
        static_cast<ULONG>(op),
        static_cast<ULONG>(field),
        data, dataLen,
        matchedState
    );

    ExReleaseRundownProtection(&generation->RundownRef);
    return rc;
}

VOID
KfilterUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    UNICODE_STRING symlinkName;
    RtlInitUnicodeString(&symlinkName, KFILTER_SYMLINK_NAME);
    IoDeleteSymbolicLink(&symlinkName);

    if (DriverObject->DeviceObject != nullptr) {
        IoDeleteDevice(DriverObject->DeviceObject);
    }

    PKFILTER_GENERATION generation = static_cast<PKFILTER_GENERATION>(
        InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(&g_ActiveGeneration), nullptr)
    );
    if (generation != nullptr) {
        RetireGeneration(generation);
    }

    DbgPrint("kfilterdrv: unloading\n");
}

_Use_decl_annotations_
NTSTATUS
KfilterCreateClose(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// Builds a new KFILTER_GENERATION from `input` and installs it as
// g_ActiveGeneration. On failure, everything allocated along the way is
// freed and g_ActiveGeneration is left untouched.
static NTSTATUS
LoadRuleset(
    _In_reads_bytes_(inputLen) const UCHAR* input,
    _In_ SIZE_T inputLen
)
{
    if (input == nullptr) {
        return STATUS_INVALID_PARAMETER;
    }

    PVOID newRaw = ExAllocatePool2(POOL_FLAG_NON_PAGED, inputLen, TAG_RAW);
    if (newRaw == nullptr) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlCopyMemory(newRaw, input, inputLen);
    const UCHAR* raw = static_cast<const UCHAR*>(newRaw);

    SIZE_T entriesBytes = kfilter_data_size();

    PVOID newEntries = nullptr;
    if (entriesBytes != 0) {
        newEntries = ExAllocatePool2(POOL_FLAG_NON_PAGED, entriesBytes, TAG_ENTRIES);
        if (newEntries == nullptr) {
            ExFreePoolWithTag(newRaw, TAG_RAW);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    LONG rc = kfilter_install_from_blob(raw, inputLen, newEntries);
    if (rc != 0) {
        if (newEntries != nullptr) {
            ExFreePoolWithTag(newEntries, TAG_ENTRIES);
        }
        ExFreePoolWithTag(newRaw, TAG_RAW);
        return STATUS_UNSUCCESSFUL;
    }

    PKFILTER_GENERATION newGeneration = static_cast<PKFILTER_GENERATION>(
        ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(KFILTER_GENERATION), TAG_GEN)
    );
    if (newGeneration == nullptr) {
        if (newEntries != nullptr) {
            ExFreePoolWithTag(newEntries, TAG_ENTRIES);
        }
        ExFreePoolWithTag(newRaw, TAG_RAW);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    ExInitializeRundownProtection(&newGeneration->RundownRef);
    newGeneration->Raw = newRaw;
    newGeneration->RawLen = inputLen;
    newGeneration->Entries = newEntries;

    ExAcquireFastMutex(&g_LoadMutex);
    PKFILTER_GENERATION oldGeneration = static_cast<PKFILTER_GENERATION>(
        InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(&g_ActiveGeneration), newGeneration)
    );
    ExReleaseFastMutex(&g_LoadMutex);

    if (oldGeneration != nullptr) {
        RetireGeneration(oldGeneration);
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
KfilterDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;

    switch (stack->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_KFILTER_LOAD_RULES: {
        const UCHAR* buf = static_cast<const UCHAR*>(Irp->AssociatedIrp.SystemBuffer);
        SIZE_T len = stack->Parameters.DeviceIoControl.InputBufferLength;

        status = LoadRuleset(buf, len);
        DbgPrint("kfilterdrv: LoadRuleset(len=%Iu) status=0x%x\n", len, status);
        break;
    }
    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

extern "C" NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    ExInitializeFastMutex(&g_LoadMutex);

    DriverObject->DriverUnload = KfilterUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = KfilterCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = KfilterCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KfilterDeviceControl;

    UNICODE_STRING deviceName;
    RtlInitUnicodeString(&deviceName, KFILTER_DEVICE_NAME);

    PDEVICE_OBJECT deviceObject = nullptr;
    // Restrict the device to SYSTEM + Administrators: this IOCTL replaces
    // the active filter ruleset, so it must not be reachable by an
    // unprivileged process.
    NTSTATUS status = IoCreateDeviceSecure(
        DriverObject,
        0,
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &SDDL_DEVOBJ_SYS_ALL_ADM_ALL,
        nullptr,
        &deviceObject
    );
    if (!NT_SUCCESS(status)) {
        DbgPrint("kfilterdrv: IoCreateDeviceSecure failed 0x%x\n", status);
        return status;
    }

    UNICODE_STRING symlinkName;
    RtlInitUnicodeString(&symlinkName, KFILTER_SYMLINK_NAME);
    status = IoCreateSymbolicLink(&symlinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("kfilterdrv: IoCreateSymbolicLink failed 0x%x\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }

    // Smoke test: proves the FFI link works end to end (rc==0, no
    // ruleset loaded yet).
    static const UCHAR testData[] = "HKLM\\SYSTEM\\CurrentControlSet\\Services\\Evil";
    ULONG matchedState = 0;
    LONG rc = KfilterMatchEvent(
        KFilterOpRegistrySet,
        KFilterFieldKeyPath,
        testData, sizeof(testData) - 1,
        &matchedState
    );
    DbgPrint("kfilterdrv: smoke test KfilterMatchEvent rc=%ld\n", rc);

    DbgPrint("kfilterdrv: loaded, device \\\\.\\KFilter ready\n");
    return STATUS_SUCCESS;
}
