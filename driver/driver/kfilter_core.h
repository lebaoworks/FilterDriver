// Declarations for the kfilter_lib Rust staticlib (kfilter-lib/src/lib.rs,
// built with `cargo build --release --features kernel`). x64 only: Rust's
// `extern "system"` and C's default calling convention match, symbols are
// undecorated, so a plain extern "C" declaration links directly.
//
// kfilter_lib never allocates/frees memory or takes a lock -- Driver.cpp
// owns all of that (see KFILTER_GENERATION) -- and owns the IOCTL blob's
// wire format entirely; the driver never parses it.
#pragma once

// Closed vocabulary shared with kfilter_compiler::Op/Field -- discriminants
// must match exactly (this is the FFI ABI).
typedef enum _KFILTER_OP {
    KFilterOpFileCreate = 0,
    KFilterOpProcessCreate = 1,
    KFilterOpProcessOpen = 2,
    KFilterOpRegistrySet = 3,
} KFILTER_OP;

typedef enum _KFILTER_FIELD {
    KFilterFieldImagePath = 0,
    KFilterFieldKeyPath = 1,
    KFilterFieldValueName = 2,
    KFilterFieldValueData = 3,
} KFILTER_FIELD;

#ifdef __cplusplus
extern "C" {
#endif

// Bytes the driver must allocate for filterData -- opaque, a fixed
// constant of the closed op/field vocabulary, not derived from a blob.
SIZE_T kfilter_data_size(void);

// Parses `blob` into `filterData` (kfilter_data_size() bytes, already
// allocated by the driver). `blob` must outlive `filterData`. 0 = success,
// -1 = invalid/truncated blob or an op/field pair outside the closed
// vocabulary -- on failure the driver must discard `filterData` rather
// than call kfilter_match against it (just free the pool block, nothing
// else to release).
LONG kfilter_install_from_blob(
    _In_reads_bytes_(blobLen) const UCHAR* blob,
    _In_ SIZE_T blobLen,
    _Inout_ PVOID filterData
);

// Matches data[0..dataLen) against the entry compiled for (op, field) --
// O(1), a direct index into `filterData`, not a scan. Pure computation --
// caller must already hold rundown protection (or equivalent) on
// `filterData` for the whole call. 1 = match (matchedState written with
// the DFA's raw ending state id, not a rule/line number), 0 = no match,
// -1 = error (null pointer).
LONG kfilter_match(
    _In_ const PVOID filterData,
    _In_ ULONG op,     // a KFILTER_OP value
    _In_ ULONG field,  // a KFILTER_FIELD value
    _In_reads_bytes_(dataLen) const UCHAR* data,
    _In_ SIZE_T dataLen,
    _Out_opt_ ULONG* matchedState
);

#ifdef __cplusplus
}
#endif

// Must match IOCTL_KFILTER_LOAD_RULES in kfilter-compiler/src/main.rs.
#define IOCTL_KFILTER_LOAD_RULES \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define KFILTER_DEVICE_NAME   L"\\Device\\KFilter"
#define KFILTER_SYMLINK_NAME  L"\\??\\KFilter"
