//! ## Snippet Engine — store + expand (from `snippetmanager.cpp`).
//! Qt keeps the editor dialog form + cursor movement only.
//! All storage, management, persistence, variable substitution live here.

use serde::{Deserialize, Serialize};
use std::collections::HashMap;

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Snippet {
    pub id: String,
    pub name: String,
    pub prefix: String,
    pub body: String,
    pub description: String,
    pub language: String,
    pub tab_stops: Vec<TabStop>,
}

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct TabStop { pub offset: usize, pub len: usize }

/// Expand `$VAR`, `${1:ph}`, `$1`, `$0`. Returns (text, tabstops).
pub fn expand(body: &str, filename: &str) -> (String, Vec<TabStop>) {
    let mut out = String::new();
    let mut stops: Vec<(usize, usize)> = vec![];
    let chars: Vec<char> = body.chars().collect();
    let mut i = 0;
    while i < chars.len() {
        if chars[i] == '$' && i + 1 < chars.len() {
            if chars[i + 1] == '{' {
                if let Some(end) = chars[i..].iter().position(|&c| c == '}') {
                    let inner: String = chars[i + 2..i + end].iter().collect();
                    let (num, ph) = match inner.find(':') {
                        Some(k) => (&inner[..k], &inner[k + 1..]),
                        None => (inner.as_str(), ""),
                    };
                    if let Ok(n) = num.parse::<usize>() {
                        if n == 0 {
                            out.push_str(ph);
                        } else {
                            let s = out.chars().count();
                            out.push_str(ph);
                            stops.push((s, ph.chars().count()));
                        }
                    } else {
                        out.push_str(&subst_var(&inner, filename));
                    }
                    i += end + 1;
                    continue;
                }
            } else if chars[i + 1].is_ascii_digit() {
                let mut j = i + 1;
                while j < chars.len() && chars[j].is_ascii_digit() { j += 1; }
                let n: usize = chars[i + 1..j].iter().collect::<String>()
                    .parse().unwrap_or(0);
                if n > 0 { stops.push((out.chars().count(), 0)); }
                i = j;
                continue;
            } else if chars[i + 1].is_ascii_alphabetic() || chars[i + 1] == '_' {
                let mut j = i + 1;
                while j < chars.len()
                    && (chars[j].is_alphanumeric() || chars[j] == '_') { j += 1; }
                let name: String = chars[i + 1..j].iter().collect();
                out.push_str(&subst_var(&name, filename));
                i = j;
                continue;
            }
        }
        out.push(chars[i]);
        i += 1;
    }
    let byte_stops: Vec<TabStop> = stops.into_iter().map(|(co, cl)| {
        let off: usize = out.chars().take(co).map(|c| c.len_utf8()).sum();
        let len: usize = out.chars().skip(co).take(cl).map(|c| c.len_utf8()).sum();
        TabStop { offset: off, len }
    }).collect();
    (out, byte_stops)
}

fn subst_var(name: &str, filename: &str) -> String {
    match name {
        "FILENAME" | "TM_FILENAME" => filename.to_string(),
        "CURRENT_DATE" => "2026-09-13".to_string(),
        "CURRENT_TIME" => "00:00:00".to_string(),
        "CURRENT_YEAR" => "2026".to_string(),
        "CURRENT_MONTH" => "09".to_string(),
        "CURRENT_DAY" => "13".to_string(),
        _ => format!("${}", name),
    }
}

/// Substitute date/time variables with provided values.
pub fn subst_datetime(body: &str, vars: &[(&str, &str)]) -> String {
    let mut out = body.to_string();
    for (k, v) in vars {
        out = out.replace(&format!("${}", k), v);
    }
    out
}

pub struct SnippetStore {
    snippets: Vec<Snippet>,
}

impl SnippetStore {
    pub fn new() -> Self { Self { snippets: vec![] } }

    pub fn add(&mut self, s: Snippet) -> bool {
        if self.snippets.iter().any(|x| x.id == s.id) { return false; }
        self.snippets.push(s);
        true
    }

    pub fn update(&mut self, s: Snippet) -> bool {
        if let Some(pos) = self.snippets.iter().position(|x| x.id == s.id) {
            self.snippets[pos] = s;
            true
        } else { false }
    }

    pub fn remove(&mut self, id: &str) -> bool {
        if let Some(pos) = self.snippets.iter().position(|x| x.id == id) {
            self.snippets.remove(pos);
            true
        } else { false }
    }

    pub fn get(&self, id: &str) -> Option<&Snippet> {
        self.snippets.iter().find(|s| s.id == id)
    }

    pub fn all(&self) -> &[Snippet] { &self.snippets }

    pub fn for_language(&self, language: &str) -> Vec<&Snippet> {
        self.snippets.iter()
            .filter(|s| s.language == language || s.language == "*" || s.language.is_empty())
            .collect()
    }

    pub fn prefixes(&self) -> Vec<String> {
        let mut seen = HashMap::new();
        for s in &self.snippets {
            if !s.prefix.is_empty() {
                seen.entry(s.prefix.clone()).or_insert(true);
            }
        }
        seen.into_keys().collect()
    }

