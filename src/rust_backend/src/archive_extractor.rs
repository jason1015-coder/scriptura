//! ## Archive Extractor
//!
//! Extracts ZIP archives for plugin installation.
//! Replaces `ArchiveExtractor` from the original C++ codebase.

use std::ffi::c_void;
use std::fs;
use std::path::Path;

type CbProgress = extern "C" fn(*const std::os::raw::c_char, i32, i32, *mut c_void);

pub struct ArchiveExtractor {
    on_progress: Option<CbProgress>,
    on_progress_data: *mut c_void,
}

impl ArchiveExtractor {
    pub fn new() -> Self {
        Self {
            on_progress: None,
            on_progress_data: std::ptr::null_mut(),
        }
    }

    /// Extract a ZIP archive from a byte buffer to a destination directory.
    pub fn extract(&self, data: &[u8], dest_dir: &str) -> Result<(), String> {
        let dest = Path::new(dest_dir);

        // Create destination directory if it doesn't exist
        fs::create_dir_all(dest)
            .map_err(|e| format!("Cannot create destination directory: {}", e))?;

        // Use cursor for in-memory extraction
        let cursor = std::io::Cursor::new(data);
        let mut archive = zip::ZipArchive::new(cursor)
            .map_err(|e| format!("Cannot open ZIP archive: {}", e))?;

        let total = archive.len() as i32;

        for i in 0..archive.len() {
            let mut file = archive.by_index(i)
                .map_err(|e| format!("Cannot read ZIP entry {}: {}", i, e))?;

            let out_path = dest.join(file.name());

            if file.name().ends_with('/') {
                // Directory entry
                fs::create_dir_all(&out_path)
                    .map_err(|e| format!("Cannot create directory '{}': {}", file.name(), e))?;
            } else {
                // File entry — ensure parent directory exists
                if let Some(parent) = out_path.parent() {
                    fs::create_dir_all(parent)
                        .map_err(|e| format!("Cannot create parent directory: {}", e))?;
                }

                let mut out_file = fs::File::create(&out_path)
                    .map_err(|e| format!("Cannot create file '{}': {}", file.name(), e))?;

                std::io::copy(&mut file, &mut out_file)
                    .map_err(|e| format!("Cannot write file '{}': {}", file.name(), e))?;
            }

            // Report progress — scoped CString, no leak
            if let Some(cb) = self.on_progress {
                let task_id = std::ffi::CString::new("extract").unwrap_or_default();
                cb(task_id.as_ptr(), (i + 1) as i32, total, self.on_progress_data);
            }
        }

        Ok(())
    }

    pub fn set_on_progress(&mut self, cb: CbProgress, data: *mut c_void) {
        self.on_progress = Some(cb);
        self.on_progress_data = data;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use tempfile::TempDir;

    /// Create a minimal in-memory ZIP archive containing a text file.
    fn create_test_zip(content: &str, filename: &str) -> Vec<u8> {
        let buf = std::io::Cursor::new(Vec::new());
        let mut zip = zip::ZipWriter::new(buf);
        let options = zip::write::FileOptions::<()>::default()
            .compression_method(zip::CompressionMethod::Stored);
        zip.start_file(filename, options).unwrap();
        zip.write_all(content.as_bytes()).unwrap();
        zip.finish().unwrap().into_inner()
    }

    #[test]
    fn test_new_extractor() {
        let ae = ArchiveExtractor::new();
        let data = create_test_zip("hello", "test.txt");
        // No-op: just verify construction
        assert!(!data.is_empty());
    }

    #[test]
    fn test_extract_single_file() {
        let ae = ArchiveExtractor::new();
        let zip_data = create_test_zip("Hello, World!", "greeting.txt");
        let tmp_dir = TempDir::new().unwrap();
        let dest = tmp_dir.path().to_str().unwrap();

        ae.extract(&zip_data, dest).unwrap();

        let extracted_path = tmp_dir.path().join("greeting.txt");
        assert!(extracted_path.exists());
        let content = std::fs::read_to_string(&extracted_path).unwrap();
        assert_eq!(content, "Hello, World!");
    }

    #[test]
    fn test_extract_with_directory_entry() {
        let ae = ArchiveExtractor::new();
        let buf = std::io::Cursor::new(Vec::new());
        let mut zip = zip::ZipWriter::new(buf);
        let options = zip::write::FileOptions::<()>::default()
            .compression_method(zip::CompressionMethod::Stored);

        // Add a directory entry
        zip.add_directory("subdir/", options).unwrap();
        // Add a file in that directory
        zip.start_file("subdir/file.txt", options).unwrap();
        zip.write_all(b"nested content").unwrap();

        let zip_data = zip.finish().unwrap().into_inner();
        let tmp_dir = TempDir::new().unwrap();
        let dest = tmp_dir.path().to_str().unwrap();

        ae.extract(&zip_data, dest).unwrap();
        assert!(tmp_dir.path().join("subdir/file.txt").exists());
    }

    #[test]
    fn test_extract_invalid_zip() {
        let ae = ArchiveExtractor::new();
        let tmp_dir = TempDir::new().unwrap();
        let dest = tmp_dir.path().to_str().unwrap();

        let result = ae.extract(b"not a zip file", dest);
        assert!(result.is_err());
        assert!(result.unwrap_err().contains("Cannot open ZIP archive"));
    }

    #[test]
    #[cfg(not(target_os = "windows"))]
    fn test_extract_to_nonwritable_path() {
        let ae = ArchiveExtractor::new();
        let zip_data = create_test_zip("data", "file.txt");
        // An absolute path under /nonexistent should fail on Unix systems
        let result = ae.extract(&zip_data, "/nonexistent_scriptura_test_dir");
        assert!(result.is_err());
    }

    #[test]
    fn test_progress_callback() {
        use std::os::raw::c_char;
        use std::sync::atomic::{AtomicBool, Ordering};

        extern "C" fn progress_cb(
            _task_id: *const c_char, _current: i32, _total: i32, user_data: *mut c_void,
        ) {
            unsafe {
                let flag = &*(user_data as *const AtomicBool);
                flag.store(true, Ordering::SeqCst);
            }
        }

        let mut ae = ArchiveExtractor::new();
        let flag = AtomicBool::new(false);
        let ptr = &flag as *const AtomicBool as *mut c_void;
        ae.set_on_progress(progress_cb, ptr);

        let zip_data = create_test_zip("test", "file.txt");
        let tmp_dir = TempDir::new().unwrap();
        ae.extract(&zip_data, tmp_dir.path().to_str().unwrap()).unwrap();

        assert!(flag.load(Ordering::SeqCst));
    }
}
