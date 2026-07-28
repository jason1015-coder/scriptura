use std::fs;
use std::path::PathBuf;

/// Session data for a single editor tab
#[derive(Clone, Debug, serde::Serialize, serde::Deserialize)]
pub struct TabSession {
    pub file_path: String,
    pub cursor_line: usize,
    pub cursor_column: usize,
    pub modified: bool,
    pub content: Option<String>, // For hot exit of unsaved files
}

/// Full editor session
#[derive(Clone, Debug, serde::Serialize, serde::Deserialize)]
pub struct EditorSession {
    pub version: u32,
    pub tabs: Vec<TabSession>,
    pub active_tab: usize,
    pub geometry: Option<String>,
    pub window_state: Option<String>,
}

impl EditorSession {
    pub fn new() -> Self {
        Self {
            version: 1,
            tabs: Vec::new(),
            active_tab: 0,
            geometry: None,
            window_state: None,
        }
    }
}

/// Get the session file path
pub fn session_file_path() -> PathBuf {
    let data_dir = dirs_data_dir();
    fs::create_dir_all(&data_dir).ok();
    data_dir.join("session.json")
}

/// Get the hot exit directory
pub fn hot_exit_dir() -> PathBuf {
    let data_dir = dirs_data_dir();
    let dir = data_dir.join("hotexit");
    fs::create_dir_all(&dir).ok();
    dir
}

/// Get app data directory
fn dirs_data_dir() -> PathBuf {
    // Use XDG_DATA_HOME on Linux, or fallback to ~/.local/share
    if let Ok(home) = std::env::var("HOME") {
        PathBuf::from(home).join(".local").join("share").join("scriptura")
    } else {
        PathBuf::from(".").join(".scriptura")
    }
}

/// Check if a saved session exists
pub fn has_saved_session() -> bool {
    session_file_path().exists()
}

/// Save session to JSON file
pub fn save_session(session: &EditorSession) -> Result<(), String> {
    let json = serde_json::to_string_pretty(session)
        .map_err(|e| e.to_string())?;
    fs::write(session_file_path(), json).map_err(|e| e.to_string())
}

/// Load session from JSON file
pub fn load_session() -> Result<EditorSession, String> {
    let data = fs::read_to_string(session_file_path())
        .map_err(|e| e.to_string())?;
    serde_json::from_str(&data).map_err(|e| e.to_string())
}

/// Save unsaved buffers for hot exit
pub fn save_hot_exit(tabs: &[TabSession]) -> Result<(), String> {
    let dir = hot_exit_dir();
    // Clean old hot exit files
    let _ = fs::remove_dir_all(&dir);
    fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
    
    let mut index = Vec::new();
    for (i, tab) in tabs.iter().enumerate() {
        if tab.modified || tab.file_path.is_empty() {
            let identifier = if tab.file_path.is_empty() {
                format!("untitled_{}", i)
            } else {
                format!("{:x}", md5_hash(&tab.file_path))
            };
            
            let hot_file = dir.join(format!("{}.json", identifier));
            let hot_data = serde_json::to_string_pretty(tab)
                .map_err(|e| e.to_string())?;
            fs::write(&hot_file, hot_data).map_err(|e| e.to_string())?;
            index.push(tab.clone());
        }
    }
    
    // Save index
    let index_data = serde_json::to_vec(&index).map_err(|e| e.to_string())?;
    fs::write(dir.join("index.json"), index_data).map_err(|e| e.to_string())?;
    Ok(())
}

/// Restore hot exit buffers
pub fn load_hot_exit() -> Result<Vec<TabSession>, String> {
    let index_file = hot_exit_dir().join("index.json");
    if !index_file.exists() {
        return Ok(Vec::new());
    }
    
    let data = fs::read_to_string(&index_file).map_err(|e| e.to_string())?;
    let tabs: Vec<TabSession> = serde_json::from_str(&data).map_err(|e| e.to_string())?;
    Ok(tabs)
}

/// Clear saved session
pub fn clear_session() -> Result<(), String> {
    let _ = fs::remove_file(session_file_path());
    let dir = hot_exit_dir();
    let _ = fs::remove_dir_all(&dir);
    Ok(())
}

/// Simple hash function for file paths
fn md5_hash(input: &str) -> u64 {
    let mut hash: u64 = 5381;
    for byte in input.bytes() {
        hash = hash.wrapping_mul(33).wrapping_add(byte as u64);
    }
    hash
}

/// FFI-compatible session data
#[repr(C)]
pub struct FfiSession {
    pub tab_count: i32,
    pub active_tab: i32,
    pub has_session: bool,
}

/// Check if session exists and return basic info (FFI entry)
pub fn session_info() -> FfiSession {
    let session = load_session().ok();
    FfiSession {
        tab_count: session.as_ref().map_or(0, |s| s.tabs.len() as i32),
        active_tab: session.as_ref().map_or(0, |s| s.active_tab as i32),
        has_session: session.is_some(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_session_roundtrip() {
        let session = EditorSession::new();
        // Would need temp dir for full test
        assert_eq!(session.version, 1);
        assert!(session.tabs.is_empty());
    }
}
