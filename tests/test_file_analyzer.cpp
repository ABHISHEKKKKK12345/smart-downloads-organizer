/**
 * tests/test_file_analyzer.cpp
 *
 * Unit tests for FileAnalyzer: category detection, MIME sniffing,
 * SHA-256 hashing, size formatting, and FileInfo helper methods.
 *
 * Uses a minimal self-contained test harness (no external framework required).
 */

#include "file_analyzer.hpp"
#include "config_manager.hpp"
#include "logger.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cassert>
#include <cstring>
#include <stdexcept>

namespace fs = std::filesystem;

// ─── Minimal test framework ───────────────────────────────────────────────────
static int g_passed = 0;
static int g_failed = 0;

template<typename T>
static void printVal(std::ostream& os, const T& v) {
    if constexpr (std::is_enum_v<T>)
        os << static_cast<long long>(v);
    else
        os << v;
}

#define EXPECT_EQ(a, b)  do { \
    auto _a = (a); auto _b = (b); \
    if (_a == _b) { ++g_passed; } \
    else { ++g_failed; \
      std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << "  expected="; \
      printVal(std::cerr, _b); \
      std::cerr << "  got="; printVal(std::cerr, _a); \
      std::cerr << "\n"; } \
} while(0)
#define EXPECT_TRUE(x)   EXPECT_EQ(!!(x), true)
#define EXPECT_FALSE(x)  EXPECT_EQ(!!(x), false)
#define EXPECT_NE(a, b)  do { \
    if ((a) != (b)) { ++g_passed; } \
    else { ++g_failed; \
      std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ \
                << "  values should differ\n"; } \
} while(0)
#define EXPECT_GE(a, b)  do { \
    if ((a) >= (b)) { ++g_passed; } \
    else { ++g_failed; \
      std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << "  "; \
      printVal(std::cerr, a); std::cerr << " not >= "; printVal(std::cerr, b); \
      std::cerr << "\n"; } \
} while(0)

#define TEST(name) static void name()
#define RUN(name)  do { std::cout << "  " #name " ... "; name(); std::cout << "ok\n"; } while(0)

// ─── Temp file helper ─────────────────────────────────────────────────────────
struct TempFile {
    fs::path path;
    explicit TempFile(const std::string& ext, const std::vector<uint8_t>& bytes = {}) {
        path = fs::temp_directory_path() / ("sdo_test_" + std::to_string(
                   std::chrono::steady_clock::now().time_since_epoch().count()) + ext);
        std::ofstream f(path, std::ios::binary);
        if (!bytes.empty()) f.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        else f << "placeholder content for test file\n";
    }
    ~TempFile() { std::error_code ec; fs::remove(path, ec); }
};

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST(test_categorize_known_extensions) {
    auto& fa = SDO::FileAnalyzer::instance();

    EXPECT_EQ(fa.categorize("jpg"),  SDO::FileCategory::Images);
    EXPECT_EQ(fa.categorize("JPEG"), SDO::FileCategory::Images);
    EXPECT_EQ(fa.categorize(".png"), SDO::FileCategory::Images);
    EXPECT_EQ(fa.categorize("mp4"),  SDO::FileCategory::Videos);
    EXPECT_EQ(fa.categorize("mkv"),  SDO::FileCategory::Videos);
    EXPECT_EQ(fa.categorize("mp3"),  SDO::FileCategory::Audio);
    EXPECT_EQ(fa.categorize("flac"), SDO::FileCategory::Audio);
    EXPECT_EQ(fa.categorize("pdf"),  SDO::FileCategory::Documents);
    EXPECT_EQ(fa.categorize("docx"), SDO::FileCategory::Documents);
    EXPECT_EQ(fa.categorize("zip"),  SDO::FileCategory::Archives);
    EXPECT_EQ(fa.categorize("7z"),   SDO::FileCategory::Archives);
    EXPECT_EQ(fa.categorize("py"),   SDO::FileCategory::Code);
    EXPECT_EQ(fa.categorize("cpp"),  SDO::FileCategory::Code);
    EXPECT_EQ(fa.categorize("ttf"),  SDO::FileCategory::Fonts);
    EXPECT_EQ(fa.categorize("epub"), SDO::FileCategory::Ebooks);
    EXPECT_EQ(fa.categorize("torrent"), SDO::FileCategory::Torrents);
    EXPECT_EQ(fa.categorize("exe"),  SDO::FileCategory::Executables);
    EXPECT_EQ(fa.categorize("sqlite"), SDO::FileCategory::Data);
}

