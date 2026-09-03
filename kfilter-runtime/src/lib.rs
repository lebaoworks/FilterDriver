//! Kernel-mode field-predicate matcher for the rules filter driver.
//!
//! `#![no_std]`, no Rust `alloc`: rule data is no longer embedded at
//! compile time. It arrives at runtime from a separate user-mode
//! process (kfilter-compiler) that sends a compiled ruleset blob to
//! the driver over an IOCTL; the driver hands the raw bytes to
//! `kfilter_load`, which validates them once, copies them into
//! kernel pool memory it owns, and installs them as the active
//! ruleset. `kfilter_match` then runs against whatever is currently
//! installed.
//!
//! Wire format (must match kfilter-compiler's WIRE_MAGIC/WIRE_VERSION):
//!   [magic: u32 LE][version: u32 LE][pattern_count: u32 LE]
//!   [line_numbers: pattern_count * u32 LE]   -- index = DFA pattern id
//!   [sparse DFA bytes: rest]
//! `line_numbers[pattern_id]` is the source `.rules` line the compiled
//! pattern came from, so `kfilter_match` can hand back a line number
//! directly instead of an opaque DFA-internal id.
//!
//! Pool memory is managed by hand via `ExAllocatePool2`/
//! `ExFreePoolWithTag` (not Rust's `alloc` crate -- no global
//! allocator needed). A small refcounted-slot scheme lets
//! `kfilter_match` run without holding any lock for the duration of
//! the search, while `kfilter_load` can swap in a new ruleset and
//! free the old one's memory only once no in-flight match still
//! references it.

#![no_std]

use core::sync::atomic::{AtomicBool, AtomicPtr, AtomicU32, Ordering};

use regex_automata::dfa::{sparse::DFA, Automaton};
use regex_automata::Input;

// ---- kernel FFI -----------------------------------------------------
//
// Verified against ntoskrnl.lib's actual x64 export table (dumpbin
// /linkermember), not just header text -- KeAcquireSpinLock is a
// header macro that expands differently per architecture: on x86 it
// calls KfAcquireSpinLock (FASTCALL), but that symbol isn't exported
// for x64 at all. The real x64 export is KeAcquireSpinLockRaiseToDpc
// (matches wdm.h's own x64 macro body: `KeAcquireSpinLock(l, o) =>
// *(o) = KeAcquireSpinLockRaiseToDpc(l)`). KeReleaseSpinLock, unlike
// acquire, is exported directly under its own name on x64.
//   - ExAllocatePool2 / ExFreePoolWithTag: real exported NTAPI symbols.
//   - POOL_FLAGS is ULONG64; POOL_FLAG_NON_PAGED == 0x40.
//   - KSPIN_LOCK is just `ULONG_PTR` (usize); KeInitializeSpinLock
//     zero-initializes it.
// On x64 there is one calling convention, so `extern "system"` links
// correctly against all of these regardless of the NTAPI/FASTCALL
// keyword the C header uses.

type PoolFlags = u64;
const POOL_FLAG_NON_PAGED: PoolFlags = 0x40;

type KIrql = u8;
type KSpinLock = usize;

const TAG_SLOT: u32 = u32::from_le_bytes(*b"SlfK"); // "KfSl" little-endian in poolmon
const TAG_DFA: u32 = u32::from_le_bytes(*b"aDfK"); // "KfDa" little-endian in poolmon
const TAG_LINE: u32 = u32::from_le_bytes(*b"nLfK"); // "KfLn" little-endian in poolmon

// Must match WIRE_MAGIC/WIRE_VERSION in kfilter-compiler/src/main.rs.
const WIRE_MAGIC: u32 = 0x4B46524C; // "KFRL"
const WIRE_VERSION: u32 = 1;
const WIRE_HEADER_LEN: usize = 12; // magic(4) + version(4) + pattern_count(4)

extern "system" {
    fn ExAllocatePool2(flags: PoolFlags, number_of_bytes: usize, tag: u32) -> *mut u8;
    fn ExFreePoolWithTag(p: *mut u8, tag: u32);
    fn KeInitializeSpinLock(spin_lock: *mut KSpinLock);
    fn KeAcquireSpinLockRaiseToDpc(spin_lock: *mut KSpinLock) -> KIrql;
    fn KeReleaseSpinLock(spin_lock: *mut KSpinLock, new_irql: KIrql);
}

// ---- ruleset slot + refcounted swap ----------------------------------

struct DfaSlot {
    /// Number of holders: 1 for the ACTIVE pointer itself (while
    /// installed) + 1 per in-flight kfilter_match currently using it.
    refcount: AtomicU32,
    /// Set once this slot has been superseded by a newer kfilter_load.
    /// The last holder to drop the refcount to 0 after this is set
    /// frees the slot.
    retired: AtomicBool,
    /// Backing allocation for the DFA portion of the wire blob. Freed
    /// once refcount hits 0 after retirement.
    dfa_bytes: &'static [u8],
    /// Backing allocation for the decoded line-number table, index =
    /// DFA pattern id. Freed alongside `dfa_bytes`.
    line_numbers: &'static [u32],
    /// Parsed once in kfilter_load; borrows `dfa_bytes`. No re-parsing
    /// cost per kfilter_match call.
    dfa: DFA<&'static [u8]>,
}

