#pragma once

#include <string>

namespace turbo_db {

class PlatformUtilsIOS {
public:
    static void applyStrictFileProtection(const std::string& filePath);
    static void applyStrictFileProtectionToDirectory(const std::string& dirPath);
};

}