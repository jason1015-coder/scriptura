//! ## Fold Engine — pure fold-range computation (from `foldmanager.cpp`).
//! Qt keeps `paintFoldIndicator` + hidden-line viewport only.

use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct FoldRegion {
    pub start_line: u32,
    pub end_line: u32,
    pub indent_level: u32,
    pub collapsed: bool,
}

/// Detect keyword-based folds (if/else/for/while/function/class/struct/enum/try/catch/switch/case).
pub fn detect_keyword_folds(lines: &[String]) -> Vec<FoldRegion> {
    let keywords = ["if", "else", "else if", "for", "while", "do", "function",
        "class", "struct", "enum", "try", "catch", "switch", "case"];
    let mut out = vec![];
    for (idx, line) in lines.iter().enumerate() {
        let trimmed = line.trim();
        if !keywords.iter().any(|kw| trimmed.starts_with(kw)) {
            continue;
        }
        let indent = indent_of(line);
        let mut end_line = idx;
        for (j, next) in lines.iter().enumerate().skip(idx + 1) {
            if next.trim().is_empty() { continue; }
            let next_indent = indent_of(next);
            if next_indent <= indent && !next.trim().starts_with("//") {
                end_line = j - 1;
                break;
            }
            end_line = j;
        }
        if end_line > idx {
            out.push(FoldRegion { start_line: idx as u32, end_line: end_line as u32, indent_level: indent, collapsed: false });
        }
    }
    out.sort_by_key(|r| (r.start_line, r.end_line));
    out
}

/// Detect brace folds by scanning lines. String/comment aware (basic).
pub fn detect_brace_folds(lines: &[String]) -> Vec<FoldRegion> {
    let mut stack: Vec<(usize, u32)> = vec![];
    let mut out = vec![];
    for (idx, line) in lines.iter().enumerate() {
        let indent = indent_of(line);
        let opens = count_outside_strings(line, '{') + count_outside_strings(line, '(');
        let closes = count_outside_strings(line, '}') + count_outside_strings(line, ')');
        for _ in 0..opens { stack.push((idx, indent)); }
        for _ in 0..closes {
            if let Some((s, lvl)) = stack.pop() {
                if idx > s {
                    out.push(FoldRegion { start_line: s as u32, end_line: idx as u32, indent_level: lvl, collapsed: false });
                }
            }
        }
    }
    out.sort_by_key(|r| (r.start_line, r.end_line));
    out
}

/// Detect indent-based folds (python-like): a run of deeper-indented lines.
pub fn detect_indent_folds(lines: &[String]) -> Vec<FoldRegion> {
    let mut out = vec![];
    let mut i = 0;
    while i < lines.len() {
        if lines[i].trim().is_empty() { i += 1; continue; }
        let base = indent_of(&lines[i]);
        let mut j = i + 1;
        while j < lines.len() && (lines[j].trim().is_empty() || indent_of(&lines[j]) > base) { j += 1; }
        if j - i >= 2 && j - 1 > i {
            // ensure at least one deeper line
            if lines[i + 1..j].iter().any(|l| !l.trim().is_empty() && indent_of(l) > base) {
                out.push(FoldRegion { start_line: i as u32, end_line: (j - 1) as u32, indent_level: base, collapsed: false });
            }
        }
        i += 1;
    }
    out
}

fn indent_of(s: &str) -> u32 {
    let mut n = 0;
    for c in s.chars() { match c { ' ' => n += 1, '\t' => n += 4, _ => break } }
    n
}

fn count_outside_strings(line: &str, target: char) -> usize {
    let mut in_s = false; let mut in_d = false; let mut n = 0;
    let mut it = line.chars().peekable();
    while let Some(c) = it.next() {
        if c == '\\' { it.next(); continue; }
        if c == '\'' && !in_d { in_s = !in_s; continue; }
        if c == '"' && !in_s { in_d = !in_d; continue; }
        if !in_s && !in_d {
            if c == '/' && it.peek() == Some(&'/') { break; }
            if c == target { n += 1; }
        }
    }
    n
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn brace_fold() {
        let lines = vec!["fn f() {".into(), "  x();".into(), "}".into()];
        let r = detect_brace_folds(&lines);
        assert_eq!(r.len(), 1);
        assert_eq!((r[0].start_line, r[0].end_line), (0, 2));
    }
}
