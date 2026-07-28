/// Encoding detection and conversion engine for files
use std::fs;
use std::path::Path;

/// Detect BOM (Byte Order Mark) and return encoding name
pub fn detect_bom(data: &[u8]) -> Option<&'static str> {
    if data.len() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF {
        Some("UTF-8")
    } else if data.len() >= 2 && data[0] == 0xFF && data[1] == 0xFE {
        Some("UTF-16LE")
    } else if data.len() >= 2 && data[0] == 0xFE && data[1] == 0xFF {
        Some("UTF-16BE")
    } else {
        None
    }
}

/// Check if data is valid UTF-8
pub fn is_valid_utf8(data: &[u8]) -> bool {
    std::str::from_utf8(data).is_ok()
}

/// Detect encoding of file contents
pub fn detect_encoding(file_path: &str) -> String {
    let data = match fs::read(file_path) {
        Ok(d) => d,
        Err(_) => return "UTF-8".to_string(),
    };
    
    // Check BOM first
    if let Some(bom_enc) = detect_bom(&data) {
        return bom_enc.to_string();
    }
    
    // Peek at first 4096 bytes
    let peek = &data[..std::cmp::min(data.len(), 4096)];
    
    // Try UTF-8
    if is_valid_utf8(peek) {
        return "UTF-8".to_string();
    }
    
    // Default to Latin-1
    "ISO-8859-1".to_string()
}

/// Detect line ending style (CRLF, LF, CR)
pub fn detect_line_ending(file_path: &str) -> String {
    let data = match fs::read(file_path) {
        Ok(d) => d,
        Err(_) => return "LF".to_string(),
    };
    
    let crlf_count = data.windows(2).filter(|w| w == b"\r\n").count();
    let cr_count = data.iter().filter(|&&b| b == b'\r').count() - crlf_count;
    let lf_count = data.iter().filter(|&&b| b == b'\n').count() - crlf_count;
    
    if crlf_count > cr_count && crlf_count > lf_count {
        "CRLF".to_string()
    } else if cr_count > lf_count {
        "CR".to_string()
    } else {
        "LF".to_string()
    }
}

/// Check if file has BOM
pub fn has_bom(file_path: &str) -> bool {
    let data = match fs::read(file_path) {
        Ok(d) => d,
        Err(_) => return false,
    };
    
    (data.len() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) || 
    (data.len() >= 2 && data[0] == 0xFF && data[1] == 0xFE) || 
    (data.len() >= 2 && data[0] == 0xFE && data[1] == 0xFF)
}

/// Convert line endings in content
pub fn convert_line_endings(content: &str, from_style: &str, to_style: &str) -> String {
    // Normalize to LF first
    let normalized = match from_style {
        "CRLF" => content.replace("\r\n", "\n"),
        "CR" => content.replace('\r', "\n"),
        _ => content.to_string(),
    };
    
    // Convert to target style
    match to_style {
        "CRLF" => normalized.replace('\n', "\r\n"),
        "CR" => normalized.replace('\n', "\r"),
        _ => normalized,
    }
}

/// Read file with specific encoding (simplified - UTF-8 only for now)
pub fn read_file_with_encoding(file_path: &str, _encoding: &str) -> Result<String, String> {
    fs::read_to_string(file_path).map_err(|e| e.to_string())
}

/// Write file with specific encoding (simplified - UTF-8 only for now)
pub fn write_file_with_encoding(file_path: &str, content: &str, _encoding: &str) -> Result<(), String> {
    fs::write(file_path, content).map_err(|e| e.to_string())
}

/// Get list of supported encodings
pub fn supported_encodings() -> Vec<(&'static str, &'static str)> {
    vec![
        ("UTF-8", "Unicode (UTF-8)"),
        ("UTF-16LE", "Unicode (UTF-16 LE)"),
        ("UTF-16BE", "Unicode (UTF-16 BE)"),
        ("ISO-8859-1", "Western (ISO-8859-1)"),
        ("Windows-1252", "Western (Windows-1252)"),
        ("Shift-JIS", "Japanese (Shift-JIS)"),
        ("GB2312", "Chinese (GB2312)"),
        ("EUC-KR", "Korean (EUC-KR)"),
        ("ISO-8859-15", "Western European (ISO-8859-15)"),
        ("KOI8-R", "Russian (KOI8-R)"),
    ]
}

/// FFI-compatible encoding detection result
#[repr(C)]
pub struct EncodingResult {
    pub encoding: [u8; 32],  // Null-terminated encoding name
    pub line_ending: [u8; 8], // Null-terminated line ending
    pub has_bom: bool,
}

/// Detect encoding and line ending for a file (FFI entry point)
pub fn detect_file_info(file_path: &str) -> EncodingResult {
    let encoding = detect_encoding(file_path);
    let line_ending = detect_line_ending(file_path);
    let bom = has_bom(file_path);
    
    let mut result = EncodingResult {
        encoding: [0; 32],
        line_ending: [0; 8],
        has_bom: bom,
    };
    
    let enc_bytes = encoding.as_bytes();
    let copy_len = std::cmp::min(enc_bytes.len(), 31);
    result.encoding[..copy_len].copy_from_slice(&enc_bytes[..copy_len]);
    
    let le_bytes = line_ending.as_bytes();
    let copy_len = std::cmp::min(le_bytes.len(), 7);
    result.line_ending[..copy_len].copy_from_slice(&le_bytes[..copy_len]);
    
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    
    #[test]
    fn test_detect_encoding_utf8() {
        let dir = std::env::temp_dir().join("scriptura_test_encoding");
        let _ = fs::create_dir_all(&dir);
        let path = dir.join("test_utf8.txt");
        let mut f = fs::File::create(&path).unwrap();
        f.write_all(b"Hello, world!").unwrap();
        f.flush().unwrap();
        
        assert_eq!(detect_encoding(path.to_str().unwrap()), "UTF-8");
        assert_eq!(detect_line_ending(path.to_str().unwrap()), "LF");
        let _ = fs::remove_file(&path);
    }
}
