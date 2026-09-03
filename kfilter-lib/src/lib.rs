//! Field-predicate matcher core, shared between the kernel driver and
//! user-mode tools (kfilter-cli).
//!
//! Two build modes, picked by the `kernel` Cargo feature:
//!
//! - **Default** (no features): plain, `std`-linkable `rlib`. Used
//!   directly by `kfilter-cli` -- no allocator needed even here,
//!   [`Ruleset`] never allocates, it only borrows a DFA byte slice
//!   the caller already owns.
//! - **`kernel`**: `#![no_std]`. Adds the [`kernel_ffi`] module: a
//!   pool-allocated (`ExAllocatePool2`/`ExFreePoolWithTag`),
//!   refcounted "active ruleset" slot holding **one [`Ruleset`] per
//!   (op, field) pair** plus the `extern "system"` exports
//!   (`kfilter_init`/`kfilter_load`/`kfilter_match`/`kfilter_unload`)
//!   the WDM driver links against. Build with:
//!   `cargo build --release --features kernel`.
//!
//! [`Ruleset::match_state`] deliberately does **not** enumerate which
//! rule(s) matched -- it drives the DFA by hand
//! (`start_state_forward` / `next_state` / `next_eoi_state`) and
//! returns the raw ending `StateID` as a plain `u32`, O(1) regardless
//! of how many patterns are "in" that state. Decoding a state id into
//! the set of `.rules` lines it represents is a **compile-time**
//! table built by `kfilter-compiler` (`build_state_line_map`, via
//! `match_len`/`match_pattern`) -- this crate never needs that table.
//!
//! One DFA per (op, field), not one shared flat DFA: measured on
//! `registry_mitre.rules` (250 patterns), splitting this way produced
//! smaller total DFA bytes (-58%), faster builds (-88%), and faster
//! per-call matches (-25%) than a single shared DFA (see
//! `kfilter-compiler`'s crate docs) -- this isn't just an
//! optimization, it's also strictly simpler here: `kfilter_match`
//! only ever searches the one DFA that was compiled for the exact
//! (op, field) the caller names, so there's no possibility of a value
//! from one field spuriously matching a pattern meant for another.

#![cfg_attr(feature = "kernel", no_std)]

use regex_automata::dfa::{sparse::DFA, Automaton};
use regex_automata::Input;

/// A loaded, ready-to-match ruleset: a thin wrapper over a
/// deserialized sparse DFA. Borrows the byte slice it was built from
/// -- callers own that buffer's lifetime (a `&'static [u8]` pool
/// allocation in the kernel, or just a local `Vec<u8>` read from disk
/// in user mode).
pub struct Ruleset<'a> {
    dfa: DFA<&'a [u8]>,
}

/// The byte slice wasn't a valid serialized sparse DFA (wrong format,
/// wrong version, or truncated/corrupted).
#[derive(Debug)]
pub struct LoadError;

impl core::fmt::Display for LoadError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str("invalid or corrupt ruleset DFA blob")
    }
}

impl<'a> Ruleset<'a> {
    /// Validates and deserializes `bytes` as a sparse DFA built by
    /// `kfilter-compiler` (checked: this is the crossing point for
    /// data from an untrusted source -- IOCTL from user mode in the
    /// kernel build, or an arbitrary file in the CLI).
    pub fn from_bytes(bytes: &'a [u8]) -> Result<Self, LoadError> {
        let (dfa, _) = DFA::from_bytes(bytes).map_err(|_| LoadError)?;
        Ok(Ruleset { dfa })
    }

    /// Runs `haystack` through the DFA and returns the raw ending
    /// state id if it lands on an accepting (match) state, `None`
    /// otherwise. See the module docs for why this returns a bare
    /// state id rather than an enumerated pattern/line list.
    pub fn match_state(&self, haystack: &[u8]) -> Option<u32> {
        let input = Input::new(haystack);
        let mut state = self.dfa.start_state_forward(&input).ok()?;
        for &byte in haystack {
            state = self.dfa.next_state(state, byte);
            if self.dfa.is_dead_state(state) {
                break;
            }
        }
        state = self.dfa.next_eoi_state(state);
        if self.dfa.is_match_state(state) {
            Some(state.as_u32())
        } else {
            None
        }
    }
}

