//! CLI: parses a `.rules` file (via the `kfilter_compiler` library),
//! compiles it into one DFA per (op, field), writes `kfilter_rules.dfa`
//! (the packed multi-entry blob) + `kfilter_state_map.json` locally,
//! and sends the packed blob to a running kfilter driver over
//! IOCTL_KFILTER_LOAD_RULES.

use kfilter_compiler::{compile_ruleset, parse_rules_file, serialize_entries, CompiledRuleset};
use std::env;
use std::ffi::c_void;
use std::fs;
use std::iter;
use std::os::windows::ffi::OsStrExt;
use std::ptr;

use windows_sys::Win32::Foundation::{CloseHandle, GetLastError, HANDLE, INVALID_HANDLE_VALUE};
use windows_sys::Win32::Storage::FileSystem::{
    CreateFileW, FILE_ATTRIBUTE_NORMAL, FILE_GENERIC_WRITE, FILE_SHARE_NONE, OPEN_EXISTING,
};
use windows_sys::Win32::System::IO::DeviceIoControl;

// Must match IOCTL_KFILTER_LOAD_RULES in driver/driver/kfilter_core.h.
// CTL_CODE(FILE_DEVICE_UNKNOWN=0x22, function=0x800, METHOD_BUFFERED=0, FILE_ANY_ACCESS=0)
const IOCTL_KFILTER_LOAD_RULES: u32 = 0x0022_2000;
const DEVICE_PATH: &str = r"\\.\KFilter";

fn to_wide(s: &str) -> Vec<u16> {
    std::ffi::OsStr::new(s)
        .encode_wide()
        .chain(iter::once(0))
        .collect()
}

fn send_to_driver(bytes: &[u8]) -> Result<(), String> {
    let path = to_wide(DEVICE_PATH);

    let handle: HANDLE = unsafe {
        CreateFileW(
            path.as_ptr(),
            FILE_GENERIC_WRITE,
            FILE_SHARE_NONE,
            ptr::null(),
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            ptr::null_mut(),
        )
    };
    if handle == INVALID_HANDLE_VALUE {
        let err = unsafe { GetLastError() };
        return Err(format!(
            "CreateFileW({DEVICE_PATH}) failed, GetLastError={err} -- is the driver loaded?"
        ));
    }

    let mut bytes_returned: u32 = 0;
    let ok = unsafe {
        DeviceIoControl(
            handle,
            IOCTL_KFILTER_LOAD_RULES,
            bytes.as_ptr() as *const c_void,
            bytes.len() as u32,
            ptr::null_mut(),
            0,
            &mut bytes_returned,
            ptr::null_mut(),
        )
    };
    let err = if ok == 0 { Some(unsafe { GetLastError() }) } else { None };
    unsafe { CloseHandle(handle) };

    match err {
        None => Ok(()),
        Some(code) => Err(format!("DeviceIoControl failed, GetLastError={code}")),
    }
}

fn write_state_map_json(rules_path: &str, compiled: &CompiledRuleset) {
    let mut out = String::new();
    out.push_str("{\n");
    out.push_str(&format!("  \"rules_file\": \"{}\",\n", rules_path.replace('\\', "\\\\")));
    out.push_str("  \"entries\": [\n");
    let n_entries = compiled.entries.len();
    for (ei, entry) in compiled.entries.iter().enumerate() {
        out.push_str(&format!(
            "    {{\"op\": \"{}\", \"field\": \"{}\", \"dfa_bytes\": {}, \"states\": {{\n",
            entry.op.as_str(),
            entry.field.as_str(),
            entry.dfa_bytes.len()
        ));
        let n_states = entry.state_map.len();
        for (i, (state_id, infos)) in entry.state_map.iter().enumerate() {
            out.push_str(&format!("      \"{state_id}\": ["));
            for (j, info) in infos.iter().enumerate() {
                out.push_str(&format!(
                    "{{\"line\": {}, \"group\": {}, \"pattern\": \"{}\", \"step\": \"{}\"}}",
                    info.line, info.group, info.pattern_name, info.step
                ));
                if j + 1 < infos.len() {
                    out.push_str(", ");
                }
            }
            out.push(']');
            if i + 1 < n_states {
                out.push(',');
            }
            out.push('\n');
        }
        out.push_str("    }}");
        if ei + 1 < n_entries {
            out.push(',');
        }
        out.push('\n');
    }
    out.push_str("  ]\n");
    out.push_str("}\n");

    fs::write("kfilter_state_map.json", &out).expect("failed to write kfilter_state_map.json");
    println!("wrote kfilter_state_map.json ({} bytes)", out.len());
}

fn main() {
    let path = env::args().nth(1).unwrap_or_else(|| {
        eprintln!("usage: kfilter-compiler <rules-file>");
        std::process::exit(1);
    });

    let steps = parse_rules_file(&path);
    let compilable: usize = steps.iter().flat_map(|s| &s.groups).map(Vec::len).sum();
    println!(
        "{path}: {} step(s) with compilable conditions -> {compilable} pattern(s)",
        steps.len()
    );

    let compiled = compile_ruleset(&steps).unwrap_or_else(|e| {
        eprintln!("{e}");
        std::process::exit(1);
    });
    println!("compiled into {} DFA(s), one per (op, field):", compiled.entries.len());
    for entry in &compiled.entries {
        println!(
            "  op={:16} field={:16} {} bytes, {} match state(s)",
            entry.op.as_str(),
            entry.field.as_str(),
            entry.dfa_bytes.len(),
            entry.state_map.len()
        );
    }

    let blob = serialize_entries(&compiled.entries);
    fs::write("kfilter_rules.dfa", &blob).expect("failed to write kfilter_rules.dfa");
    println!("wrote kfilter_rules.dfa ({} bytes, packed multi-DFA blob)", blob.len());

    write_state_map_json(&path, &compiled);

    match send_to_driver(&blob) {
        Ok(()) => println!("sent to {DEVICE_PATH}: driver ruleset updated"),
        Err(e) => {
            eprintln!("{e}");
            std::process::exit(1);
        }
    }
}
