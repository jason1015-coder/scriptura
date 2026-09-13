//! ## Edit Engine — pure typing decisions (moved from `codeeditor.cpp`).
//!
//! Ports `CodeEditor::handleSmartIndent` (~1061), `handleBracketAutoClose`
//! (~1085), occurrence search (`selectNextOccurrence`), Tab handling.
//! Qt forwards `key + line_text + cols`; Rust returns the decision. No Qt here.

/// Compute the indent string to insert after Enter.
/// Mirrors `handleSmartIndent`: base indent + one extra level if trimmed line
/// ends with `{`, `(` or `[`.
pub fn smart_indent_insert(line_text: &str, tab_width: usize) -> String {
    let mut indent = 0usize;
    for c in line_text.chars() {
        match c {
            ' ' => indent += 1,
            '\t' => indent += tab_width.max(1),
            _ => break,
        }
    }
    let trimmed = line_text.trim_end();
    if trimmed.ends_with('{') || trimmed.ends_with('(') || trimmed.ends_with('[') {
        indent += tab_width.max(1);
    }
    format!("\n{}", " ".repeat(indent))
}

/// Count leading indent width (spaces + tab_width per tab).
pub fn indent_width(line_text: &str, tab_width: usize) -> usize {
    let mut indent = 0;
    for c in line_text.chars() {
        match c { ' ' => indent += 1, '\t' => indent += tab_width.max(1), _ => break }
    }
    indent
}

#[derive(Debug, PartialEq)]
pub enum BracketDecision {
    /// Insert open+close and put caret between: `(|)`.
    AutoClose { close: char },
    /// Skip over an existing closing quote: don't insert.
    SkipOver,
    /// Do nothing special — let Qt/Rust insert the typed char normally.
    InsertNormal,
}

/// Decide bracket auto-close. Ports `handleBracketAutoClose`.
/// `typed` is the typed char, `next` the char under caret (None at EOL).
pub fn bracket_decision(typed: char, next: Option<char>) -> BracketDecision {
    let close = match typed { '(' => ')', '[' => ']', '{' => '}', '"' => '"', '\'' => '\'', _ => return BracketDecision::InsertNormal };
    if typed == close && (typed == '"' || typed == '\'') {
        if next == Some(typed) { return BracketDecision::SkipOver; }
    }
    if let Some(n) = next {
        if n.is_alphanumeric() || n == '_' { return BracketDecision::InsertNormal; }
    }
    BracketDecision::AutoClose { close }
}

/// Find all non-overlapping occurrences of `needle` in `line`. Char indices.
pub fn occurrences_in_line(line: &str, needle: &str) -> Vec<(usize, usize)> {
    if needle.is_empty() { return vec![]; }
    let lchars: Vec<char> = line.chars().collect();
    let nchars: Vec<char> = needle.chars().collect();
    if nchars.len() > lchars.len() { return vec![]; }
    let mut out = vec![];
    let mut i = 0;
    while i + nchars.len() <= lchars.len() {
        if lchars[i..i + nchars.len()] == nchars[..] {
            out.push((i, i + nchars.len()));
            i += nchars.len().max(1);
        } else { i += 1; }
    }
    out
}

/// Next occurrence of `needle` in full text after flat char offset `from`.
/// Returns (start_char_offset, end_char_offset).
pub fn next_occurrence(text: &str, needle: &str, from: usize) -> Option<(usize, usize)> {
    if needle.is_empty() { return None; }
    let chars: Vec<char> = text.chars().collect();
    let n: Vec<char> = needle.chars().collect();
    if n.is_empty() || n.len() > chars.len() { return None; }
    let mut i = from.min(chars.len());
    loop {
        if i + n.len() > chars.len() {
            if from == 0 { return None; }
            i = 0; // wrap once
            if 0 + n.len() > from { return None; }
        }
        if chars[i..i + n.len()] == n[..] { return Some((i, i + n.len())); }
        i += 1;
        if i + n.len() > chars.len() && from == 0 { return None; }
        if i >= chars.len() {
            if from == 0 { return None; }
            i = 0;
            if n.len() > from { return None; }
        }
        if i == from { return None; }
    }
}

/// Flat char offset -> UTF-16 units (Qt/QTextCursor/QString index space).
pub fn char_offset_to_utf16(text: &str, char_off: usize) -> usize {
    text.chars().take(char_off.min(text.chars().count()))
        .map(|c| c.len_utf16()).sum()
}

/// UTF-16 units -> flat char offset.
pub fn utf16_to_char_offset(text: &str, utf16: usize) -> usize {
    let mut u = 0;
    for (i, c) in text.chars().enumerate() {
        if u >= utf16 { return i; }
        u += c.len_utf16();
    }
    text.chars().count()
}

/// Next occurrence with UTF-16 offsets (Qt document position space).
/// `from_utf16` is a UTF-16 offset into the flat text.
pub fn next_occurrence_utf16(
    text: &str, needle: &str, from_utf16: usize,
) -> Option<(usize, usize)> {
    let from_char = utf16_to_char_offset(text, from_utf16);
    let (s, e) = next_occurrence(text, needle, from_char)?;
    Some((char_offset_to_utf16(text, s), char_offset_to_utf16(text, e)))
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn indent_after_brace() {
        assert_eq!(smart_indent_insert("    foo {", 4), "\n        ");
        assert_eq!(smart_indent_insert("    foo", 4), "\n    ");
        assert_eq!(smart_indent_insert("\tx", 4), "\n    ");
    }
    #[test] fn bracket_close_and_skip() {
        assert_eq!(bracket_decision('(', Some(' ')), BracketDecision::AutoClose { close: ')' });
        assert_eq!(bracket_decision('(', Some('a')), BracketDecision::InsertNormal);
        assert_eq!(bracket_decision('"', Some('"')), BracketDecision::SkipOver);
        assert_eq!(bracket_decision('x', None), BracketDecision::InsertNormal);
    }
    #[test] fn occurrences() {
        assert_eq!(occurrences_in_line("aaa", "aa"), vec![(0, 2)]);
        assert_eq!(next_occurrence("hello hello", "hello", 1), Some((6, 11)));
        // UTF-16 mapping with emoji (2 units): offsets shift correctly.
        let t = "a\u{1F600} hello hello";
        let (s, e) = next_occurrence_utf16(t, "hello", 0).unwrap();
        assert!(e > s);
        assert_eq!(char_offset_to_utf16(t, utf16_to_char_offset(t, s)), s);
    }
}
