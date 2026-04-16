#include "CachedCryptoContext.h"

namespace secure_db {

CachedCryptoContext::CachedCryptoContext(std::unique_ptr<SecureCryptoContext> inner, size_t cache_pages)
    : inner_(std::move(inner)), max_cache_pages_(cache_pages) {}

CachedCryptoContext::~CachedCryptoContext() = default;

std::vector<uint8_t> CachedCryptoContext::encrypt(const uint8_t* plaintext, size_t length) {
    return inner_->encrypt(plaintext, length);
}

std::vector<uint8_t> CachedCryptoContext::decrypt(const uint8_t* ciphertext, size_t length) {
    // Assuming ciphertext format starts with or is associated with a page offset if used for caching.
    // For record-level caching, we might need the offset passed in.
    // In this simplified version, if it's too short, just pass through.
    if (length < 8) return inner_->decrypt(ciphertext, length);
    
    uint64_t page_offset;
    std::memcpy(&page_offset, ciphertext, sizeof(uint64_t));
    
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    std::vector<uint8_t> cached;
    if (lookupCache(page_offset, cached)) {
        return cached;
    }
    
    // Decrypt and cache (assuming first 8 bytes was the offset for cache key)
    std::vector<uint8_t> decrypted = inner_->decrypt(ciphertext + 8, length - 8);
    insertCache(page_offset, decrypted.data(), decrypted.size());
    
    return decrypted;
}

void CachedCryptoContext::encryptInto(const uint8_t* plaintext, size_t length, 
                                      uint8_t* out_buffer, size_t& out_length) {
    inner_->encryptInto(plaintext, length, out_buffer, out_length);
}

bool CachedCryptoContext::decryptInto(const uint8_t* ciphertext, size_t length,
                                      uint8_t* out_buffer, size_t& out_length) {
    if (length < 8) return inner_->decryptInto(ciphertext, length, out_buffer, out_length);

    uint64_t page_offset;
    std::memcpy(&page_offset, ciphertext, sizeof(uint64_t));

    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = cache_map_.find(page_offset);
    if (it != cache_map_.end()) {
        lru_cache_.splice(lru_cache_.begin(), lru_cache_, it->second);
        out_length = it->second->decrypted_data.size();
        std::memcpy(out_buffer, it->second->decrypted_data.data(), out_length);
        return true;
    }

    if (inner_->decryptInto(ciphertext + 8, length - 8, out_buffer, out_length)) {
        insertCache(page_offset, out_buffer, out_length);
        return true;
    }
    return false;
}

bool CachedCryptoContext::lookupCache(uint64_t page_offset, std::vector<uint8_t>& out) {
    auto it = cache_map_.find(page_offset);
    if (it == cache_map_.end()) return false;
    
    lru_cache_.splice(lru_cache_.begin(), lru_cache_, it->second);
    out = it->second->decrypted_data;
    return true;
}

void CachedCryptoContext::insertCache(uint64_t page_offset, const uint8_t* data, size_t len) {
    if (lru_cache_.size() >= max_cache_pages_) {
        auto& oldest = lru_cache_.back();
        cache_map_.erase(oldest.page_offset);
        lru_cache_.pop_back();
    }
    
    CacheEntry entry;
    entry.page_offset = page_offset;
    entry.decrypted_data.assign(data, data + len);
    
    lru_cache_.push_front(std::move(entry));
    cache_map_[page_offset] = lru_cache_.begin();
}

void CachedCryptoContext::clearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    lru_cache_.clear();
    cache_map_.clear();
}

void CachedCryptoContext::invalidatePage(uint64_t page_offset) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = cache_map_.find(page_offset);
    if (it != cache_map_.end()) {
        lru_cache_.erase(it->second);
        cache_map_.erase(it);
    }
}

} // namespace secure_db