#[cfg(feature = "kernel")]
pub use kernel_ffi::*;

#[cfg(feature = "kernel")]
mod kernel_ffi {
    use super::Ruleset;
    use core::sync::atomic::{AtomicBool, AtomicPtr, AtomicU32, Ordering};

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
    const TAG_RAW: u32 = u32::from_le_bytes(*b"aRfK"); // "KfRa" little-endian in poolmon
    const TAG_ENTRIES: u32 = u32::from_le_bytes(*b"tEfK"); // "KfEt" little-endian in poolmon

    // Must match kfilter-compiler's WIRE_MAGIC/WIRE_VERSION and
    // serialize_entries layout exactly.
    const WIRE_MAGIC: u32 = 0x4B46_524D; // "KFRM"
    const WIRE_VERSION: u32 = 1;
    const WIRE_HEADER_LEN: usize = 12; // magic(4) + version(4) + entry_count(4)

    extern "system" {
        fn ExAllocatePool2(flags: PoolFlags, number_of_bytes: usize, tag: u32) -> *mut u8;
        fn ExFreePoolWithTag(p: *mut u8, tag: u32);
        fn KeInitializeSpinLock(spin_lock: *mut KSpinLock);
        fn KeAcquireSpinLockRaiseToDpc(spin_lock: *mut KSpinLock) -> KIrql;
        fn KeReleaseSpinLock(spin_lock: *mut KSpinLock, new_irql: KIrql);
    }

    // ---- ruleset slot + refcounted swap ----------------------------------

    /// One (op, field) DFA, ready to match. `op`/`field` and the bytes
    /// `ruleset` borrows all point into the slot's `raw` allocation.
    struct Entry {
        op: &'static [u8],
        field: &'static [u8],
        ruleset: Ruleset<'static>,
    }

    struct DfaSlot {
        /// Number of holders: 1 for the ACTIVE pointer itself (while
        /// installed) + 1 per in-flight kfilter_match currently using it.
        refcount: AtomicU32,
        /// Set once this slot has been superseded by a newer kfilter_load.
        /// The last holder to drop the refcount to 0 after this is set
        /// frees the slot.
        retired: AtomicBool,
        /// Backing allocation for the raw wire blob (op/field names +
        /// every entry's DFA bytes all live inside this one buffer).
        raw: &'static [u8],
        /// Backing allocation for the `Entry` array itself (a separate
        /// pool block, since `Entry` isn't POD-copyable straight out
        /// of the wire bytes -- each one is constructed via
        /// `Ruleset::from_bytes`).
        entries: &'static [Entry],
    }

    static ACTIVE: AtomicPtr<DfaSlot> = AtomicPtr::new(core::ptr::null_mut());
    static mut ACTIVE_LOCK: KSpinLock = 0;

    #[panic_handler]
    fn panic(_info: &core::panic::PanicInfo) -> ! {
        loop {}
    }

    // MSVC's linker requires this symbol to exist whenever any code
    // touches XMM/SSE registers (which LLVM can do even for plain
    // integer/memory codegen, not just real floating point math) --
    // it's a marker the CRT startup would normally check, not a
    // function that gets called, so a dummy value is the standard,
    // safe fix (same pattern used throughout embedded/kernel Rust).
    // Only needed even after rebuilding core/compiler_builtins with
    // -Z build-std (see BUILD.md); build-std alone resolves the
    // riskier __CxxFrameHandler3 (unwind personality) requirement.
    #[no_mangle]
    pub static _fltused: i32 = 0;