static ACTIVE: AtomicPtr<DfaSlot> = AtomicPtr::new(core::ptr::null_mut());
static mut ACTIVE_LOCK: KSpinLock = 0;

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

unsafe fn alloc_pool(len: usize, tag: u32) -> *mut u8 {
    if len == 0 {
        return core::ptr::null_mut();
    }
    ExAllocatePool2(POOL_FLAG_NON_PAGED, len, tag)
}

/// Atomically reads the active slot and takes out a reference on it
/// (increments refcount) under the spinlock, so a concurrent
/// kfilter_load can never free a slot a matcher just started using.
unsafe fn acquire_active() -> *mut DfaSlot {
    let lock_ptr = core::ptr::addr_of_mut!(ACTIVE_LOCK);
    let irql = KeAcquireSpinLockRaiseToDpc(lock_ptr);
    let p = ACTIVE.load(Ordering::Relaxed);
    if let Some(slot) = p.as_ref() {
        slot.refcount.fetch_add(1, Ordering::AcqRel);
    }
    KeReleaseSpinLock(lock_ptr, irql);
    p
}

/// Drops a reference taken by `acquire_active` (or the ACTIVE
/// pointer's own reference, when retiring a slot). Frees the slot's
/// memory once the count reaches 0 *and* it has been retired.
unsafe fn release_slot(slot: *mut DfaSlot) {
    let Some(slot_ref) = slot.as_ref() else {
        return;
    };
    let prev = slot_ref.refcount.fetch_sub(1, Ordering::AcqRel);
    if prev == 1 && slot_ref.retired.load(Ordering::Acquire) {
        free_slot(slot);
    }
}

unsafe fn free_slot(slot: *mut DfaSlot) {
    let slot_ref = &*slot;
    let dfa_ptr = slot_ref.dfa_bytes.as_ptr().cast_mut();
    // `line_numbers` uses a dangling (non-null, non-allocated) sentinel
    // pointer when empty -- see kfilter_load -- so gate freeing it on
    // length, not nullness.
    let has_lines = !slot_ref.line_numbers.is_empty();
    let line_ptr = slot_ref.line_numbers.as_ptr().cast_mut();
    core::ptr::drop_in_place(slot);
    if !dfa_ptr.is_null() {
        ExFreePoolWithTag(dfa_ptr, TAG_DFA);
    }
    if has_lines {
        ExFreePoolWithTag(line_ptr.cast::<u8>(), TAG_LINE);
    }
    ExFreePoolWithTag(slot.cast::<u8>(), TAG_SLOT);
}

/// Initializes the spinlock. Call once from DriverEntry before
/// registering the device object (and therefore before any IOCTL
/// could reach kfilter_load/kfilter_match).
#[no_mangle]
pub extern "system" fn kfilter_init() {
    unsafe { KeInitializeSpinLock(core::ptr::addr_of_mut!(ACTIVE_LOCK)) };
}

/// Releases the active ruleset, if any. Call from DriverUnload.
#[no_mangle]
pub extern "system" fn kfilter_unload() {
    unsafe {
        let lock_ptr = core::ptr::addr_of_mut!(ACTIVE_LOCK);
        let irql = KeAcquireSpinLockRaiseToDpc(lock_ptr);
        let old = ACTIVE.swap(core::ptr::null_mut(), Ordering::AcqRel);
        KeReleaseSpinLock(lock_ptr, irql);

        if let Some(old_ref) = old.as_ref() {
            old_ref.retired.store(true, Ordering::Release);
            release_slot(old);
        }
    }
}

