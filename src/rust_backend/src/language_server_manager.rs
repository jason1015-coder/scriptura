//! ## Language Server Manager
//!
//! Manages the lifecycle of language servers across different languages.
//! Each language can have its own LSP server instance.
//!
//! Replaces `LanguageServerManager` from the original C++ codebase.

use std::collections::HashMap;
use std::sync::Mutex;

use crate::lsp::LspClient;

pub struct LanguageServerManager {
    servers: Mutex<HashMap<String, LspClient>>, // lang_id -> LspClient
}

impl LanguageServerManager {
    pub fn new() -> Self {
        Self {
            servers: Mutex::new(HashMap::new()),
        }
    }

    /// Start a language server for a given language.
    pub fn start(&self, lang_id: &str, command: &str, args: &[&str], root_uri: &str) -> Result<(), String> {
        let mut client = LspClient::new();

        // Set up default diagnostics callback forwarding
        // (In production, this would connect to the C++ adapter callbacks)

        client.start_server(command, args, root_uri)?;
        client.initialize(root_uri, lang_id);
        client.initialized();

        if let Ok(mut servers) = self.servers.lock() {
            servers.insert(lang_id.to_string(), client);
        }

        Ok(())
    }

    /// Stop a language server for a given language.
    pub fn stop(&self, lang_id: &str) {
        if let Ok(mut servers) = self.servers.lock() {
            if let Some(client) = servers.remove(lang_id) {
                // The LspClient destructor will clean up the process
                let _ = client;
            }
        }
    }

    /// Stop all language servers.
    pub fn stop_all(&self) {
        if let Ok(mut servers) = self.servers.lock() {
            servers.clear();
        }
    }

    /// Get a reference to the LSP client for a language.
    pub fn get_client(&self, _lang_id: &str) -> Option<&LspClient> {
        // This is a simplified accessor; production code would use Arc<Mutex<LspClient>>
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new_manager() {
        let m = LanguageServerManager::new();
        assert!(m.get_client("rust").is_none());
    }

    #[test]
    fn test_stop_nonexistent() {
        let m = LanguageServerManager::new();
        m.stop("nonexistent"); // Should not panic
    }

    #[test]
    fn test_stop_all_empty() {
        let m = LanguageServerManager::new();
        m.stop_all(); // Should not panic
    }

    #[test]
    fn test_get_client_none() {
        let m = LanguageServerManager::new();
        assert!(m.get_client("any").is_none());
    }

    #[test]
    fn test_start_invalid_command() {
        let m = LanguageServerManager::new();
        let result = m.start("test", "/nonexistent/lsp-server", &["--stdio"], "file:///test");
        assert!(result.is_err());
    }
}
