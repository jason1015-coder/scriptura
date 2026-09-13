//! ## Application Crash Handler
//!
//! Replaces `CrashHandler` from the original C++ codebase.
//! Handles application-level crash detection and dump path management.

use std::path::{Path, PathBuf};

pub struct AppCrashHandler {
    dump_path: PathBuf,
}

impl AppCrashHandler {
    pub fn new() -> Self {
        let dump_path = Self::compute_dump_path();
        Self { dump_path }
    }

    pub fn install(&self) {
        let path = self.dump_path.clone();
        std::panic::set_hook(Box::new(move |info| {
            let msg = info.to_string();
            if let Some(parent) = path.parent() {
                let _ = std::fs::create_dir_all(parent);
            }
            let _ = std::fs::write(&path, format!("Scriptura Crash Report\n{}\n", msg));
        }));
    }

    pub fn dump_path(&self) -> &Path {
        &self.dump_path
    }

    fn compute_dump_path() -> PathBuf {
        let temp_dir = std::env::temp_dir();
        temp_dir.join("scriptura-crash.log")
    }
}
