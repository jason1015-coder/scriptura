//! ## Language Registry
//!
//! Manages programming language definitions including:
//! - Language ID to metadata mapping
//! - File extension to language ID detection
//! - Language server configuration per language
//!
//! Replaces `LanguageRegistry` from the original C++ codebase.

use std::collections::HashMap;

#[derive(Debug, Clone)]
pub struct LanguageEntry {
    pub id: String,
    pub name: String,
    pub extensions: Vec<String>,
    pub server_command: Option<String>,
    pub server_args: Vec<String>,
}

pub struct LanguageRegistry {
    languages: HashMap<String, LanguageEntry>, // lang_id -> entry
    extension_map: HashMap<String, String>,    // extension -> lang_id
}

impl LanguageRegistry {
    pub fn new() -> Self {
        let mut reg = Self {
            languages: HashMap::new(),
            extension_map: HashMap::new(),
        };
        reg.register_builtin();
        reg
    }

    fn register_builtin(&mut self) {
        let builtins = vec![
            ("c", "C", ".c,.h"),
            ("cpp", "C++", ".cpp,.hpp,.cc,.cxx,.h,.hh,.hxx,.hpp,.ixx,.txx"),
            ("rust", "Rust", ".rs"),
            ("python", "Python", ".py,.pyw"),
            ("javascript", "JavaScript", ".js,.jsx,.mjs"),
            ("typescript", "TypeScript", ".ts,.tsx,.mts,.cts"),
            ("go", "Go", ".go"),
            ("java", "Java", ".java,.class"),
            ("kotlin", "Kotlin", ".kt,.kts"),
            ("swift", "Swift", ".swift"),
            ("ruby", "Ruby", ".rb,.erb"),
            ("php", "PHP", ".php,.phtml"),
            ("html", "HTML", ".html,.htm"),
            ("css", "CSS", ".css,.scss,.less"),
            ("sql", "SQL", ".sql"),
            ("json", "JSON", ".json"),
            ("yaml", "YAML", ".yaml,.yml"),
            ("markdown", "Markdown", ".md,.markdown"),
            ("xml", "XML", ".xml,.xsd,.xslt"),
            ("cmake", "CMake", "CMakeLists.txt,.cmake"),
        ];

        for (id, name, exts) in builtins {
            let extensions: Vec<String> = exts.split(',').map(|s| s.trim().to_string()).collect();
            for ext in &extensions {
                self.extension_map.insert(ext.to_lowercase(), id.to_string());
            }
            self.languages.insert(id.to_string(), LanguageEntry {
                id: id.to_string(),
                name: name.to_string(),
                extensions,
                server_command: None,
                server_args: Vec::new(),
            });
        }
    }

    /// Register a new language or update an existing one.
    pub fn register(&mut self, lang_id: &str, name: &str, extensions: &str,
                     server_command: Option<&str>, server_args: &[&str]) {
        let ext_list: Vec<String> = extensions.split(',')
            .map(|s| s.trim().to_string())
            .collect();

        for ext in &ext_list {
            self.extension_map.insert(ext.to_lowercase(), lang_id.to_string());
        }

        self.languages.insert(lang_id.to_string(), LanguageEntry {
            id: lang_id.to_string(),
            name: name.to_string(),
            extensions: ext_list,
            server_command: server_command.map(|s| s.to_string()),
            server_args: server_args.iter().map(|s| s.to_string()).collect(),
        });
    }

    /// Unregister a language.
    pub fn unregister(&mut self, lang_id: &str) {
        if let Some(entry) = self.languages.remove(lang_id) {
            for ext in entry.extensions {
                self.extension_map.remove(&ext.to_lowercase());
            }
        }
    }

    /// Get language info as JSON string.
    pub fn get(&self, lang_id: &str) -> Option<String> {
        self.languages.get(lang_id).map(|entry| {
            serde_json::to_string(&json!({
                "id": entry.id,
                "name": entry.name,
                "extensions": entry.extensions,
                "serverCommand": entry.server_command,
                "serverArgs": entry.server_args
            })).unwrap_or_default()
        })
    }

    /// Detect language from filename (returns language ID).
    pub fn detect(&self, filename: &str) -> Option<&str> {
        let fname = filename.to_lowercase();

        // Try full filename first (for CMakeLists.txt, Makefile, etc.)
        if let Some(lang_id) = self.extension_map.get(&fname) {
            return Some(lang_id.as_str());
        }

        // Try extension
        if let Some(dot) = fname.rfind('.') {
            let ext = &fname[dot..];
            if let Some(lang_id) = self.extension_map.get(ext) {
                return Some(lang_id.as_str());
            }
        }

        None
    }