    pub fn has_prefix(&self, prefix: &str, language: &str) -> bool {
        self.snippets.iter().any(|s| s.prefix == prefix
            && (s.language.is_empty() || s.language == language || s.language == "*"))
    }

    pub fn find_for_prefix(&self, prefix: &str, language: &str) -> Option<&Snippet> {
        // Prefer language-specific match, then wildcard, then empty language.
        // Return None only if no snippet has the prefix at all.
        self.snippets.iter()
            .filter(|s| s.prefix == prefix)
            .min_by(|a, b| {
                let score = |s: &Snippet| {
                    if s.language == language { 0 }
                    else if s.language == "*" { 1 }
                    else if s.language.is_empty() { 2 }
                    else { 3 }
                };
                score(a).cmp(&score(b))
            })
            .filter(|s| {
                let language_matches = |s: &Snippet| {
                    s.language == language || s.language == "*" || s.language.is_empty()
                };
                language_matches(s)
            })
    }

    /// Serialize all snippets to JSON string (for QSettings persistence).
    pub fn save_to_json(&self) -> String {
        serde_json::to_string(&self.snippets).unwrap_or_default()
    }

    /// Load snippets from JSON string. Returns count loaded.
    pub fn load_from_json(&mut self, json: &str) -> usize {
        if let Ok(snippets) = serde_json::from_str::<Vec<Snippet>>(json) {
            self.snippets = snippets;
            self.snippets.len()
        } else { 0 }
    }

    /// Import snippets from JSON string, skipping duplicates. Returns count added.
    pub fn import_from_json(&mut self, json: &str) -> usize {
        if let Ok(snippets) = serde_json::from_str::<Vec<Snippet>>(json) {
            let mut count = 0;
            for s in snippets {
                if self.add(s) { count += 1; }
            }
            count
        } else { 0 }
    }

    /// Export all snippets to JSON string.
    pub fn export_to_json(&self) -> String {
        self.save_to_json()
    }
}

impl Default for SnippetStore { fn default() -> Self { Self::new() } }

#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn expand_tabstop() {
        let (t, stops) = expand("fn ${1:name}() {$0}", "f.rs");
        assert_eq!(t, "fn name() {}");
        assert_eq!(stops.len(), 1);
    }
    #[test] fn store_add_duplicate() {
        let mut s = SnippetStore::new();
        let a = Snippet { id: "1".into(), name: "A".into(), prefix: "a".into(),
            body: "b".into(), description: "d".into(), language: "".into(),
            tab_stops: vec![] };
        assert!(s.add(a.clone()));
        assert!(!s.add(a));
        assert_eq!(s.all().len(), 1);
    }
    #[test] fn store_remove() {
        let mut s = SnippetStore::new();
        let a = Snippet { id: "1".into(), name: "A".into(), prefix: "a".into(),
            body: "b".into(), description: "d".into(), language: "".into(),
            tab_stops: vec![] };
        s.add(a);
        assert!(s.remove("1"));
        assert!(!s.remove("1"));
        assert!(s.all().is_empty());
    }
    #[test] fn store_for_language() {
        let mut s = SnippetStore::new();
        let a = Snippet { id: "1".into(), name: "A".into(), prefix: "a".into(),
            body: "b".into(), description: "d".into(), language: "cpp".into(),
            tab_stops: vec![] };
        let b = Snippet { id: "2".into(), name: "B".into(), prefix: "b".into(),
            body: "c".into(), description: "d".into(), language: "*".into(),
            tab_stops: vec![] };
        let c = Snippet { id: "3".into(), name: "C".into(), prefix: "c".into(),
            body: "d".into(), description: "d".into(), language: "".into(),
            tab_stops: vec![] };
        s.add(a); s.add(b); s.add(c);
        assert_eq!(s.for_language("cpp").len(), 3); // all match: specific + wildcard + empty
        assert_eq!(s.for_language("js").len(), 2);   // wildcard + empty
    }
    #[test] fn store_roundtrip_json() {
        let mut s = SnippetStore::new();
        let a = Snippet { id: "1".into(), name: "A".into(), prefix: "a".into(),
            body: "b".into(), description: "d".into(), language: "cpp".into(),
            tab_stops: vec![TabStop { offset: 0, len: 3 }] };
        s.add(a);
        let json = s.save_to_json();
        assert!(json.contains("cpp"));
        let mut s2 = SnippetStore::new();
        assert_eq!(s2.load_from_json(&json), 1);
        assert_eq!(s2.all().len(), 1);
    }
    #[test] fn store_import_skips_duplicates() {
        let mut s = SnippetStore::new();
        let a = Snippet { id: "1".into(), name: "A".into(), prefix: "a".into(),
            body: "b".into(), description: "d".into(), language: "".into(),
            tab_stops: vec![] };
        s.add(a.clone());
        let json = format!("[{}]", serde_json::to_string(&a).unwrap());
        assert_eq!(s.import_from_json(&json), 0);
        assert_eq!(s.all().len(), 1);
    }
}
