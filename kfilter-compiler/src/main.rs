//! Part 1 of the split: parses a `.rules` file, compiles each step's
//! field conditions into a ruleset blob (line-number table + sparse
//! DFA), and sends it to the running kfilter driver over
//! IOCTL_KFILTER_LOAD_RULES. Also writes a local copy for inspection.
//!
//! Grammar covered (see README.md R1-R12):
//!   pattern <name> [scope=<value>]
//!       step <label> op=<operation> [<field><op>"<value>" ...]
//!   end
//! Operators: `=` exact, `~~` glob, `~` contains, `=~` regex passthrough.
//!
//! What's intentionally NOT compiled here (skipped with a warning,
//! never a hard error -- the kernel filter's scope is prefiltering,
//! not correlation):
//!   - `$step.field` references (cross-step comparison; needs runtime
//!     state, not a static regex).
//!   - Steps with zero field conditions (would need per-op dispatch to
//!     express "matches whenever this op fires", not implemented yet).

use regex_automata::dfa::dense;
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

// Wire format sent to the driver, must match kfilter-runtime/src/lib.rs:
//   [magic: u32 LE][version: u32 LE][pattern_count: u32 LE]
//   [line_numbers: pattern_count * u32 LE]
//   [sparse DFA bytes: rest]
const WIRE_MAGIC: u32 = 0x4B46524C; // "KFRL"
const WIRE_VERSION: u32 = 1;

// ---- .rules parsing ---------------------------------------------------

#[derive(Clone, Copy)]
enum CondOp {
    Exact,
    Glob,
    Contains,
    Regex,
}

struct Condition {
    #[allow(dead_code)]
    field: String,
    op: CondOp,
    value: String,
}

struct Step {
    line: u32,
    // Kept for future diagnostics (e.g. a verbose/--map mode); the
    // compiled ruleset blob only needs `line` (see WIRE_MAGIC format).
    #[allow(dead_code)]
    pattern_name: String,
    #[allow(dead_code)]
    label: String,
    conditions: Vec<Condition>,
}

fn unescape(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    let mut chars = s.chars();
    while let Some(c) = chars.next() {
        if c == '\\' {
            match chars.next() {
                Some('\\') => out.push('\\'),
                Some('"') => out.push('"'),
                Some(other) => {
                    out.push('\\');
                    out.push(other);
                }
                None => out.push('\\'),
            }
        } else {
            out.push(c);
        }
    }
    out
}

fn regex_escape(s: &str) -> String {
    let mut out = String::new();
    for c in s.chars() {
        if "\\.+*?()|[]{}^$".contains(c) {
            out.push('\\');
        }
        out.push(c);
    }
    out
}

/// rule `~~` operator: glob wildcard, anchored full match.
fn glob_to_regex(glob: &str) -> String {
    let mut re = String::from("^");
    for c in glob.chars() {
        match c {
            '*' => re.push_str(".*"),
            '?' => re.push('.'),
            _ => re.push_str(&regex_escape(&c.to_string())),
        }
    }
    re.push('$');
    re
}

/// rule `=` operator: exact match, anchored.
fn exact_to_regex(s: &str) -> String {
    format!("^{}$", regex_escape(s))
}

/// rule `~` operator: substring match, unanchored.
fn contains_to_regex(s: &str) -> String {
    regex_escape(s)
}

fn compile_condition(c: &Condition) -> String {
    match c.op {
        CondOp::Exact => exact_to_regex(&c.value),
        CondOp::Glob => glob_to_regex(&c.value),
        CondOp::Contains => contains_to_regex(&c.value),
        CondOp::Regex => c.value.clone(),
    }
}

/// Parses the condition text after `op=<value>` on a `step` line,
/// respecting quoted values that may contain spaces and `\\`/`\"`
/// escapes. Returns `None` if the text can't be statically compiled
/// (unknown operator, malformed field, or an unquoted/`$ref` value).
fn parse_conditions(rest: &str) -> Option<Vec<Condition>> {
    let chars: Vec<char> = rest.chars().collect();
    let n = chars.len();
    let mut i = 0;
    let mut conditions = Vec::new();

    while i < n {
        while i < n && chars[i].is_whitespace() {
            i += 1;
        }
        if i >= n {
            break;
        }

        let field_start = i;
        while i < n && (chars[i].is_alphanumeric() || chars[i] == '_' || chars[i] == '.') {
            i += 1;
        }
        if i == field_start {
            return None;
        }
        let field: String = chars[field_start..i].iter().collect();

        let op = if chars[i..].starts_with(&['=', '~']) {
            i += 2;
            CondOp::Regex
        } else if chars[i..].starts_with(&['~', '~']) {
            i += 2;
            CondOp::Glob
        } else if i < n && chars[i] == '~' {
            i += 1;
            CondOp::Contains
        } else if i < n && chars[i] == '=' {
            i += 1;
            CondOp::Exact
        } else {
            return None;
        };

        if i >= n || chars[i] != '"' {
            // Bareword value (e.g. `$drop.image`) -- not statically
            // compilable.
            return None;
        }
        i += 1;
        let value_start = i;
        let mut closed = false;
        while i < n {
            if chars[i] == '\\' && i + 1 < n {
                i += 2;
            } else if chars[i] == '"' {
                closed = true;
                break;
            } else {
                i += 1;
            }
        }
        if !closed {
            return None;
        }
        let raw_value: String = chars[value_start..i].iter().collect();
        i += 1; // consume closing quote

        conditions.push(Condition {
            field,
            op,
            value: unescape(&raw_value),
        });
    }

    Some(conditions)
}

