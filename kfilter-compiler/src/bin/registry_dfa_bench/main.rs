//! Native (non-Windows) benchmark: compiles the MITRE ATT&CK registry
//! ruleset (see ../../registry_mitre.rules, generated from the ATT&CK
//! enterprise STIX dataset) into a sparse DFA using the same
//! glob/exact -> regex -> regex_automata pipeline as main.rs, and
//! reports the resulting size. Skips the Windows driver IOCTL send
//! since this is meant to run standalone for sizing purposes.

use regex_automata::dfa::dense;
use std::time::Instant;

mod registry_patterns_data;
use registry_patterns_data::REGISTRY_PATTERNS;

fn main() {
    let patterns: Vec<&str> = REGISTRY_PATTERNS.to_vec();
    println!("loaded {} regex patterns", patterns.len());

    let t0 = Instant::now();
    let dfa = dense::Builder::new()
        .build_many(&patterns)
        .expect("failed to build dense DFA from patterns");
    let dense_elapsed = t0.elapsed();

    let (dense_bytes, _padding) = dfa.to_bytes_native_endian();

    let t1 = Instant::now();
    let sparse = dfa.to_sparse().expect("failed to convert to sparse DFA");
    let sparse_elapsed = t1.elapsed();

    let sparse_bytes = sparse.to_bytes_native_endian();

    std::fs::write("registry_rules.dfa", &sparse_bytes)
        .expect("failed to write registry_rules.dfa");

    println!(
        "dense DFA:  {} bytes ({:.2} KiB, {:.2} MiB) -- build time {:?}",
        dense_bytes.len(),
        dense_bytes.len() as f64 / 1024.0,
        dense_bytes.len() as f64 / (1024.0 * 1024.0),
        dense_elapsed
    );
    println!(
        "sparse DFA: {} bytes ({:.2} KiB, {:.2} MiB) -- convert time {:?}",
        sparse_bytes.len(),
        sparse_bytes.len() as f64 / 1024.0,
        sparse_bytes.len() as f64 / (1024.0 * 1024.0),
        sparse_elapsed
    );
    println!(
        "sparse/dense ratio: {:.1}%",
        100.0 * sparse_bytes.len() as f64 / dense_bytes.len() as f64
    );
    println!("written to registry_rules.dfa");
}
