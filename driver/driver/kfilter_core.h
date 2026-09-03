// Declarations for the kfilter_lib Rust staticlib (kfilter-lib/src/lib.rs,
// built with `cargo build --release --features kernel`).
// x64 only: Rust's `extern "system"` and C's default calling convention are
// the same Microsoft x64 convention, and symbol names are undecorated on
// x64, so a plain extern "C" declaration links directly against the .lib.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Initializes the internal spinlock. Call once from DriverEntry before
// registering the device object (before any IOCTL can arrive).
VOID kfilter_init(void);

// Releases the active ruleset, if any. Call from DriverUnload.
VOID kfilter_unload(void);

// Validates data[0..len) as a packed multi-DFA blob -- one DFA per (op,
// field), see kfilter-compiler's serialize_entries / this crate's
// kfilter_load doc comment for the exact layout -- copies it into
// driver-owned pool memory, and installs it as the active ruleset.
// 0 = ok (installed), -1 = rejected (previous ruleset, if any, still active).
LONG kfilter_load(
    _In_reads_bytes_(len) const UCHAR* data,
    _In_ SIZE_T len
);

// Match data[0..dataLen) against the DFA compiled for (op, field) in the
// currently active ruleset. op/field are compared as raw bytes against
// what kfilter-compiler labeled each DFA with (e.g. "registry_set",
// "key_path") -- pass string literals matching your .rules file's `op=`
// and field names.
// 1 = match (matchedState written if non-null with the DFA's raw ending
// state id -- NOT a rule/line number), 0 = no match (including "no ruleset
// loaded yet" or "no DFA compiled for this (op, field)"), -1 = error.
//
// matchedState is only meaningful together with kfilter_state_map.json
// (written by kfilter-compiler next to kfilter_rules.dfa): that file maps
// each (op, field, state id) to the source .rules line(s) it represents.
// This lookup is intentionally done in user mode, not here -- see
// kfilter-lib's module doc comment for why.
LONG kfilter_match(
    _In_reads_bytes_(opLen) const UCHAR* op,
    _In_ SIZE_T opLen,
    _In_reads_bytes_(fieldLen) const UCHAR* field,
    _In_ SIZE_T fieldLen,
    _In_reads_bytes_(dataLen) const UCHAR* data,
    _In_ SIZE_T dataLen,
    _Out_opt_ ULONG* matchedState
);

#ifdef __cplusplus
}
#endif

// IOCTL used by kfilter-compiler to push a newly-built ruleset.
// Must match IOCTL_KFILTER_LOAD_RULES in kfilter-compiler/src/main.rs.
// CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KFILTER_LOAD_RULES \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define KFILTER_DEVICE_NAME   L"\\Device\\KFilter"
#define KFILTER_SYMLINK_NAME  L"\\??\\KFilter"
