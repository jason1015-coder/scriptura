//! ## Bracket Engine — pure match/depth (from `bracketcolorizer.cpp`).
//! Qt only applies `ExtraSelection` colors.

use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct BracketPair { pub open: usize, pub close: usize, pub depth: u32 }

/// Find bracket pairs in char-offset space. String/comment aware (basic).
pub fn find_pairs(text: &str) -> Vec<BracketPair> {
    let chars: Vec<char> = text.chars().collect();
    let mut stack: Vec<(usize, char)> = vec![];
    let mut out = vec![];
    let mut in_s = false; let mut in_d = false; let mut in_line = false;
    let mut i = 0;
    while i < chars.len() {
        let c = chars[i];
        if in_line { if c == '\n' { in_line = false; } i += 1; continue; }
        if c == '\\' && (in_s || in_d) { i += 2; continue; }
        if c == '/' && !in_s && !in_d && i + 1 < chars.len() && chars[i + 1] == '/' { in_line = true; i += 2; continue; }
        if c == '\'' && !in_d { in_s = !in_s; i += 1; continue; }
        if c == '"' && !in_s { in_d = !in_d; i += 1; continue; }
        if in_s || in_d { i += 1; continue; }
        match c {
            '(' | '[' | '{' => stack.push((i, c)),
            ')' | ']' | '}' => {
                let want = match c { ')' => '(', ']' => '[', _ => '{' };
                if let Some(&(pos, o)) = stack.last() {
                    if o == want {
                        stack.pop();
                        out.push(BracketPair { open: pos, close: i, depth: stack.len() as u32 });
                    }
                }
            }
            _ => {}
        }
        i += 1;
    }
    out.sort_by_key(|p| p.open);
    out
}

/// Depth at char offset.
pub fn depth_at(pairs: &[BracketPair], offset: usize) -> u32 {
    pairs.iter().filter(|p| p.open <= offset && offset <= p.close).count() as u32
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn nested() {
        let p = find_pairs("(a[{}])");
        assert_eq!(p.len(), 3);
        assert_eq!(depth_at(&p, 3), 3);
    }
}
