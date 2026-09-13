//! ## Search Engine — find/replace + fuzzy (from findreplace/universalsearch).
//! Qt keeps QLineEdit/QListWidget only.

/// Case-insensitive fuzzy score matching `UniversalSearchPopup::fuzzyScore`
/// (higher is better, 0 = no match): +3 for a consecutive run, +1 per gap.
pub fn fuzzy_score(pattern: &str, text: &str) -> u32 {
    if pattern.is_empty() { return 0; }
    let p: Vec<char> = pattern.to_lowercase().chars().collect();
    let t: Vec<char> = text.to_lowercase().chars().collect();
    let mut pi = 0usize;
    let mut score = 0u32;
    let mut last: i64 = -1;
    for (ti, &c) in t.iter().enumerate() {
        if pi < p.len() && c == p[pi] {
            score += if last == (ti as i64 - 1) { 3 } else { 1 };
            last = ti as i64;
            pi += 1;
        }
    }
    if pi == p.len() { score } else { 0 }
}

/// All non-overlapping matches of needle in text (char offsets).
pub fn find_all(text: &str, needle: &str, case_sensitive: bool) -> Vec<(usize, usize)> {
    if needle.is_empty() { return vec![]; }
    let (hay, ndl) = if case_sensitive {
        (text.to_string(), needle.to_string())
    } else { (text.to_lowercase(), needle.to_lowercase()) };
    let h: Vec<char> = hay.chars().collect();
    let n: Vec<char> = ndl.chars().collect();
    let mut out = vec![];
    let mut i = 0;
    while i + n.len() <= h.len() {
        if h[i..i + n.len()] == n[..] {
            out.push((i, i + n.len()));
            i += n.len().max(1);
        } else { i += 1; }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn fuzzy() {
        // Ported C++ semantics: consecutive=3, gap=1, 0 on no-match/empty.
        assert!(fuzzy_score("fb", "fooBar") > 0);
        assert_eq!(fuzzy_score("zx", "fooBar"), 0);
        assert_eq!(fuzzy_score("", "fooBar"), 0);
        // "fb" in "fooBar": f=3 (first char), b after gap +1 => 4
        assert_eq!(fuzzy_score("fb", "fooBar"), 4);
        // consecutive "ab" in "ab" = 3+3 = 6
        assert_eq!(fuzzy_score("ab", "ab"), 6);
        assert_eq!(fuzzy_score("fb", "FBX"), 3 + 3); // case-insensitive consecutive
        assert_eq!(find_all("aaa", "aa", true), vec![(0, 2)]);
    }
}