    unsafe fn alloc_pool(len: usize, tag: u32) -> *mut u8 {
        if len == 0 {
            return core::ptr::null_mut();
        }
        ExAllocatePool2(POOL_FLAG_NON_PAGED, len, tag)
    }

    fn read_u32(buf: &[u8], offset: usize) -> Option<u32> {
        let b = buf.get(offset..offset + 4)?;
        Some(u32::from_le_bytes([b[0], b[1], b[2], b[3]]))
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
        let raw_ptr = slot_ref.raw.as_ptr().cast_mut();
        let entries_ptr = slot_ref.entries.as_ptr().cast_mut();
        core::ptr::drop_in_place(slot);
        if !raw_ptr.is_null() {
            ExFreePoolWithTag(raw_ptr, TAG_RAW);
        }
        if !entries_ptr.is_null() {
            ExFreePoolWithTag(entries_ptr.cast::<u8>(), TAG_ENTRIES);
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

    /// Validates `data[..len]` as a packed multi-DFA blob (see the
    /// wire format in `kfilter-compiler`'s `serialize_entries`):
    /// `[magic][version][entry_count]` then, per entry,
    /// `[op_len][op][field_len][field][dfa_len][dfa bytes]`. Copies it
    /// into driver-owned pool memory and installs it as the active
    /// ruleset; the previous ruleset (if any) is freed once no
    /// in-flight kfilter_match call still references it.
    ///
    /// Returns 0 on success, -1 on error (null/too-short input, a bad
    /// magic/version, truncated entry, allocation failure, or a
    /// corrupt/incompatible DFA blob within some entry -- the caller
    /// should treat -1 as "ruleset rejected, previous ruleset (if
    /// any) still active").
    ///
    /// # Safety
    /// `data` must be valid for reads of `len` bytes.
    #[no_mangle]
    pub unsafe extern "system" fn kfilter_load(data: *const u8, len: usize) -> i32 {
        if data.is_null() || len < WIRE_HEADER_LEN {
            return -1;
        }

        // Copy the whole blob into pool memory first; every subsequent
        // parse step reads from this trusted copy, not the live IOCTL
        // buffer.
        let raw_buf = alloc_pool(len, TAG_RAW);
        if raw_buf.is_null() {
            return -1;
        }
        core::ptr::copy_nonoverlapping(data, raw_buf, len);
        let raw: &'static [u8] = core::slice::from_raw_parts(raw_buf, len);

        let Some(magic) = read_u32(raw, 0) else {
            ExFreePoolWithTag(raw_buf, TAG_RAW);
            return -1;
        };
        let Some(version) = read_u32(raw, 4) else {
            ExFreePoolWithTag(raw_buf, TAG_RAW);
            return -1;
        };
        let Some(entry_count) = read_u32(raw, 8) else {
            ExFreePoolWithTag(raw_buf, TAG_RAW);
            return -1;
        };
        let entry_count = entry_count as usize;
        if magic != WIRE_MAGIC || version != WIRE_VERSION {
            ExFreePoolWithTag(raw_buf, TAG_RAW);
            return -1;
        }

        let entries_buf = if entry_count == 0 {
            core::ptr::NonNull::<Entry>::dangling().as_ptr()
        } else {
            let Some(entries_size) = entry_count.checked_mul(core::mem::size_of::<Entry>()) else {
                ExFreePoolWithTag(raw_buf, TAG_RAW);
                return -1;
            };
            let p = alloc_pool(entries_size, TAG_ENTRIES) as *mut Entry;
            if p.is_null() {
                ExFreePoolWithTag(raw_buf, TAG_RAW);
                return -1;
            }
            p
        };

        let mut offset = WIRE_HEADER_LEN;
        let mut filled = 0usize;
        let mut ok = true;

        for i in 0..entry_count {
            let Some(op_len) = read_u32(raw, offset) else { ok = false; break };
            offset += 4;
            let Some(op) = raw.get(offset..offset + op_len as usize) else { ok = false; break };
            offset += op_len as usize;

            let Some(field_len) = read_u32(raw, offset) else { ok = false; break };
            offset += 4;
            let Some(field) = raw.get(offset..offset + field_len as usize) else { ok = false; break };
            offset += field_len as usize;

            let Some(dfa_len) = read_u32(raw, offset) else { ok = false; break };
            offset += 4;
            let Some(dfa_bytes) = raw.get(offset..offset + dfa_len as usize) else { ok = false; break };
            offset += dfa_len as usize;

            let ruleset = match Ruleset::from_bytes(dfa_bytes) {
                Ok(r) => r,
                Err(_) => {
                    ok = false;
                    break;
                }
            };

            core::ptr::write(entries_buf.add(i), Entry { op, field, ruleset });
            filled = i + 1;
        }

        if !ok {
            for i in 0..filled {
                core::ptr::drop_in_place(entries_buf.add(i));
            }
            if entry_count != 0 {
                ExFreePoolWithTag(entries_buf.cast::<u8>(), TAG_ENTRIES);
            }
            ExFreePoolWithTag(raw_buf, TAG_RAW);
            return -1;
        }

        let entries: &'static [Entry] = core::slice::from_raw_parts(entries_buf, entry_count);

        let slot_ptr = alloc_pool(core::mem::size_of::<DfaSlot>(), TAG_SLOT) as *mut DfaSlot;
        if slot_ptr.is_null() {
            for i in 0..filled {
                core::ptr::drop_in_place(entries_buf.add(i));
            }
            if entry_count != 0 {
                ExFreePoolWithTag(entries_buf.cast::<u8>(), TAG_ENTRIES);
            }
            ExFreePoolWithTag(raw_buf, TAG_RAW);
            return -1;
        }
        core::ptr::write(
            slot_ptr,
            DfaSlot {
                refcount: AtomicU32::new(1), // the ACTIVE pointer's own reference
                retired: AtomicBool::new(false),
                raw,
                entries,
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

    /// Matches `data[..data_len]` against the DFA compiled for
    /// `(op, field)` in the currently active ruleset, returning the
    /// **raw ending state id** via `matched_state` -- see the
    /// crate-level docs for why. `op`/`field` are compared as raw
    /// bytes against what `kfilter-compiler` labeled each DFA with
    /// (e.g. `b"registry_set"`, `b"key_path"`).
    ///
    /// Returns 1 and writes the state id when the match lands on an
    /// accepting state, 0 when it doesn't (including "no ruleset
    /// loaded yet" or "no DFA compiled for this (op, field)" -- the
    /// caller can't distinguish those without also consulting
    /// `kfilter_state_map.json`, which isn't available in the
    /// kernel), -1 on error (null `data`/`op`/`field`).
    ///
    /// # Safety
    /// `data` must be valid for reads of `data_len` bytes; `op` for
    /// `op_len` bytes; `field` for `field_len` bytes.
    #[no_mangle]
    pub unsafe extern "system" fn kfilter_match(
        op: *const u8,
        op_len: usize,
        field: *const u8,
        field_len: usize,
        data: *const u8,
        data_len: usize,
        matched_state: *mut u32,
    ) -> i32 {
        if data.is_null() || op.is_null() || field.is_null() {
            return -1;
        }

        let slot = acquire_active();
        let Some(slot_ref) = slot.as_ref() else {
            return 0;
        };

        let op_bytes = core::slice::from_raw_parts(op, op_len);
        let field_bytes = core::slice::from_raw_parts(field, field_len);
        let haystack = core::slice::from_raw_parts(data, data_len);

        let entry = slot_ref
            .entries
            .iter()
            .find(|e| e.op == op_bytes && e.field == field_bytes);

        let result = match entry {
            Some(e) => match e.ruleset.match_state(haystack) {
                Some(state) => {
                    if !matched_state.is_null() {
                        *matched_state = state;
                    }
                    1
                }
                None => 0,
            },
            None => 0,
        };

        release_slot(slot);
        result
    }
}