/// Validates `data[..len]` as a serialized ruleset blob (see the wire
/// format above), copies its pieces into driver-owned pool memory,
/// and installs it as the active ruleset. The previous ruleset (if
/// any) is freed once no in-flight kfilter_match call still
/// references it.
///
/// Returns 0 on success, -1 on error (null/too-short input, a bad
/// magic/version, allocation failure, or a corrupt/incompatible DFA
/// blob -- the caller should treat -1 as "ruleset rejected, previous
/// ruleset (if any) still active").
///
/// # Safety
/// `data` must be valid for reads of `len` bytes.
#[no_mangle]
pub unsafe extern "system" fn kfilter_load(data: *const u8, len: usize) -> i32 {
    if data.is_null() || len < WIRE_HEADER_LEN {
        return -1;
    }

    let header = core::slice::from_raw_parts(data, WIRE_HEADER_LEN);
    let magic = u32::from_le_bytes([header[0], header[1], header[2], header[3]]);
    let version = u32::from_le_bytes([header[4], header[5], header[6], header[7]]);
    let pattern_count =
        u32::from_le_bytes([header[8], header[9], header[10], header[11]]) as usize;
    if magic != WIRE_MAGIC || version != WIRE_VERSION {
        return -1;
    }

    let Some(line_bytes_len) = pattern_count.checked_mul(4) else {
        return -1;
    };
    let Some(dfa_offset) = WIRE_HEADER_LEN.checked_add(line_bytes_len) else {
        return -1;
    };
    if len < dfa_offset {
        return -1;
    }
    let dfa_len = len - dfa_offset;
    if dfa_len == 0 {
        return -1;
    }

    // Fresh allocation for the line-number table: ExAllocatePool2's
    // result is always well aligned for u32 (MEMORY_ALLOCATION_ALIGNMENT
    // is 16 on x64), regardless of the source buffer's alignment.
    // `slice::from_raw_parts` requires a non-null, aligned pointer even
    // for a zero-length slice, so fall back to a dangling-but-aligned
    // pointer when there are no patterns instead of passing through a
    // null allocation result.
    let line_buf = if line_bytes_len == 0 {
        core::ptr::NonNull::<u32>::dangling().as_ptr()
    } else {
        let p = alloc_pool(line_bytes_len, TAG_LINE) as *mut u32;
        if p.is_null() {
            return -1;
        }
        core::ptr::copy_nonoverlapping(data.add(WIRE_HEADER_LEN), p.cast::<u8>(), line_bytes_len);
        p
    };
    let line_numbers: &'static [u32] = core::slice::from_raw_parts(line_buf, pattern_count);

    let dfa_buf = alloc_pool(dfa_len, TAG_DFA);
    if dfa_buf.is_null() {
        if pattern_count != 0 {
            ExFreePoolWithTag(line_buf.cast::<u8>(), TAG_LINE);
        }
        return -1;
    }
    core::ptr::copy_nonoverlapping(data.add(dfa_offset), dfa_buf, dfa_len);
    let dfa_bytes: &'static [u8] = core::slice::from_raw_parts(dfa_buf, dfa_len);

    // Checked deserialization: `data` crossed a trust boundary (IOCTL
    // from user mode), so validate the copy fully, once, here -- not
    // on the kfilter_match hot path.
    let dfa = match DFA::from_bytes(dfa_bytes) {
        Ok((dfa, _)) => dfa,
        Err(_) => {
            ExFreePoolWithTag(dfa_buf, TAG_DFA);
            if pattern_count != 0 {
                ExFreePoolWithTag(line_buf.cast::<u8>(), TAG_LINE);
            }
            return -1;
        }
    };

    let slot_ptr = alloc_pool(core::mem::size_of::<DfaSlot>(), TAG_SLOT) as *mut DfaSlot;
    if slot_ptr.is_null() {
        ExFreePoolWithTag(dfa_buf, TAG_DFA);
        if pattern_count != 0 {
            ExFreePoolWithTag(line_buf.cast::<u8>(), TAG_LINE);
        }
        return -1;
    }
    core::ptr::write(
        slot_ptr,
        DfaSlot {
            refcount: AtomicU32::new(1), // the ACTIVE pointer's own reference
            retired: AtomicBool::new(false),
            dfa_bytes,
            line_numbers,
            dfa,
        },
    );

    let lock_ptr = core::ptr::addr_of_mut!(ACTIVE_LOCK);
    let irql = KeAcquireSpinLockRaiseToDpc(lock_ptr);
    let old = ACTIVE.swap(slot_ptr, Ordering::AcqRel);
    KeReleaseSpinLock(lock_ptr, irql);

    if let Some(old_ref) = old.as_ref() {
        old_ref.retired.store(true, Ordering::Release);
        release_slot(old);
    }

    0
}

/// Matches `data[..len]` against the currently active ruleset.
///
/// Returns 1 and writes the matching condition's **source `.rules`
/// line number** to `matched_line` (if non-null) on match, 0 on no
/// match (including "no ruleset loaded yet"), -1 on error (null data,
/// or an internal pattern id somehow outside the loaded line-number
/// table -- should not happen for a blob that passed kfilter_load).
///
/// # Safety
/// `data` must be valid for reads of `len` bytes.
#[no_mangle]
pub unsafe extern "system" fn kfilter_match(
    data: *const u8,
    len: usize,
    matched_line: *mut u32,
) -> i32 {
    if data.is_null() {
        return -1;
    }

    let slot = acquire_active();
    let Some(slot_ref) = slot.as_ref() else {
        return 0;
    };

    let haystack = core::slice::from_raw_parts(data, len);
    let input = Input::new(haystack);

    let result = match slot_ref.dfa.try_search_fwd(&input) {
        Ok(Some(hm)) => {
            let idx = hm.pattern().as_usize();
            match slot_ref.line_numbers.get(idx) {
                Some(&line) => {
                    if !matched_line.is_null() {
                        *matched_line = line;
                    }
                    1
                }
                None => -1,
            }
        }
        Ok(None) => 0,
        Err(_) => -1,
    };

    release_slot(slot);
    result
}
