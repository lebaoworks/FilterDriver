//! Field-predicate matcher core, shared between the kernel driver and
//! user-mode tools (kfilter-cli).
//!
//! Two build modes via the `kernel` Cargo feature: default is a plain
//! `std`-linkable rlib (used by `kfilter-cli`); `kernel` adds
//! `#![no_std]` and the [`kernel_ffi`] exports the WDM driver links
//! against.
//!
//! This crate never allocates, frees, or locks -- all resource
//! management is the driver's job. It also owns the IOCTL blob's wire
//! format and the entry table's layout entirely; the driver only ever
//! holds opaque pointers and sizes it got from this crate.

#![cfg_attr(feature = "kernel", no_std)]

use regex_automata::dfa::{sparse::DFA, Automaton};
use regex_automata::Input;

/// A loaded, ready-to-match ruleset: a thin wrapper over a
/// deserialized sparse DFA, borrowing the byte slice it was built
/// from.
pub struct Ruleset<'a> {
    dfa: DFA<&'a [u8]>,
}

/// The byte slice wasn't a valid serialized sparse DFA.
#[derive(Debug)]
pub struct LoadError;

impl core::fmt::Display for LoadError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        f.write_str("invalid or corrupt ruleset DFA blob")
    }
}

impl<'a> Ruleset<'a> {
    pub fn from_bytes(bytes: &'a [u8]) -> Result<Self, LoadError> {
        let (dfa, _) = DFA::from_bytes(bytes).map_err(|_| LoadError)?;
        Ok(Ruleset { dfa })
    }

    /// Returns the DFA's raw ending state id if `haystack` lands on a
    /// match state. A bare state id keeps this O(1) memory regardless
    /// of ruleset size; decoding it into `.rules` lines is a
    /// compile-time-only table built by `kfilter-compiler`.
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

    #[panic_handler]
    fn panic(_info: &core::panic::PanicInfo) -> ! {
        loop {}
    }

    // Required by the MSVC linker whenever code touches XMM/SSE
    // registers -- a marker, never actually called.
    #[no_mangle]
    pub static _fltused: i32 = 0;

    /// One (op, field) slot -- `None` if no DFA was compiled for that
    /// pair. Owns no external memory; see the crate docs.
    type Entry = Option<Ruleset<'static>>;

    // op/field vocabulary, hand-duplicated from kfilter_compiler::{Op,
    // Field, Op::fields} (separate Cargo project, can't share code) --
    // kept in sync by that crate's op_field_table_matches_kfilter_lib
    // test.
    const OP_COUNT: u32 = 4;
    const MAX_FIELDS_PER_OP: u32 = 3; // max(1, 1, 1, 3) below

    /// `field`'s position within `op`'s own field list, or `None` if
    /// not valid for `op`. The entry table is indexed
    /// `op * MAX_FIELDS_PER_OP + op_field_local_index(op, field)` --
    /// sized by the busiest op's field count rather than the total
    /// distinct fields across every op, so it stays small even when
    /// ops mostly use disjoint fields.
    const fn op_field_local_index(op: u32, field: u32) -> Option<u32> {
        match op {
            0 | 1 | 2 => match field {
                0 => Some(0), // ImagePath
                _ => None,
            },
            3 => match field {
                1 => Some(0), // KeyPath
                2 => Some(1), // ValueName
                3 => Some(2), // ValueData
                _ => None,
            },
            _ => None,
        }
    }

    fn entry_table_len() -> usize {
        (OP_COUNT * MAX_FIELDS_PER_OP) as usize
    }

    // Wire format: [magic: u32 LE][version: u32 LE][entry_count: u32
    // LE], then entry_count repetitions of
    // [op: u32 LE][field: u32 LE][dfa_len: u32 LE][dfa bytes]. Must
    // match kfilter_compiler::{WIRE_MAGIC, WIRE_VERSION,
    // serialize_entries} exactly.
    const WIRE_MAGIC: u32 = 0x4B46_524D; // "KFRM"
    const WIRE_VERSION: u32 = 1;
    const WIRE_HEADER_LEN: usize = 12;

    fn read_u32_le(bytes: &[u8], offset: usize) -> Option<u32> {
        let end = offset.checked_add(4)?;
        let slice = bytes.get(offset..end)?;
        Some(u32::from_le_bytes(slice.try_into().unwrap()))
    }