TEST(test_categorize_unknown_extension) {
    auto& fa = SDO::FileAnalyzer::instance();
    EXPECT_EQ(fa.categorize("xyzzy"),  SDO::FileCategory::Unknown);
    EXPECT_EQ(fa.categorize(""),       SDO::FileCategory::Unknown);
    EXPECT_EQ(fa.categorize("zzz999"),SDO::FileCategory::Unknown);
}

TEST(test_categorize_case_insensitive) {
    auto& fa = SDO::FileAnalyzer::instance();
    EXPECT_EQ(fa.categorize("JPG"),  SDO::FileCategory::Images);
    EXPECT_EQ(fa.categorize("MP4"),  SDO::FileCategory::Videos);
    EXPECT_EQ(fa.categorize("PDF"),  SDO::FileCategory::Documents);
    EXPECT_EQ(fa.categorize("ZIP"),  SDO::FileCategory::Archives);
}

TEST(test_mime_type_jpeg) {
    // JPEG magic: FF D8
    TempFile tf(".jpg", {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 'J','F','I','F'});
    auto& fa = SDO::FileAnalyzer::instance();
    EXPECT_EQ(fa.detectMimeType(tf.path.string()), std::string("image/jpeg"));
}

TEST(test_mime_type_png) {
    TempFile tf(".png", {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A,0x00,0x00});
    auto& fa = SDO::FileAnalyzer::instance();
    EXPECT_EQ(fa.detectMimeType(tf.path.string()), std::string("image/png"));
}

TEST(test_mime_type_pdf) {
    TempFile tf(".pdf", {'%','P','D','F','-','1','.','4'});
    auto& fa = SDO::FileAnalyzer::instance();
    EXPECT_EQ(fa.detectMimeType(tf.path.string()), std::string("application/pdf"));
}

TEST(test_mime_type_zip) {
    TempFile tf(".zip", {'P','K',0x03,0x04,0x14,0x00});
    auto& fa = SDO::FileAnalyzer::instance();
    EXPECT_EQ(fa.detectMimeType(tf.path.string()), std::string("application/zip"));
}

TEST(test_mime_type_text_fallback) {
    TempFile tf(".py", {'#','!','/','u','s','r','/','b','i','n','/','p','y'});
    auto& fa = SDO::FileAnalyzer::instance();
    EXPECT_EQ(fa.detectMimeType(tf.path.string()), std::string("text/plain"));
}

TEST(test_sha256_consistency) {
    TempFile tf(".bin");
    auto& fa = SDO::FileAnalyzer::instance();
    std::string h1 = fa.sha256(tf.path.string());
    std::string h2 = fa.sha256(tf.path.string());
    EXPECT_EQ(h1, h2);
    EXPECT_EQ(h1.size(), 64u); // SHA-256 = 32 bytes = 64 hex chars
}