    /// List all registered language IDs.
    pub fn languages(&self) -> Vec<String> {
        self.languages.keys().cloned().collect()
    }
}

// Helper macro for serde_json in tests
use serde_json::json;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new_registry_has_builtins() {
        let reg = LanguageRegistry::new();
        assert!(reg.languages().len() >= 20);
        assert!(reg.get("rust").is_some());
        assert!(reg.get("python").is_some());
        assert!(reg.get("javascript").is_some());
    }

    #[test]
    fn test_detect_by_extension() {
        let reg = LanguageRegistry::new();
        assert_eq!(reg.detect("main.rs"), Some("rust"));
        assert_eq!(reg.detect("main.py"), Some("python"));
        assert_eq!(reg.detect("index.js"), Some("javascript"));
        assert_eq!(reg.detect("index.tsx"), Some("typescript"));
    }

    #[test]
    fn test_detect_by_full_filename() {
        let reg = LanguageRegistry::new();
        assert_eq!(reg.detect("CMakeLists.txt"), Some("cmake"));
    }

    #[test]
    fn test_detect_unknown_extension() {
        let reg = LanguageRegistry::new();
        assert_eq!(reg.detect("file.xyz"), None);
    }

    #[test]
    fn test_detect_no_extension() {
        let reg = LanguageRegistry::new();
        assert_eq!(reg.detect("Makefile"), None);
    }

    #[test]
    fn test_detect_case_insensitive() {
        let reg = LanguageRegistry::new();
        assert_eq!(reg.detect("Main.RS"), Some("rust"));
        assert_eq!(reg.detect("Main.PY"), Some("python"));
    }

    #[test]
    fn test_get_rust_info() {
        let reg = LanguageRegistry::new();
        let info = reg.get("rust").unwrap();
        assert!(info.contains("Rust"));
        assert!(info.contains(".rs"));
    }

    #[test]
    fn test_get_nonexistent() {
        let reg = LanguageRegistry::new();
        assert!(reg.get("nonexistent").is_none());
    }

    #[test]
    fn test_register_new_language() {
        let mut reg = LanguageRegistry::new();
        reg.register("my_lang", "My Language", ".my,.myl",
                     Some("my-lsp"), &["--flag", "value"]);
        let info = reg.get("my_lang").unwrap();
        assert!(info.contains("My Language"));
        assert!(info.contains("my-lsp"));
        assert!(info.contains("--flag"));
    }

    #[test]
    fn test_unregister_language() {
        let mut reg = LanguageRegistry::new();
        reg.register("custom", "Custom", ".cust", None, &[]);
        assert!(reg.get("custom").is_some());
        reg.unregister("custom");
        assert!(reg.get("custom").is_none());
        // Extension mapping should also be removed
        assert_eq!(reg.detect("file.cust"), None);
    }

    #[test]
    fn test_unregister_builtin() {
        let mut reg = LanguageRegistry::new();
        reg.unregister("rust");
        assert!(reg.get("rust").is_none());
        assert_eq!(reg.detect("main.rs"), None);
    }

    #[test]
    fn test_unregister_nonexistent() {
        let mut reg = LanguageRegistry::new();
        reg.unregister("nonexistent"); // Should not panic
    }

    #[test]
    fn test_register_overwrites() {
        let mut reg = LanguageRegistry::new();
        reg.register("rust", "My Rust", ".my_rs", None, &[]);
        let info = reg.get("rust").unwrap();
        assert!(info.contains("My Rust"));
        assert!(!info.contains(".rs"));
    }

    #[test]
    fn test_languages_list() {
        let mut reg = LanguageRegistry::new();
        let before = reg.languages().len();
        reg.register("extra", "Extra", ".extra", None, &[]);
        assert_eq!(reg.languages().len(), before + 1);
    }

    #[test]
    fn test_detect_dotfiles() {
        let reg = LanguageRegistry::new();
        assert_eq!(reg.detect(".bashrc"), None);
    }

    #[test]
    fn test_get_json_format() {
        let reg = LanguageRegistry::new();
        let info = reg.get("json").unwrap();
        assert!(info.contains("\"id\":\"json\"") || info.contains("\"id\": \"json\""));
        assert!(info.contains("\"name\":\"JSON\"") || info.contains("\"name\": \"JSON\""));
    }
}
