// Declarations for the kfilter_core Rust staticlib (kfilter-runtime/src/lib.rs).
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

// Validates data[0..len) as a serialized ruleset blob (wire format:
// magic/version/pattern_count header + line-number table + sparse DFA
// bytes -- see kfilter-runtime/src/lib.rs and kfilter-compiler's
// WIRE_MAGIC/WIRE_VERSION), copies it into driver-owned pool memory,
// and installs it as the active ruleset.
// 0 = ok (installed), -1 = rejected (previous ruleset, if any, still active).
LONG kfilter_load(
    _In_reads_bytes_(len) const UCHAR* data,
    _In_ SIZE_T len
);

// Match data[0..len) against the currently active ruleset.
// 1 = match (matchedLine written if non-null with the source .rules
// line number the matching condition came from), 0 = no match
// (including "no ruleset loaded yet"), -1 = error.
LONG kfilter_match(
    _In_reads_bytes_(len) const UCHAR* data,
    _In_ SIZE_T len,
    _Out_opt_ ULONG* matchedLine
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
