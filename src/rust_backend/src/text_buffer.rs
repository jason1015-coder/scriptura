//! ## Text Buffer — Rust-owned source of truth for editor text.
//!
//! Qt (`CodeEditor : QPlainTextEdit`) becomes a thin view: it forwards keys
//! and renders. Storage, mutation, versioning and undo/redo live here.
//!
//! ### Coordinate contract (Qt UTF-16 vs Rust UTF-8)
//! * FFI takes `line: u32, col_utf16: u32` (QTextCursor convention).
//! * Internally converts UTF-16 col -> byte index. Never index from C++ raw.
//! * `version` bumps per mutation; used for LSP didChange + parity checks.

use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
struct UndoEntry {
    start: usize,
    text_before: String,
    text_after: String,
}

pub struct TextBuffer {
    text: String,
    version: u64,
    undo: Vec<UndoEntry>,
    redo: Vec<UndoEntry>,
    max_undo: usize,
}

impl TextBuffer {
    pub fn new() -> Self {
        Self { text: String::new(), version: 0,
               undo: Vec::new(), redo: Vec::new(), max_undo: 500 }
    }
    pub fn set_text(&mut self, s: &str) {
        let before = std::mem::replace(&mut self.text, s.to_string());
        self.push_undo(UndoEntry { start: 0,
            text_before: before, text_after: self.text.clone() });
        self.redo.clear();
        self.version += 1;
    }
    pub fn text(&self) -> &str { &self.text }
    pub fn version(&self) -> u64 { self.version }
    pub fn line_count(&self) -> usize {
        if self.text.is_empty() { return 1; }
        self.text.split('\n').count()
    }
    pub fn line_text(&self, line: usize) -> String {
        self.text.split('\n').nth(line).unwrap_or("").to_string()
    }
    /// (line, UTF-16 col) -> flat byte offset. Clamps safely.
    pub fn offset_of(&self, line: u32, col_utf16: u32) -> usize {
        let mut base = 0usize;
        for (i, l) in self.text.split('\n').enumerate() {
            if i == line as usize {
                let mut utf16 = 0u32;
                let mut byte = 0usize;
                for ch in l.chars() {
                    if utf16 >= col_utf16 { break; }
                    utf16 += ch.len_utf16() as u32;
                    byte += ch.len_utf8();
                }
                return base + byte;
            }
            base += l.len() + 1;
        }
        self.text.len()
    }
    /// Flat byte offset -> (line, col_utf16).
    pub fn line_col_of(&self, offset: usize) -> (u32, u32) {
        let off = offset.min(self.text.len());
        let off = floor_char_boundary(&self.text, off);
        let prefix = &self.text[..off];
        let line = prefix.matches('\n').count() as u32;
        let ls = prefix.rfind('\n').map(|i| i + 1).unwrap_or(0);
        let col: usize = prefix[ls..].chars().map(|c| c.len_utf16()).sum();
        (line, col as u32)
    }
    /// Insert at (line, col_utf16). Returns new caret (line, col_utf16).
    pub fn insert(&mut self, line: u32, col_utf16: u32, s: &str) -> (u32, u32) {
        let off = self.offset_of(line, col_utf16);
        self.text.insert_str(off, s);
        self.push_undo(UndoEntry { start: off,
            text_before: String::new(), text_after: s.to_string() });
        self.redo.clear();
        self.version += 1;
        self.line_col_of(off + s.len())
    }
    /// Delete [start, end). Returns deleted text.
    pub fn delete_range(&mut self, sl: u32, sc: u32, el: u32, ec: u32) -> String {
        let s = self.offset_of(sl, sc);
        let e = self.offset_of(el, ec);
        let (s, e) = if s <= e { (s, e) } else { (e, s) };
        let removed = self.text[s..e].to_string();
        self.text.replace_range(s..e, "");
        self.push_undo(UndoEntry { start: s,
            text_before: removed.clone(), text_after: String::new() });
        self.redo.clear();
        self.version += 1;
        removed
    }
    pub fn undo(&mut self) -> bool {
        let Some(e) = self.undo.pop() else { return false };
        let end = floor_char_boundary(&self.text,
            (e.start + e.text_after.len()).min(self.text.len()));
        let start = floor_char_boundary(&self.text,
            e.start.min(self.text.len()));
        self.text.replace_range(start..end, &e.text_before);
        self.redo.push(e);
        self.version += 1;
        true
    }
    pub fn redo(&mut self) -> bool {
        let Some(e) = self.redo.pop() else { return false };
        let end = floor_char_boundary(&self.text,
            (e.start + e.text_before.len()).min(self.text.len()));
        let start = floor_char_boundary(&self.text,
            e.start.min(self.text.len()));
        self.text.replace_range(start..end, &e.text_after);
        self.undo.push(e);
        self.version += 1;
        true
    }
    pub fn clear_undo(&mut self) { self.undo.clear(); self.redo.clear(); }
    pub fn can_undo(&self) -> bool { !self.undo.is_empty() }
    pub fn can_redo(&self) -> bool { !self.redo.is_empty() }
    fn push_undo(&mut self, e: UndoEntry) {
        self.undo.push(e);
        if self.undo.len() > self.max_undo { self.undo.remove(0); }
    }
}

impl Default for TextBuffer { fn default() -> Self { Self::new() } }

fn floor_char_boundary(s: &str, mut idx: usize) -> usize {
    while idx > 0 && !s.is_char_boundary(idx) { idx -= 1; }
    idx.min(s.len())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn basic_insert_delete() {
        let mut b = TextBuffer::new();
        b.set_text("hello\nworld");
        assert_eq!(b.line_count(), 2);
        let (l, c) = b.insert(0, 5, "!");
        assert_eq!((l, c), (0, 6));
        assert_eq!(b.text(), "hello!\nworld");
        assert_eq!(b.delete_range(0, 5, 0, 6), "!");
    }
    #[test] fn utf16_emoji_col() {
        let mut b = TextBuffer::new();
        b.set_text("a\u{1F600}b");
        assert_eq!(b.offset_of(0, 1), 1);
        assert_eq!(b.offset_of(0, 3), 5);
        assert_eq!(b.line_col_of(6), (0, 4));
    }
    #[test] fn undo_redo() {
        let mut b = TextBuffer::new();
        b.set_text("abc");
        b.insert(0, 3, "d");
        assert_eq!(b.text(), "abcd");
        assert!(b.undo());
        assert_eq!(b.text(), "abc");
        assert!(b.redo());
        assert_eq!(b.text(), "abcd");
    }
}
