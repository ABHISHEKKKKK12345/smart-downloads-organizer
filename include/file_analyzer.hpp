#pragma once

#include "types.hpp"
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>

namespace SDO {

class FileAnalyzer {
public:
    static FileAnalyzer& instance();

    // Analyze a file path and populate FileInfo
    FileInfo analyze(const std::string& path);

    // Fast (stats only, no hash)
    FileInfo analyzeQuick(const std::string& path);

    // Hashing
    std::string sha256(const std::string& path);
    std::string md5(const std::string& path);

    // Category detection
    FileCategory categorize(const std::string& extension, const std::string& mimeType = "");
    std::string  detectMimeType(const std::string& path);
    std::string  formatSize(uint64_t bytes);

    // Duplicate detection helpers
    std::string hashKey(const FileInfo& fi) const;

    // Extension maps
    const std::map<std::string, FileCategory>& extensionMap() const;

private:
    FileAnalyzer();
    FileAnalyzer(const FileAnalyzer&) = delete;
    FileAnalyzer& operator=(const FileAnalyzer&) = delete;

    void buildExtensionMap();
    std::string toLower(const std::string& s) const;
    std::string computeSHA256(const std::string& path);
    std::string computeMD5(const std::string& path);

    mutable std::mutex                  m_mutex;
    std::map<std::string, FileCategory> m_extMap;
};

} // namespace SDO
