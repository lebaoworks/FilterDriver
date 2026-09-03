//! Simulation/testing harness: compiles a `.rules` file (via
//! `kfilter_compiler`, the same code path the real CLI uses) into one
//! DFA per (op, field), loads each into a `kfilter_lib::Ruleset` (the
//! same matcher the kernel driver runs, minus the `kernel` feature),
//! and evaluates a file of structured events against each step's full
//! AND/OR condition -- entirely offline, no driver or IOCTL needed.
//!
//! `.evt` format: one event per line,
//!   op=<op> <field1>="<value1>" <field2>="<value2>" ...
//! (same quoting/escaping as `.rules` values). Blank lines and
//! `#`-comments are skipped.
//!
//! For each field present on an event, looks up the entry compiled
//! for `(event.op, field_name)` and runs its `Ruleset::match_state`
//! once. A step's OR-group is satisfied once every field its
//! conditions require has been hit; the step overall is satisfied
//! once any one group is.

use kfilter_compiler::{compile_ruleset, parse_rules_file, Field, Op, Step};
use kfilter_lib::Ruleset;
use std::collections::{BTreeMap, BTreeSet, HashMap};
use std::env;
use std::fs;

struct Event {
    line: u32,
    op: Op,
    op_str: String,
    fields: Vec<(String, String)>,
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

/// Parses one `.evt` line: `op=<op> <field>="<value>" ...`.
fn parse_event_line(line_no: u32, line: &str) -> Option<Event> {
    let rest = line.strip_prefix("op=")?;
    let mut parts = rest.splitn(2, char::is_whitespace);
    let op_str = parts.next()?.to_string();
    let op = Op::from_str(&op_str)?;
    let remainder = parts.next().unwrap_or("").trim_start();

    let chars: Vec<char> = remainder.chars().collect();
    let n = chars.len();
    let mut i = 0;
    let mut fields = Vec::new();

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

        if i >= n || chars[i] != '=' {
            return None;
        }
        i += 1;
        if i >= n || chars[i] != '"' {
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
        i += 1;

        fields.push((field, unescape(&raw_value)));
    }

    Some(Event { line: line_no, op, op_str, fields })
}

fn parse_events_file(path: &str) -> Vec<Event> {
    let content = fs::read_to_string(path).unwrap_or_else(|e| {
        eprintln!("failed to read {path}: {e}");
        std::process::exit(1);
    });

    let mut events = Vec::new();
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
        match parse_event_line(line_no, line) {
            Some(ev) => events.push(ev),
            None => eprintln!("{path}:{line_no}: malformed event line, skipping: {line:?}"),
        }
    }
    events
}

/// Distinct fields required by `steps[line].groups[group]`.
fn required_fields(steps_by_line: &HashMap<u32, &Step>, line: u32, group: u32) -> BTreeSet<Field> {
    steps_by_line
        .get(&line)
        .and_then(|s| s.groups.get(group as usize))
        .map(|conds| conds.iter().map(|c| c.field).collect())
        .unwrap_or_default()
}

fn main() {
    let mut args = env::args().skip(1);
    let (rules_path, events_path) = match (args.next(), args.next()) {
        (Some(r), Some(e)) => (r, e),
        _ => {
            eprintln!("usage: kfilter-cli <rules-file> <events-file>");
            std::process::exit(1);
        }
    };

    let steps = parse_rules_file(&rules_path);
    let compilable: usize = steps.iter().flat_map(|s| &s.groups).map(Vec::len).sum();
    println!(
        "{rules_path}: {} step(s) with compilable conditions -> {compilable} pattern(s)",
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

    // One Ruleset per compiled entry, keyed by (op, field) for O(1)-ish
    // lookup per event field below.
    let rulesets: Vec<Ruleset> = compiled
        .entries
        .iter()
        .map(|e| {
            Ruleset::from_bytes(&e.dfa_bytes).unwrap_or_else(|err| {
                eprintln!("failed to load DFA for ({}, {}): {err}", e.op.as_str(), e.field.as_str());
                std::process::exit(1);
            })
        })
        .collect();
    let entry_index: HashMap<(Op, Field), usize> = compiled
        .entries
        .iter()
        .enumerate()
        .map(|(i, e)| ((e.op, e.field), i))
        .collect();

    let steps_by_line: HashMap<u32, &Step> = steps.iter().map(|s| (s.line, s)).collect();
    let events = parse_events_file(&events_path);

    let mut matched = 0usize;
    println!("\n--- results ---");
    for event in &events {
        // (line, group) -> set of fields this event satisfied a
        // condition of that group for.
        let mut hits: BTreeMap<(u32, u32), BTreeSet<Field>> = BTreeMap::new();

        for (field_name, value) in &event.fields {
            let Some(field) = Field::from_str(field_name) else {
                continue; // unknown field name, no rule could reference it
            };
            let Some(&entry_idx) = entry_index.get(&(event.op, field)) else {
                continue; // no rule compiled for this (op, field) at all
            };
            let Some(state_id) = rulesets[entry_idx].match_state(value.as_bytes()) else {
                continue;
            };
            let Some(infos) = compiled.entries[entry_idx].state_map.get(&state_id) else {
                continue;
            };
            for info in infos {
                hits.entry((info.line, info.group)).or_default().insert(field);
            }
        }

        let mut event_matched = false;
        for ((line, group), hit_fields) in &hits {
            let required = required_fields(&steps_by_line, *line, *group);
            if !required.is_subset(hit_fields) {
                continue;
            }
            event_matched = true;
            let step = steps_by_line.get(line);
            let (pattern_name, label) = step
                .map(|s| (s.pattern_name.as_str(), s.label.as_str()))
                .unwrap_or(("?", "?"));
            println!(
                "{events_path}:{}: op={} -> {rules_path}:{line} ({pattern_name}.{label}, group {group}) MATCH",
                event.line, event.op_str
            );
        }
        if event_matched {
            matched += 1;
        } else {
            println!("{events_path}:{}: op={} -> no step fully satisfied", event.line, event.op_str);
        }
    }

    println!("\n{matched}/{} event(s) satisfied at least one step", events.len());
}
