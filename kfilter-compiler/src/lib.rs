//! `.rules` parsing + compilation to **one DFA per (op, field)**, as a
//! library so both `kfilter-compiler`'s own CLI (`src/main.rs`, which
//! also pushes the result to a live driver over IOCTL) and
//! `kfilter-cli` (pure user-mode simulation) can reuse the exact same
//! logic.
//!
//! Grammar covered (see README.md R1-R14):
//!   pattern <name> [scope=<value>]
//!       step <label> op=<operation> [<field><op>"<value>" [and|or <field><op>"<value>" ...]]
//!   end
//! Operators: `=` exact, `~~` glob, `~` contains, `=~` regex passthrough.
//!
//! Conditions on a step combine with `and`/`or` (`and` binds tighter,
//! same precedence as most languages); omitting a keyword between two
//! conditions defaults to `and` (backward compatible with every
//! existing `.rules` file, which never uses these keywords). A step's
//! condition list is therefore stored as **groups**: `Vec<Vec<Condition>>`
//! in DNF form, i.e. `a and b or c and d` -> `[[a, b], [c, d]]`, and the
//! step is satisfied when *any* group has *all* of its conditions hold.
//!
//! Compiled output is **one DFA per distinct (op, field) pair**, not
//! one flat DFA for the whole ruleset: measured on `registry_mitre.rules`
//! (250 patterns), splitting this way produced *smaller* total DFA
//! bytes (-58%), *faster* builds (-88%), and *faster* per-call matches
//! (-25%) than a single shared DFA -- splitting removes states a flat
//! DFA otherwise needs solely to keep unrelated fields' patterns
//! distinguishable from each other, even though a real event never
//! tests two different fields' values against the same string. It
//! also sidesteps the "wildcard from field A pollutes field B's
//! matches" correctness issue by construction (a DFA for
//! `(registry_set, value_name)` simply never contains a `key_path`
//! pattern), instead of needing the op/field post-filter this crate
//! used to require of its callers.
//!
//! What's intentionally NOT compiled here (skipped with a warning,
//! never a hard error -- the kernel filter's scope is prefiltering,
//! not correlation):
//!   - `$step.field` references (cross-step comparison; needs runtime
//!     state, not a static regex).
//!   - Steps with zero field conditions (would need per-op dispatch to
//!     express "matches whenever this op fires", not implemented yet).

use regex_automata::dfa::{dense, sparse, Automaton};
use regex_automata::util::primitives::StateID;
use regex_automata::{Input, MatchKind};
use std::collections::{BTreeMap, HashSet, VecDeque};

// ---- .rules parsing ---------------------------------------------------

#[derive(Clone, Copy)]
pub enum CondOp {
    Exact,
    Glob,
    Contains,
    Regex,
}

pub struct Condition {
    pub field: String,
    pub op: CondOp,
    pub value: String,
}