fn parse_rules_file(path: &str) -> Vec<Step> {
    let content = fs::read_to_string(path).unwrap_or_else(|e| {
        eprintln!("failed to read {path}: {e}");
        std::process::exit(1);
    });

    let mut steps = Vec::new();
    let mut current_pattern: Option<String> = None;

    for (idx, raw_line) in content.lines().enumerate() {
        let line_no = (idx + 1) as u32;
        let line = raw_line.trim();
        let line = match line.find('#') {
            Some(pos) => line[..pos].trim_end(),
            None => line,
        };
        if line.is_empty() {
            continue;
        }

        if let Some(rest) = line.strip_prefix("pattern ") {
            let name = rest.split_whitespace().next().unwrap_or("").to_string();
            current_pattern = Some(name);
        } else if line == "end" {
            current_pattern = None;
        } else if let Some(rest) = line.strip_prefix("step ") {
            let Some(pattern_name) = current_pattern.clone() else {
                eprintln!("{path}:{line_no}: `step` outside of a `pattern` block, skipping");
                continue;
            };
            let rest = rest.trim_start();
            let mut parts = rest.splitn(2, char::is_whitespace);
            let label = parts.next().unwrap_or("").to_string();
            let remainder = parts.next().unwrap_or("").trim_start();

            let Some(op_rest) = remainder.strip_prefix("op=") else {
                eprintln!("{path}:{line_no}: step '{pattern_name}.{label}' missing `op=`, skipping");
                continue;
            };
            let mut op_parts = op_rest.splitn(2, char::is_whitespace);
            let _op_name = op_parts.next().unwrap_or("");
            let cond_text = op_parts.next().unwrap_or("");

            match parse_conditions(cond_text) {
                Some(conditions) if !conditions.is_empty() => {
                    steps.push(Step {
                        line: line_no,
                        pattern_name,
                        label,
                        conditions,
                    });
                }
                Some(_) => {
                    eprintln!(
                        "{path}:{line_no}: step '{pattern_name}.{label}' has no field conditions -- skipping (needs per-op dispatch, not implemented yet)"
                    );
                }
                None => {
                    eprintln!(
                        "{path}:{line_no}: step '{pattern_name}.{label}' has an unsupported condition (e.g. $ref) -- skipping"
                    );
                }
            }
        }
    }

    steps
}

// ---- driver IOCTL delivery --------------------------------------------

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

fn main() {
    let path = env::args().nth(1).unwrap_or_else(|| {
        eprintln!("usage: kfilter-compiler <rules-file>");
        std::process::exit(1);
    });

    let steps = parse_rules_file(&path);

    let mut patterns: Vec<String> = Vec::new();
    let mut line_numbers: Vec<u32> = Vec::new();
    for step in &steps {
        for cond in &step.conditions {
            patterns.push(compile_condition(cond));
            line_numbers.push(step.line);
        }
    }

    println!(
        "{path}: {} step(s) with compilable conditions -> {} pattern(s)",
        steps.len(),
        patterns.len()
    );

    if patterns.is_empty() {
        eprintln!("no compilable patterns found in {path}, nothing to send");
        std::process::exit(1);
    }

    let dfa = dense::Builder::new().build_many(&patterns).unwrap_or_else(|e| {
        eprintln!("failed to build DFA: {e}");
        std::process::exit(1);
    });
    let sparse = dfa.to_sparse().expect("failed to convert to sparse DFA");
    let dfa_bytes = sparse.to_bytes_native_endian();

    let mut blob = Vec::with_capacity(12 + line_numbers.len() * 4 + dfa_bytes.len());
    blob.extend_from_slice(&WIRE_MAGIC.to_le_bytes());
    blob.extend_from_slice(&WIRE_VERSION.to_le_bytes());
    blob.extend_from_slice(&(line_numbers.len() as u32).to_le_bytes());
    for ln in &line_numbers {
        blob.extend_from_slice(&ln.to_le_bytes());
    }
    blob.extend_from_slice(&dfa_bytes);

    fs::write("kfilter_rules.dfa", &blob).expect("failed to write kfilter_rules.dfa");
    println!(
        "wrote kfilter_rules.dfa ({} bytes total: {} header/line-map + {} DFA)",
        blob.len(),
        12 + line_numbers.len() * 4,
        dfa_bytes.len()
    );

    match send_to_driver(&blob) {
        Ok(()) => println!("sent to {DEVICE_PATH}: driver ruleset updated"),
        Err(e) => {
            eprintln!("{e}");
            std::process::exit(1);
        }
    }
}