TEST(test_sha256_known_value) {
    // SHA-256("abc") = ba7816bf8f01cfea414140de5dae2ec73b00361bbef0469345d08cf93bece0d5 (wait — correct is:)
    // SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    TempFile tf(".bin", {}); // 0-byte file
    // But our TempFile writes a fallback string — create explicit empty file
    fs::path ep = fs::temp_directory_path() / "sdo_empty_test.bin";
    { std::ofstream f(ep, std::ios::binary); } // empty
    auto& fa = SDO::FileAnalyzer::instance();
    std::string hash = fa.sha256(ep.string());
    std::error_code ec; fs::remove(ep, ec);
    EXPECT_EQ(hash, std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

TEST(test_sha256_different_files) {
    TempFile tf1(".bin", {1,2,3,4,5});
    TempFile tf2(".bin", {5,4,3,2,1});
    auto& fa = SDO::FileAnalyzer::instance();
    EXPECT_NE(fa.sha256(tf1.path.string()), fa.sha256(tf2.path.string()));
}

TEST(test_format_size) {
    auto& fa = SDO::FileAnalyzer::instance();
    EXPECT_EQ(fa.formatSize(0),                  std::string("0 B"));
    EXPECT_EQ(fa.formatSize(512),                std::string("512 B"));
    EXPECT_EQ(fa.formatSize(1024),               std::string("1.0 KB"));
    EXPECT_EQ(fa.formatSize(1024*1024),          std::string("1.0 MB"));
    EXPECT_EQ(fa.formatSize(1024ULL*1024*1024),  std::string("1.0 GB"));
}

TEST(test_analyze_quick) {
    TempFile tf(".pdf", {'%','P','D','F','-','1','.','4',' ','x'});
    auto& fa = SDO::FileAnalyzer::instance();
    SDO::FileInfo fi = fa.analyzeQuick(tf.path.string());
    EXPECT_EQ(fi.path,      tf.path.string());
    EXPECT_EQ(fi.extension, std::string("pdf"));
    EXPECT_EQ(fi.category,  SDO::FileCategory::Documents);
    EXPECT_TRUE(fi.sizeBytes > 0);
    EXPECT_FALSE(fi.filename.empty());
}

TEST(test_analyze_full) {
    TempFile tf(".png", {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A,0x00});
    auto& fa = SDO::FileAnalyzer::instance();
    SDO::FileInfo fi = fa.analyze(tf.path.string());
    EXPECT_EQ(fi.category,  SDO::FileCategory::Images);
    EXPECT_EQ(fi.mimeType,  std::string("image/png"));
    EXPECT_EQ(fi.sha256Hash.size(), 64u);
    EXPECT_FALSE(fi.sha256Hash.empty());
}

TEST(test_fileinfo_category_name) {
    SDO::FileInfo fi;
    fi.category = SDO::FileCategory::Images;
    EXPECT_EQ(fi.categoryName(), std::string("Images"));
    fi.category = SDO::FileCategory::Videos;
    EXPECT_EQ(fi.categoryName(), std::string("Videos"));
    fi.category = SDO::FileCategory::Unknown;
    EXPECT_EQ(fi.categoryName(), std::string("Unknown"));
}

TEST(test_fileinfo_status_name) {
    SDO::FileInfo fi;
    fi.status = SDO::FileStatus::Normal;
    EXPECT_EQ(fi.statusName(), std::string("Normal"));
    fi.status = SDO::FileStatus::Duplicate;
    EXPECT_EQ(fi.statusName(), std::string("Duplicate"));
    fi.status = SDO::FileStatus::Large;
    EXPECT_EQ(fi.statusName(), std::string("Large"));
}

TEST(test_fileinfo_age) {
    SDO::FileInfo fi;
    fi.modifiedAt = std::chrono::system_clock::now() -
                    std::chrono::hours(24 * 10); // 10 days ago
    int64_t age = fi.ageInDays();
    EXPECT_TRUE(age >= 9 && age <= 11); // allow 1-day tolerance
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "\n=== FileAnalyzer Tests ===\n\n";

    // Minimal init — no GUI needed
    SDO::ConfigManager::instance().load(SDO::ConfigManager::defaultConfigPath());
    SDO::Logger::instance().init("", SDO::LogLevel::Critical); // suppress output in tests

    RUN(test_categorize_known_extensions);
    RUN(test_categorize_unknown_extension);
    RUN(test_categorize_case_insensitive);
    RUN(test_mime_type_jpeg);
    RUN(test_mime_type_png);
    RUN(test_mime_type_pdf);
    RUN(test_mime_type_zip);
    RUN(test_mime_type_text_fallback);
    RUN(test_sha256_consistency);
    RUN(test_sha256_known_value);
    RUN(test_sha256_different_files);
    RUN(test_format_size);
    RUN(test_analyze_quick);
    RUN(test_analyze_full);
    RUN(test_fileinfo_category_name);
    RUN(test_fileinfo_status_name);
    RUN(test_fileinfo_age);

    std::cout << "\n─────────────────────────────────\n";
    std::cout << "Passed: " << g_passed << "  Failed: " << g_failed << "\n";
    return g_failed == 0 ? 0 : 1;
}