pub struct Step {
    pub line: u32,
    pub pattern_name: String,
    pub label: String,
    pub op: String,
    /// DNF: `groups[g]` is one AND-group; the step is satisfied when
    /// any group has all of its conditions hold. A step with no
    /// `and`/`or` keywords (the common case, and every existing
    /// `.rules` file) has exactly one group.
    pub groups: Vec<Vec<Condition>>,
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
pub fn glob_to_regex(glob: &str) -> String {
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
pub fn exact_to_regex(s: &str) -> String {
    format!("^{}$", regex_escape(s))
}

/// rule `~` operator: substring match, unanchored.
pub fn contains_to_regex(s: &str) -> String {
    regex_escape(s)
}

pub fn compile_condition(c: &Condition) -> String {
    match c.op {
        CondOp::Exact => exact_to_regex(&c.value),
        CondOp::Glob => glob_to_regex(&c.value),
        CondOp::Contains => contains_to_regex(&c.value),
        CondOp::Regex => c.value.clone(),
    }
}

/// Parses the condition text after `op=<value>` on a `step` line into
/// DNF groups (see the crate docs for the `and`/`or` grammar),
/// respecting quoted values that may contain spaces and `\\`/`\"`
/// escapes. Returns `None` if the text can't be statically compiled
/// (unknown operator, malformed field, an unquoted/`$ref` value, or a
/// stray/leading/trailing `and`/`or` with nothing on one side).
fn parse_conditions(rest: &str) -> Option<Vec<Vec<Condition>>> {
    let chars: Vec<char> = rest.chars().collect();
    let n = chars.len();
    let mut i = 0;
    let mut groups: Vec<Vec<Condition>> = vec![Vec::new()];

    loop {
        while i < n && chars[i].is_whitespace() {
            i += 1;
        }
        if i >= n {
            break;
        }

        // Peek a bareword: is it the `and`/`or` combinator keyword
        // (only when followed by whitespace/end -- a field literally
        // named "and"/"or" immediately followed by an operator, e.g.
        // `or~~"x"`, still parses as a field below).
        let word_start = i;
        let mut j = i;
        while j < n && chars[j].is_alphabetic() {
            j += 1;
        }
        let word: String = chars[word_start..j].iter().collect();
        let word_is_standalone = j >= n || chars[j].is_whitespace();
        if word_is_standalone && word.eq_ignore_ascii_case("or") {
            groups.push(Vec::new());
            i = j;
            continue;
        }
        if word_is_standalone && word.eq_ignore_ascii_case("and") {
            i = j; // no-op: conditions in the same group are already AND
            continue;
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

        groups.last_mut().unwrap().push(Condition {
            field,
            op,
            value: unescape(&raw_value),
        });
    }

    // A stray leading/trailing/doubled `or` leaves an empty group
    // (e.g. `a or` or `or a` or `a or or b`) -- reject rather than
    // silently drop it.
    if groups.iter().any(Vec::is_empty) {
        return None;
    }

    Some(groups)
}

/// Parses `.rules` source text into steps with compilable conditions.
/// Warnings for skipped steps (unsupported `$ref`, zero conditions,
/// malformed `step`/`op=`) go to stderr; `path` is only used to make
/// those messages point at the right file, this function does no I/O
/// itself.
pub fn parse_rules(path: &str, content: &str) -> Vec<Step> {
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
            let op_name = op_parts.next().unwrap_or("").to_string();
            let cond_text = op_parts.next().unwrap_or("");

            if cond_text.trim().is_empty() {
                eprintln!(
                    "{path}:{line_no}: step '{pattern_name}.{label}' has no field conditions -- skipping (needs per-op dispatch, not implemented yet)"
                );
                continue;
            }

            match parse_conditions(cond_text) {
                Some(groups) => {
                    steps.push(Step {
                        line: line_no,
                        pattern_name,
                        label,
                        op: op_name,
                        groups,
                    });
                }
                None => {
                    eprintln!(
                        "{path}:{line_no}: step '{pattern_name}.{label}' has an unsupported condition (e.g. $ref) or malformed and/or -- skipping"
                    );
                }
            }
        }
    }

    steps
}

/// Convenience wrapper: reads `path` and calls [`parse_rules`]. Prints
/// an error and exits the process on read failure -- both binaries
/// that use this library want that same fail-fast behavior.
pub fn parse_rules_file(path: &str) -> Vec<Step> {
    let content = std::fs::read_to_string(path).unwrap_or_else(|e| {
        eprintln!("failed to read {path}: {e}");
        std::process::exit(1);
    });
    parse_rules(path, &content)
}

// ---- compile steps -> per-(op,field) DFA + state_id -> lines table ----

/// A `.rules` line (and, within it, which OR-group -- see the crate
/// docs' DNF explanation) a match state resolves to, with rule/step
/// names for readability. Unlike the earlier single-flat-DFA design,
/// this no longer needs `op`/`field`: a [`RulesetEntry`] is already
/// scoped to exactly one (op, field) pair, so any state reached in
/// its DFA is automatically evidence for that field alone.
pub struct LineInfo {
    pub line: u32,
    /// Index into `step.groups`. Two conditions with the same `line`
    /// but different `group` are alternatives (`or`); same `line` and
    /// `group` means they're `and`-ed together and *all* need to have
    /// fired (possibly across different [`RulesetEntry`]s, if the
    /// group spans more than one field) for the step to be satisfied
    /// -- that correlation is what a caller evaluating a real event
    /// does with this data (see `kfilter-cli`'s module docs).
    pub group: u32,
    pub pattern_name: String,
    pub step: String,
}

/// One DFA covering every compiled pattern for a single (op, field)
/// pair, plus the table to resolve its match states back to `.rules`
/// lines.
pub struct RulesetEntry {
    pub op: String,
    pub field: String,
    /// Serialized sparse DFA, built with `MatchKind::All`. This is
    /// exactly the payload `kfilter_lib::Ruleset::from_bytes` expects
    /// for this entry (see [`serialize_entries`] for how entries are
    /// packed together for the kernel).
    pub dfa_bytes: Vec<u8>,
    /// `state_id -> [line info, ...]`, precomputed by walking every
    /// match state of this entry's DFA (`match_len`/`match_pattern`).
    /// **User mode only** -- the kernel never needs this table.
    pub state_map: BTreeMap<u32, Vec<LineInfo>>,
}

pub struct CompiledRuleset {
    pub entries: Vec<RulesetEntry>,
}

/// Groups every step's field conditions by (op, field), builds one
/// sparse DFA (`MatchKind::All`) per group, and precomputes each
/// group's `state_id -> lines` table by walking its match states.
pub fn compile_ruleset(steps: &[Step]) -> Result<CompiledRuleset, String> {
    // (op, field) -> [(regex pattern, line, group_idx), ...]
    let mut groups: BTreeMap<(String, String), Vec<(String, u32, u32)>> = BTreeMap::new();
    for step in steps {
        for (group_idx, group) in step.groups.iter().enumerate() {
            for cond in group {
                groups
                    .entry((step.op.clone(), cond.field.clone()))
                    .or_default()
                    .push((compile_condition(cond), step.line, group_idx as u32));
            }
        }
    }

    if groups.is_empty() {
        return Err("no compilable patterns".to_string());
    }

    let mut line_info: BTreeMap<u32, (&str, &str)> = BTreeMap::new();
    for step in steps {
        line_info.insert(step.line, (step.pattern_name.as_str(), step.label.as_str()));
    }

    let mut entries = Vec::with_capacity(groups.len());
    for ((op, field), items) in groups {
        let patterns: Vec<String> = items.iter().map(|(p, _, _)| p.clone()).collect();
        let owners: Vec<(u32, u32)> = items.iter().map(|(_, line, g)| (*line, *g)).collect();

        // MatchKind::All: keep every pattern's match info per state
        // (not just one "winner") so build_state_line_map can
        // enumerate them. The kernel-deployed DFA still only ever
        // returns 1 state id per search -- this only affects what's
        // *preserved in the DFA*, not what the runtime search API
        // reports on its own.
        let dfa = dense::Builder::new()
            .configure(dense::Config::new().match_kind(MatchKind::All))
            .build_many(&patterns)
            .map_err(|e| format!("failed to build DFA for ({op}, {field}): {e}"))?;
        let sparse = dfa
            .to_sparse()
            .map_err(|e| format!("failed to convert ({op}, {field}) DFA to sparse: {e}"))?;

        let raw_state_map = build_state_line_map(&sparse, &owners);
        let state_map: BTreeMap<u32, Vec<LineInfo>> = raw_state_map
            .into_iter()
            .map(|(state_id, owned)| {
                let infos = owned
                    .into_iter()
                    .map(|(line, group)| {
                        let (pattern_name, step) = line_info.get(&line).copied().unwrap_or(("?", "?"));
                        LineInfo {
                            line,
                            group,
                            pattern_name: pattern_name.to_string(),
                            step: step.to_string(),
                        }
                    })
                    .collect();
                (state_id, infos)
            })
            .collect();

        let dfa_bytes = sparse.to_bytes_native_endian();
        entries.push(RulesetEntry { op, field, dfa_bytes, state_map });
    }

    Ok(CompiledRuleset { entries })
}

/// Walks every state reachable from the DFA's start state (over all
/// 256 byte values) and, for each one, checks what its "end of
/// haystack" successor state looks like -- exactly mirroring
/// `kfilter_lib::Ruleset::match_state`'s own `start_state_forward` /
/// `next_state` (per byte) / `next_eoi_state` (once, at the end)
/// sequence. Any such end-of-haystack state that turns out to be a
/// match state gets recorded as `state_id -> [(line, group), ...]`
/// (deduplicated, since a state can represent several patterns that
/// all reduce to the same source line + group -- see the "duplicate
/// regex text" case discussed for `value_data~~"*.dll"`-style
/// conditions).
///
/// Must be run on the exact same `sparse::DFA` object whose bytes get
/// serialized and shipped -- state ids are only meaningful relative
/// to one specific compiled DFA.
fn build_state_line_map(
    dfa: &sparse::DFA<Vec<u8>>,
    owners: &[(u32, u32)],
) -> BTreeMap<u32, Vec<(u32, u32)>> {
    let mut map: BTreeMap<u32, Vec<(u32, u32)>> = BTreeMap::new();
    let mut seen: HashSet<StateID> = HashSet::new();
    let mut queue: VecDeque<StateID> = VecDeque::new();

    let start = dfa
        .start_state_forward(&Input::new(b""))
        .expect("start state");
    seen.insert(start);
    queue.push_back(start);

    while let Some(state) = queue.pop_front() {
        if !dfa.is_dead_state(state) {
            for byte in 0u16..=255 {
                let next = dfa.next_state(state, byte as u8);
                if seen.insert(next) {
                    queue.push_back(next);
                }
            }
        }

        let eoi = dfa.next_eoi_state(state);
        if dfa.is_match_state(eoi) {
            let mut owned: Vec<(u32, u32)> = Vec::new();
            for i in 0..dfa.match_len(eoi) {
                let pattern_id = dfa.match_pattern(eoi, i).as_usize();
                if let Some(&pair) = owners.get(pattern_id) {
                    if !owned.contains(&pair) {
                        owned.push(pair);
                    }
                }
            }
            owned.sort_unstable();
            map.entry(eoi.as_u32()).or_insert(owned);
        }
    }

    map
}

// ---- wire format: pack every entry's DFA for the kernel ---------------

/// `[magic: u32 LE][version: u32 LE][entry_count: u32 LE]`, followed
/// by `entry_count` repetitions of
/// `[op_len: u32 LE][op bytes][field_len: u32 LE][field bytes][dfa_len: u32 LE][dfa bytes]`.
/// Must match `kfilter-lib`'s `kfilter_load` parser exactly.
pub const WIRE_MAGIC: u32 = 0x4B46_524D; // "KFRM"
pub const WIRE_VERSION: u32 = 1;

/// Packs every entry into the blob `kfilter_load` (kernel) expects
/// over IOCTL_KFILTER_LOAD_RULES.
pub fn serialize_entries(entries: &[RulesetEntry]) -> Vec<u8> {
    let mut out = Vec::new();
    out.extend_from_slice(&WIRE_MAGIC.to_le_bytes());
    out.extend_from_slice(&WIRE_VERSION.to_le_bytes());
    out.extend_from_slice(&(entries.len() as u32).to_le_bytes());
    for e in entries {
        let op = e.op.as_bytes();
        let field = e.field.as_bytes();
        out.extend_from_slice(&(op.len() as u32).to_le_bytes());
        out.extend_from_slice(op);
        out.extend_from_slice(&(field.len() as u32).to_le_bytes());
        out.extend_from_slice(field);
        out.extend_from_slice(&(e.dfa_bytes.len() as u32).to_le_bytes());
        out.extend_from_slice(&e.dfa_bytes);
    }
    out
}
