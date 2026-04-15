#include "SodiumCryptoContext.h"
#include <cstring>
#include <mutex>

namespace secure_db {

void SodiumCryptoContext::ensureInitialized() {
    static std::once_flag init_flag;
    std::call_once(init_flag, []() {
        if (sodium_init() == -1) {
            throw std::runtime_error("SodiumCryptoContext: libsodium init failed");
        }
    });
}

SodiumCryptoContext::SodiumCryptoContext() {
    ensureInitialized();
    std::memset(master_key_, 0, sizeof(master_key_));
}

SodiumCryptoContext::~SodiumCryptoContext() {
    // Securely wipe key from memory
    sodium_memzero(master_key_, sizeof(master_key_));
}

void SodiumCryptoContext::setMasterKey(const std::vector<uint8_t>& key) {
    std::lock_guard<std::mutex> lock(key_mutex_);
    if (key.size() != crypto_aead_xchacha20poly1305_ietf_KEYBYTES) {
        throw std::runtime_error("SodiumCryptoContext: Invalid key size. Expected 32 bytes.");
    }
    std::memcpy(master_key_, key.data(), key.size());
    key_is_set_ = true;
}

std::vector<uint8_t> SodiumCryptoContext::encrypt(const uint8_t* plaintext, size_t length) {
    std::lock_guard<std::mutex> lock(key_mutex_);
    if (!key_is_set_) throw std::runtime_error("SodiumCryptoContext: Master key not initialized");

    // 1. Generate random 24-byte nonce
    uint8_t nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
    randombytes_buf(nonce, sizeof(nonce));

    // 2. Prepare output buffer: Nonce + Plaintext + MAC Tag (ABYTES)
    size_t out_len = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES + length + crypto_aead_xchacha20poly1305_ietf_ABYTES;
    std::vector<uint8_t> result(out_len);

    // Copy nonce to head of result
    std::memcpy(result.data(), nonce, sizeof(nonce));

    // 3. Perform AEAD Encryption
    unsigned long long actual_cipher_len;
    int status = crypto_aead_xchacha20poly1305_ietf_encrypt(
        result.data() + sizeof(nonce), &actual_cipher_len,
        plaintext, length,
        nullptr, 0, // No Additional Authenticated Data
        nullptr, nonce, master_key_
    );

    if (status != 0) throw std::runtime_error("SodiumCryptoContext: Encryption failed");
    
    return result;
}

std::vector<uint8_t> SodiumCryptoContext::decrypt(const uint8_t* ciphertext_with_nonce, size_t length) {
    std::lock_guard<std::mutex> lock(key_mutex_);
    if (!key_is_set_) throw std::runtime_error("SodiumCryptoContext: Master key not initialized");

    // 1. Length validation (must at least contain nonce and MAC tag)
    if (length < crypto_aead_xchacha20poly1305_ietf_NPUBBYTES + crypto_aead_xchacha20poly1305_ietf_ABYTES) {
        throw std::runtime_error("SodiumCryptoContext: Ciphertext payload too short/corrupted");
    }

    // 2. Extract Nonce
    const uint8_t* nonce = ciphertext_with_nonce;
    const uint8_t* actual_ciphertext = ciphertext_with_nonce + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
    size_t actual_cipher_len = length - crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;

    // 3. Prepare Plaintext Buffer
    std::vector<uint8_t> plaintext(actual_cipher_len - crypto_aead_xchacha20poly1305_ietf_ABYTES);
    unsigned long long actual_plain_len;

    // 4. Decrypt and Authenticate
    int status = crypto_aead_xchacha20poly1305_ietf_decrypt(
        plaintext.data(), &actual_plain_len,
        nullptr,
        actual_ciphertext, actual_cipher_len,
        nullptr, 0,
        nonce, master_key_
    );

    if (status != 0) {
        // Critical: Authentication failed. Data has been tampered with or key is wrong.
        throw std::runtime_error("SodiumCryptoContext: Integrity check failed. Block is corrupted or tampered.");
    }

    return plaintext;
}

} // namespace secure_db
