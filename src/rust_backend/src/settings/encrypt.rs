//! # Encryption Module
//! AES-256-GCM authenticated encryption for settings data.

use aes_gcm::{
    aead::{Aead, KeyInit, OsRng},
    Aes256Gcm, Key, Nonce,
};
use base64::{engine::general_purpose::STANDARD as BASE64, Engine};
use rand::RngCore;
use std::sync::Mutex;
use thiserror::Error;

const NONCE_SIZE: usize = 12;
const KEY_SIZE: usize = 32;

#[derive(Error, Debug)]
pub enum EncryptionError {
    #[error("Encryption failed: {0}")]
    Encryption(String),
    #[error("Decryption failed: {0}")]
    Decryption(String),
    #[error("Key not available: {0}")]
    KeyUnavailable(String),
    #[error("Invalid data format: {0}")]
    InvalidFormat(String),
}

#[derive(Clone)]
pub struct EncryptionKey {
    key_b64: String,
}

impl EncryptionKey {
    pub fn generate() -> Self {
        let mut key_bytes = [0u8; KEY_SIZE];
        OsRng.fill_bytes(&mut key_bytes);
        Self {
            key_b64: BASE64.encode(&key_bytes),
        }
    }

    pub fn from_base64(encoded: &str) -> Result<Self, EncryptionError> {
        let key_bytes = BASE64
            .decode(encoded)
            .map_err(|e| EncryptionError::InvalidFormat(e.to_string()))?;
        if key_bytes.len() != KEY_SIZE {
            return Err(EncryptionError::InvalidFormat("Key must be 32 bytes".to_string()));
        }
        Ok(Self { key_b64: encoded.to_string() })
    }

    pub fn to_base64(&self) -> String {
        self.key_b64.clone()
    }

    fn key_bytes(&self) -> [u8; KEY_SIZE] {
        BASE64.decode(&self.key_b64).expect("Invalid key encoding").try_into().expect("Wrong key size")
    }
}

pub fn encrypt(plaintext: &[u8], key: &EncryptionKey) -> Result<Vec<u8>, EncryptionError> {
    let key_bytes = key.key_bytes();
    let key = Key::<Aes256Gcm>::from_slice(&key_bytes);
    let mut nonce_bytes = [0u8; NONCE_SIZE];
    OsRng.fill_bytes(&mut nonce_bytes);
    let nonce = Nonce::from_slice(&nonce_bytes);
    let cipher = Aes256Gcm::new(key);
    let mut result = nonce_bytes.to_vec();
    // Encrypt ONLY the plaintext, then append the ciphertext after the nonce
    result.extend_from_slice(&cipher.encrypt(nonce, plaintext).map_err(|e| EncryptionError::Encryption(e.to_string()))?);
    Ok(result)
}

pub fn decrypt(ciphertext: &[u8], key: &EncryptionKey) -> Result<Vec<u8>, EncryptionError> {
    if ciphertext.len() < NONCE_SIZE + 16 {
        return Err(EncryptionError::InvalidFormat("Ciphertext too short".to_string()));
    }
    let key_bytes = key.key_bytes();
    let key = Key::<Aes256Gcm>::from_slice(&key_bytes);
    let nonce = Nonce::from_slice(&ciphertext[..NONCE_SIZE]);
    let cipher = Aes256Gcm::new(key);
    // Decrypt only the ciphertext portion (after the nonce), NOT the whole buffer
    cipher.decrypt(nonce, &ciphertext[NONCE_SIZE..]).map_err(|e| EncryptionError::Decryption(e.to_string()))
}

pub fn encrypt_string(plaintext: &str, key: &EncryptionKey) -> Result<String, EncryptionError> {
    let ciphertext = encrypt(plaintext.as_bytes(), key)?;
    Ok(BASE64.encode(&ciphertext))
}

pub fn decrypt_string(ciphertext_b64: &str, key: &EncryptionKey) -> Result<String, EncryptionError> {
    let ciphertext = BASE64.decode(ciphertext_b64).map_err(|e| EncryptionError::InvalidFormat(e.to_string()))?;
    let plaintext = decrypt(&ciphertext, key)?;
    String::from_utf8(plaintext).map_err(|e| EncryptionError::InvalidFormat(e.to_string()))
}

pub struct KeyStore {
    key: Mutex<Option<EncryptionKey>>,
}

impl KeyStore {
    pub fn new() -> Self {
        Self { key: Mutex::new(None) }
    }

    pub fn set_key(&self, key: EncryptionKey) {
        *self.key.lock().unwrap() = Some(key);
    }

    pub fn get_key(&self) -> Result<EncryptionKey, EncryptionError> {
        self.key.lock().unwrap().clone().ok_or_else(|| EncryptionError::KeyUnavailable("No encryption key loaded".to_string()))
    }
}

impl Default for KeyStore {
    fn default() -> Self { Self::new() }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_encrypt_decrypt_roundtrip() {
        let key = EncryptionKey::generate();
        let plaintext = "Hello, World!";
        let ciphertext = encrypt_string(plaintext, &key).unwrap();
        let decrypted = decrypt_string(&ciphertext, &key).unwrap();
        assert_eq!(plaintext, decrypted);
    }

    #[test]
    fn test_encrypt_different_outputs() {
        let key = EncryptionKey::generate();
        let plaintext = "Test data";
        let ct1 = encrypt_string(plaintext, &key).unwrap();
        let ct2 = encrypt_string(plaintext, &key).unwrap();
        assert_ne!(ct1, ct2);
    }

    #[test]
    fn test_wrong_key_fails() {
        let key1 = EncryptionKey::generate();
        let key2 = EncryptionKey::generate();
        let ciphertext = encrypt_string("Secret message", &key1).unwrap();
        assert!(decrypt_string(&ciphertext, &key2).is_err());
    }
}
