#include "LazyRecordProxy.h"

namespace secure_db {

LazyRecordProxy::LazyRecordProxy(std::shared_ptr<std::vector<uint8_t>> binary_data)
    : binary_data_(std::move(binary_data)) {}

facebook::jsi::Value LazyRecordProxy::get(facebook::jsi::Runtime& runtime,
                                          const facebook::jsi::PropNameID& name) {
    std::string key = name.utf8(runtime);
    
    auto it = value_cache_.find(key);
    if (it != value_cache_.end()) {
        return facebook::jsi::Value(runtime, it->second);
    }
    
    facebook::jsi::Value val = parseProperty(runtime, key);
    if (!val.isUndefined()) {
        value_cache_.emplace(key, facebook::jsi::Value(runtime, val));
    }
    return val;
}

std::vector<facebook::jsi::PropNameID> LazyRecordProxy::getPropertyNames(facebook::jsi::Runtime& runtime) {
    ensureNamesInitialized(runtime);
    std::vector<facebook::jsi::PropNameID> names;
    names.reserve(property_names_.size());
    for (const auto& name : property_names_) {
        names.push_back(facebook::jsi::PropNameID::forUtf8(runtime, name));
    }
    return names;
}

void LazyRecordProxy::ensureNamesInitialized(facebook::jsi::Runtime& runtime) {
    if (names_initialized_) return;
    
    if (binary_data_->empty() || static_cast<BinaryType>((*binary_data_)[0]) != BinaryType::Object) {
        names_initialized_ = true;
        return;
    }

    size_t consumed = 1;
    uint32_t numProps;
    std::memcpy(&numProps, binary_data_->data() + consumed, sizeof(uint32_t));
    consumed += sizeof(uint32_t);

    for (uint32_t i = 0; i < numProps; ++i) {
        uint32_t keyLen;
        std::memcpy(&keyLen, binary_data_->data() + consumed, sizeof(uint32_t));
        consumed += sizeof(uint32_t);
        
        property_names_.emplace_back(reinterpret_cast<const char*>(binary_data_->data() + consumed), keyLen);
        consumed += keyLen;
        
        // Skip value
        consumed += BinarySerializer::getBinarySize(binary_data_->data() + consumed, binary_data_->size() - consumed);
    }
    
    names_initialized_ = true;
}

facebook::jsi::Value LazyRecordProxy::parseProperty(facebook::jsi::Runtime& runtime, const std::string& key) {
    if (binary_data_->empty() || static_cast<BinaryType>((*binary_data_)[0]) != BinaryType::Object) {
        return facebook::jsi::Value::undefined();
    }

    size_t consumed = 1;
    uint32_t numProps;
    std::memcpy(&numProps, binary_data_->data() + consumed, sizeof(uint32_t));
    consumed += sizeof(uint32_t);

    for (uint32_t i = 0; i < numProps; ++i) {
        uint32_t keyLen;
        std::memcpy(&keyLen, binary_data_->data() + consumed, sizeof(uint32_t));
        consumed += sizeof(uint32_t);
        
        std::string currentKey(reinterpret_cast<const char*>(binary_data_->data() + consumed), keyLen);
        consumed += keyLen;
        
        if (currentKey == key) {
            auto result = BinarySerializer::deserialize(runtime, binary_data_->data() + consumed, binary_data_->size() - consumed);
            return std::move(result.first);
        }
        
        // Skip value
        consumed += BinarySerializer::getBinarySize(binary_data_->data() + consumed, binary_data_->size() - consumed);
    }
    
    return facebook::jsi::Value::undefined();
}

} // namespace secure_db
