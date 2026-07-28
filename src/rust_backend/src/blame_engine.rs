
/// Blame info for a single line
#[derive(Clone, Debug)]
pub struct BlameLineInfo {
    pub commit_hash: String,
    pub author: String,
    pub date: String,
    pub summary: String,
    pub line: usize,
}

/// Parse git blame porcelain output
pub fn parse_blame_output(output: &str) -> Vec<BlameLineInfo> {
    let mut result = Vec::new();
    let mut current_line = 0;
    let mut current_info = BlameLineInfo {
        commit_hash: String::new(),
        author: String::new(),
        date: String::new(),
        summary: String::new(),
        line: 0,
    };
    
    for line in output.lines() {
        if line.starts_with('\t') {
            // Actual line content - record blame info
            current_info.line = current_line;
            result.push(current_info.clone());
            current_line += 1;
            current_info = BlameLineInfo {
                commit_hash: String::new(),
                author: String::new(),
                date: String::new(),
                summary: String::new(),
                line: 0,
            };
            continue;
        }
        
        let parts: Vec<&str> = line.splitn(2, ' ').collect();
        if parts.is_empty() {
            continue;
        }
        
        // Header line: "<commit-hash> <original-line> <final-line> [<num-lines>]"
        if parts.len() >= 1 && parts[0].len() == 40 {
            current_info.commit_hash = parts[0].to_string();
            continue;
        }
        
        match parts[0] {
            "author" => {
                current_info.author = parts.get(1).unwrap_or(&"").trim().to_string();
            }
            "author-time" => {
                if let Some(time_str) = parts.get(1) {
                    if let Ok(timestamp) = time_str.trim().parse::<i64>() {
                        // Convert to human-readable date (simplified)
                        current_info.date = format_timestamp(timestamp);
                    }
                }
            }
            "summary" => {
                current_info.summary = parts.get(1).unwrap_or(&"").trim().to_string();
            }
            _ => {}
        }
    }
    
    result
}

/// Format Unix timestamp to human-readable date
fn format_timestamp(timestamp: i64) -> String {
    // Simplified timestamp formatting
    // In production, use chrono crate for proper timezone support
    let days = timestamp / 86400;
    let hours = (timestamp % 86400) / 3600;
    let minutes = (timestamp % 3600) / 60;
    
    if days > 365 {
        format!("{} years ago", days / 365)
    } else if days > 30 {
        format!("{} months ago", days / 30)
    } else if days > 0 {
        format!("{} days ago", days)
    } else if hours > 0 {
        format!("{} hours ago", hours)
    } else {
        format!("{} minutes ago", minutes)
    }
}

/// Parse blame output and return JSON string for FFI
pub fn parse_blame_json(output: &str) -> String {
    let lines = parse_blame_output(output);
    let mut result = String::from("[");
    for (i, info) in lines.iter().enumerate() {
        if i > 0 { result.push(','); }
        result.push_str(&format!(
            r#"{{"commitHash":"{}","author":"{}","date":"{}","summary":"{}","line":{}}}"#,
            escape_json(&info.commit_hash),
            escape_json(&info.author),
            escape_json(&info.date),
            escape_json(&info.summary),
            info.line
        ));
    }
    result.push(']');
    result
}

/// Escape string for JSON output
fn escape_json(s: &str) -> String {
    s.replace('\\', "\\\\")
     .replace('"', "\\\"")
     .replace('\n', "\\n")
     .replace('\r', "\\r")
     .replace('\t', "\\t")
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_parse_blame_output() {
        let output = "abc123def456789012345678901234567890abcd 1 1 1\nauthor John Doe\nauthor-time 1700000000\nsummary Initial commit\n\tHello, world!";
        let result = parse_blame_output(output);
        assert_eq!(result.len(), 1);
        assert_eq!(result[0].author, "John Doe");
    }
}
