//! ## Bookmark Engine — store + navigate (from `bookmarkmanager.cpp`).
//! Qt only renders gutter icons + emits signals.

use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Bookmark {
    pub id: i32,
    pub file: String,
    pub line: u32,
    pub text: String,
}

/// Legacy Qt wire shape: {filePath,line,text,id}.
#[derive(Clone, Debug, Serialize, Deserialize)]
struct QtBookmark {
    #[serde(rename = "filePath", alias = "file")]
    file_path: String,
    line: u32,
    #[serde(default)]
    text: String,
    id: i32,
}

pub struct BookmarkStore {
    items: Vec<Bookmark>,
    next_id: i32,
}

impl BookmarkStore {
    pub fn new() -> Self { Self { items: vec![], next_id: 1 } }
    /// Toggle. Returns Some(id) when added, None when removed.
    pub fn toggle(&mut self, file: &str, line: u32, text: &str) -> Option<i32> {
        if let Some(p) = self.items.iter()
            .position(|b| b.file == file && b.line == line) {
            self.items.remove(p);
            return None;
        }
        let id = self.next_id;
        self.next_id += 1;
        self.items.push(Bookmark { id,
            file: file.into(), line, text: text.into() });
        Some(id)
    }
    pub fn remove(&mut self, id: i32) -> Option<Bookmark> {
        self.items.iter().position(|b| b.id == id)
            .map(|p| self.items.remove(p))
    }
    pub fn clear(&mut self) { self.items.clear(); }
    pub fn clear_file(&mut self, file: &str) -> Vec<Bookmark> {
        let mut removed = vec![];
        let mut i = 0;
        while i < self.items.len() {
            if self.items[i].file == file {
                removed.push(self.items.remove(i));
            } else { i += 1; }
        }
        removed
    }
    pub fn get(&self, id: i32) -> Option<Bookmark> {
        self.items.iter().find(|b| b.id == id).cloned()
    }
    pub fn at(&self, file: &str, line: u32) -> Option<i32> {
        self.items.iter()
            .find(|b| b.file == file && b.line == line).map(|b| b.id)
    }
    pub fn is_bookmarked(&self, file: &str, line: u32) -> bool {
        self.at(file, line).is_some()
    }
    pub fn for_file(&self, file: &str) -> Vec<Bookmark> {
        self.items.iter().filter(|b| b.file == file).cloned().collect()
    }
    pub fn all(&self) -> Vec<Bookmark> { self.items.clone() }
    pub fn count(&self) -> usize { self.items.len() }
    /// Next after current_line in file. Matches findNextBookmarkIndex:
    /// same-file forward, then wrap in file, then any bookmark.
    pub fn next_after(&self, file: &str, current_line: i64) -> Option<Bookmark> {
        if self.items.is_empty() { return None; }
        if let Some(b) = self.items.iter().find(|b|
            b.file == file && b.line as i64 > current_line) {
            return Some(b.clone());
        }
        if let Some(b) = self.items.iter().find(|b| b.file == file) {
            return Some(b.clone());
        }
        self.items.first().cloned()
    }
    /// Previous before current_line. Matches findPreviousBookmarkIndex.
    pub fn prev_before(&self, file: &str, current_line: i64) -> Option<Bookmark> {
        if self.items.is_empty() { return None; }
        let mut result: Option<&Bookmark> = None;
        for b in &self.items {
            if b.file == file && (b.line as i64) < current_line {
                result = Some(b);
            }
        }
        if let Some(b) = result { return Some(b.clone()); }
        for b in self.items.iter().rev() {
            if b.file == file { return Some(b.clone()); }
        }
        self.items.last().cloned()
    }
    /// Legacy Qt shape [{filePath,line,text,id}] for QSettings round-trip.
    pub fn to_qt_json(&self) -> String {
        let arr: Vec<QtBookmark> = self.items.iter().map(|b| QtBookmark {
            file_path: b.file.clone(), line: b.line,
            text: b.text.clone(), id: b.id,
        }).collect();
        serde_json::to_string(&arr).unwrap_or("[]".into())
    }
    pub fn load_qt_json(&mut self, json: &str) {
        let arr: Vec<QtBookmark> = serde_json::from_str(json).unwrap_or_default();
        self.items.clear();
        self.next_id = 1;
        for q in arr {
            if q.id >= self.next_id { self.next_id = q.id + 1; }
            self.items.push(Bookmark { id: q.id, file: q.file_path,
                line: q.line, text: q.text });
        }
    }
    pub fn to_json(&self) -> String {
        serde_json::to_string(&self.items).unwrap_or("[]".into())
    }
}

impl Default for BookmarkStore { fn default() -> Self { Self::new() } }

#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn toggle_roundtrip() {
        let mut s = BookmarkStore::new();
        assert!(s.toggle("f", 3, "x").is_some());
        assert!(s.toggle("f", 3, "x").is_none());
    }
    #[test] fn navigation_matches_cpp() {
        let mut s = BookmarkStore::new();
        s.toggle("a", 0, "first");
        s.toggle("a", 10, "second");
        s.toggle("b", 5, "other");
        assert_eq!(s.next_after("a", -1).unwrap().line, 0);
        assert_eq!(s.next_after("a", 10).unwrap().line, 0);
        assert_eq!(s.prev_before("a", 10).unwrap().line, 0);
        assert_eq!(s.prev_before("a", 0).unwrap().line, 10);
        assert!(s.next_after("zzz", -1).is_some());
    }
    #[test] fn qt_json_roundtrip() {
        let mut s = BookmarkStore::new();
        s.toggle("/tmp/a.cpp", 7, "line");
        let j = s.to_qt_json();
        assert!(j.contains("filePath"));
        let mut s2 = BookmarkStore::new();
        s2.load_qt_json(&j);
        assert_eq!(s2.count(), 1);
        assert!(s2.is_bookmarked("/tmp/a.cpp", 7));
        assert!(s2.toggle("/tmp/a.cpp", 8, "x").unwrap() > 1);
    }
}