    fn parse_header(bytes: &[u8]) -> Option<u32> {
        if read_u32_le(bytes, 0)? != WIRE_MAGIC || read_u32_le(bytes, 4)? != WIRE_VERSION {
            return None;
        }
        read_u32_le(bytes, 8)
    }

    /// Bytes the driver must allocate for the entry table passed to
    /// [`kfilter_install_from_blob`]/[`kfilter_match`] -- opaque, a
    /// fixed constant of the closed op/field vocabulary.
    #[no_mangle]
    pub extern "system" fn kfilter_data_size() -> usize {
        entry_table_len() * core::mem::size_of::<Entry>()
    }

    unsafe fn install_all(bytes: &'static [u8], entries: *mut u8) -> Option<()> {
        let table_len = entry_table_len();
        for i in 0..table_len {
            core::ptr::write(entries.cast::<Entry>().add(i), None);
        }

        let entry_count = parse_header(bytes)?;
        let mut offset = WIRE_HEADER_LEN;
        for _ in 0..entry_count {
            let op = read_u32_le(bytes, offset)?;
            offset += 4;
            let field = read_u32_le(bytes, offset)?;
            offset += 4;
            let dfa_len = read_u32_le(bytes, offset)? as usize;
            offset += 4;
            let end = offset.checked_add(dfa_len)?;
            let dfa_bytes = bytes.get(offset..end)?;
            offset = end;

            let local = op_field_local_index(op, field)?;
            let index = (op * MAX_FIELDS_PER_OP + local) as usize; // always < table_len
            debug_assert!(index < table_len);

            let ruleset = Ruleset::from_bytes(dfa_bytes).ok()?;
            core::ptr::write(entries.cast::<Entry>().add(index), Some(ruleset));
        }
        Some(())
    }

    /// Parses `blob` and constructs every entry into `entries`
    /// ([`kfilter_data_size`] bytes, already allocated by the driver).
    /// `blob` must outlive `entries` -- each entry borrows its DFA
    /// bytes directly from it.
    ///
    /// Returns 0 on success, -1 on any failure (bad magic/version,
    /// truncated data, an op/field pair outside the closed vocabulary,
    /// or a corrupt DFA). On failure the driver must discard `entries`
    /// rather than call [`kfilter_match`] against it.
    ///
    /// # Safety
    /// `blob` must be valid for reads of `blob_len` bytes and must
    /// outlive `entries`. `entries` must point to [`kfilter_data_size`]
    /// writable, well-aligned bytes.
    #[no_mangle]
    pub unsafe extern "system" fn kfilter_install_from_blob(
        blob: *const u8,
        blob_len: usize,
        entries: *mut u8,
    ) -> i32 {
        if blob.is_null() || entries.is_null() {
            return -1;
        }
        let bytes: &'static [u8] = core::slice::from_raw_parts(blob, blob_len);
        match install_all(bytes, entries) {
            Some(()) => 0,
            None => -1,
        }
    }

    /// Matches `data[..data_len]` against the entry compiled for
    /// `(op, field)` -- O(1), a direct index into `entries`, not a
    /// scan. No allocation, no locking -- the caller must already hold
    /// rundown protection (or equivalent) on `entries` for the whole
    /// call.
    ///
    /// Returns 1 and writes the state id on a match, 0 on no match, -1
    /// on error (a null pointer).
    ///
    /// # Safety
    /// `entries` must point to [`kfilter_data_size`] bytes, fully
    /// constructed by [`kfilter_install_from_blob`]. `data` must be
    /// valid for reads of `data_len` bytes.
    #[no_mangle]
    pub unsafe extern "system" fn kfilter_match(
        entries: *const u8,
        op: u32,
        field: u32,
        data: *const u8,
        data_len: usize,
        matched_state: *mut u32,
    ) -> i32 {
        if entries.is_null() || data.is_null() {
            return -1;
        }
        let Some(local) = op_field_local_index(op, field) else {
            return 0;
        };
        let index = (op * MAX_FIELDS_PER_OP + local) as usize;

        let slot: &Entry = &*entries.cast::<Entry>().add(index);
        match slot {
            Some(ruleset) => {
                let haystack = core::slice::from_raw_parts(data, data_len);
                match ruleset.match_state(haystack) {
                    Some(state) => {
                        if !matched_state.is_null() {
                            *matched_state = state;
                        }
                        1
                    }
                    None => 0,
                }
            }
            None => 0,
        }
    }
}
