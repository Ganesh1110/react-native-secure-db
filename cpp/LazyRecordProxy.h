#pragma once
#include <jsi/jsi.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "BinarySerializer.h"

namespace turbo_db {

class LazyRecordProxy : public facebook::jsi::HostObject {
public:
    LazyRecordProxy(std::shared_ptr<std::vector<uint8_t>> binary_data);
    
    facebook::jsi::Value get(facebook::jsi::Runtime& runtime,
                             const facebook::jsi::PropNameID& name) override;
    
    std::vector<facebook::jsi::PropNameID> getPropertyNames(facebook::jsi::Runtime& runtime) override;

private:
    std::shared_ptr<std::vector<uint8_t>> binary_data_;
    std::unordered_map<std::string, facebook::jsi::Value> value_cache_;
    std::vector<std::string> property_names_;
    bool names_initialized_ = false;

    void ensureNamesInitialized(facebook::jsi::Runtime& runtime);
    facebook::jsi::Value parseProperty(facebook::jsi::Runtime& runtime, const std::string& key);
};

} // namespace turbo_db
