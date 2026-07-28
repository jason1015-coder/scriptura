
/// Represents a single diff line with its type
#[derive(Clone, Debug)]
pub struct DiffLine {
    pub text: String,
    pub line_type: LineType,
}

#[derive(Clone, Debug, PartialEq)]
pub enum LineType {
    Unchanged,
    Added,
    Removed,
    Modified,
}

/// Represents a hunk of changes between two texts
#[derive(Clone, Debug)]
pub struct DiffHunk {
    pub left_start: usize,
    pub left_count: usize,
    pub right_start: usize,
    pub right_count: usize,
    pub left_lines: Vec<DiffLine>,
    pub right_lines: Vec<DiffLine>,
}

/// Compute LCS-based diff between two texts
pub fn compute_diff(left: &str, right: &str) -> Vec<DiffHunk> {
    let left_lines: Vec<&str> = left.lines().collect();
    let right_lines: Vec<&str> = right.lines().collect();
    
    let left_len = left_lines.len();
    let right_len = right_lines.len();
    
    // Build LCS DP table
    let mut dp = vec![vec![0usize; right_len + 1]; left_len + 1];
    for i in 1..=left_len {
        for j in 1..=right_len {
            if left_lines[i - 1] == right_lines[j - 1] {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = dp[i - 1][j].max(dp[i][j - 1]);
            }
        }
    }
    
    // Trace back to find changes
    let mut changes: Vec<(Option<usize>, Option<usize>)> = Vec::new();
    let mut i = left_len;
    let mut j = right_len;
    
    while i > 0 || j > 0 {
        if i > 0 && j > 0 && left_lines[i - 1] == right_lines[j - 1] {
            i -= 1;
            j -= 1;
        } else if j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j]) {
            j -= 1;
            changes.push((None, Some(j)));
        } else {
            i -= 1;
            changes.push((Some(i), None));
        }
    }
    
    changes.reverse();
    
    // Group changes into hunks
    let mut hunks: Vec<DiffHunk> = Vec::new();
    let mut current_hunk: Option<DiffHunk> = None;
    
    for (left_idx, right_idx) in &changes {
        if left_idx.is_some() || right_idx.is_some() {
            let hunk = current_hunk.get_or_insert_with(|| DiffHunk {
                left_start: left_idx.unwrap_or(right_idx.unwrap_or(0)),
                left_count: 0,
                right_start: right_idx.unwrap_or(left_idx.unwrap_or(0)),
                right_count: 0,
                left_lines: Vec::new(),
                right_lines: Vec::new(),
            });
            
            if let Some(li) = left_idx {
                hunk.left_lines.push(DiffLine {
                    text: left_lines[*li].to_string(),
                    line_type: LineType::Removed,
                });
                hunk.left_count += 1;
            } else {
                hunk.left_lines.push(DiffLine {
                    text: String::new(),
                    line_type: LineType::Added,
                });
            }
            
            if let Some(ri) = right_idx {
                hunk.right_lines.push(DiffLine {
                    text: right_lines[*ri].to_string(),
                    line_type: LineType::Added,
                });
                hunk.right_count += 1;
            } else {
                hunk.right_lines.push(DiffLine {
                    text: String::new(),
                    line_type: LineType::Removed,
                });
            }
        } else {
            if let Some(hunk) = current_hunk.take() {
                hunks.push(hunk);
            }
        }
    }
    
    if let Some(hunk) = current_hunk.take() {
        hunks.push(hunk);
    }
    
    hunks
}

/// FFI-compatible diff result
#[repr(C)]
pub struct FfiDiffResult {
    pub hunks_ptr: *mut FfiDiffHunk,
    pub hunks_len: usize,
}

#[repr(C)]
pub struct FfiDiffHunk {
    pub left_start: i32,
    pub left_count: i32,
    pub right_start: i32,
    pub right_count: i32,
}

/// Compute diff and return JSON string for FFI
pub fn compute_diff_json(left: &str, right: &str) -> String {
    let hunks = compute_diff(left, right);
    let mut result = String::from("[");
    for (i, hunk) in hunks.iter().enumerate() {
        if i > 0 { result.push(','); }
        result.push_str(&format!(
            r#"{{"leftStart":{},"leftCount":{},"rightStart":{},"rightCount":{}}}"#,
            hunk.left_start, hunk.left_count, hunk.right_start, hunk.right_count
        ));
    }
    result.push(']');
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_identical_texts() {
        let hunks = compute_diff("hello\nworld", "hello\nworld");
        assert!(hunks.is_empty());
    }
    
    #[test]
    fn test_added_line() {
        let hunks = compute_diff("hello", "hello\nworld");
        assert_eq!(hunks.len(), 1);
        assert_eq!(hunks[0].right_lines.len(), 1);
    }
    
    #[test]
    fn test_removed_line() {
        let hunks = compute_diff("hello\nworld", "hello");
        assert_eq!(hunks.len(), 1);
        assert_eq!(hunks[0].left_lines.len(), 1);
    }
}
