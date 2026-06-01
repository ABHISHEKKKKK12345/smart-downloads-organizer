/**
 * tests/test_database.cpp
 *
 * Unit tests for the SQLite Database layer: upsert, query, duplicate groups,
 * undo log, statistics, and maintenance operations.
 */

#include "database.hpp"
#include "config_manager.hpp"
#include "logger.hpp"
#include <iostream>
#include <filesystem>
#include <cassert>
#include <chrono>
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

// ─── Test database path ───────────────────────────────────────────────────────
static const std::string TEST_DB =
    (fs::temp_directory_path() / "sdo_test.db").string();

static SDO::FileInfo makeFI(const std::string& path, const std::string& hash = "") {
    SDO::FileInfo fi;
    fi.path       = path;
    fi.filename   = fs::path(path).filename().string();
    fi.extension  = "pdf";
    fi.category   = SDO::FileCategory::Documents;
    fi.status     = SDO::FileStatus::Normal;
    fi.sizeBytes  = 1024;
    fi.sha256Hash = hash;
    fi.detectedAt = std::chrono::system_clock::now();
    fi.modifiedAt = std::chrono::system_clock::now();
    fi.createdAt  = std::chrono::system_clock::now();
    fi.accessedAt = std::chrono::system_clock::now();
    return fi;
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST(test_open_close) {
    std::error_code ec; fs::remove(TEST_DB, ec);
    auto& db = SDO::Database::instance();
    EXPECT_TRUE(db.open(TEST_DB));
    EXPECT_TRUE(db.isOpen());
    db.close();
    EXPECT_FALSE(db.isOpen());
}

TEST(test_upsert_and_get) {
    auto& db = SDO::Database::instance();
    db.open(TEST_DB);
    SDO::FileInfo fi = makeFI("/tmp/sdo_test_doc.pdf", "aabbcc");
    EXPECT_TRUE(db.upsertFile(fi));
    auto result = db.getFile("/tmp/sdo_test_doc.pdf");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->filename,   std::string("sdo_test_doc.pdf"));
    EXPECT_EQ(result->sha256Hash, std::string("aabbcc"));
    EXPECT_EQ(result->sizeBytes,  uint64_t(1024));
}

TEST(test_get_nonexistent) {
    auto& db = SDO::Database::instance();
    auto result = db.getFile("/nonexistent/path/file.pdf");
    EXPECT_FALSE(result.has_value());
}

TEST(test_upsert_overwrites) {
    auto& db = SDO::Database::instance();
    SDO::FileInfo fi = makeFI("/tmp/sdo_test_doc.pdf", "aabbcc");
    db.upsertFile(fi);
    fi.sizeBytes  = 9999;
    fi.sha256Hash = "ddeeff";
    EXPECT_TRUE(db.upsertFile(fi));
    auto r = db.getFile("/tmp/sdo_test_doc.pdf");
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(r->sizeBytes,  uint64_t(9999));
    EXPECT_EQ(r->sha256Hash, std::string("ddeeff"));
}

TEST(test_delete_file) {
    auto& db = SDO::Database::instance();
    SDO::FileInfo fi = makeFI("/tmp/sdo_delete_me.pdf");
    db.upsertFile(fi);
    EXPECT_TRUE(db.deleteFile("/tmp/sdo_delete_me.pdf"));
    EXPECT_FALSE(db.getFile("/tmp/sdo_delete_me.pdf").has_value());
}

TEST(test_mark_processed) {
    auto& db = SDO::Database::instance();
    SDO::FileInfo fi = makeFI("/tmp/sdo_process.pdf");
    fi.isProcessed = false;
    db.upsertFile(fi);
    EXPECT_TRUE(db.markProcessed("/tmp/sdo_process.pdf"));
    auto r = db.getFile("/tmp/sdo_process.pdf");
    EXPECT_TRUE(r.has_value() && r->isProcessed);
}

TEST(test_get_files_by_category) {
    auto& db = SDO::Database::instance();
    // Ensure at least one document exists
    db.upsertFile(makeFI("/tmp/sdo_cat_test.pdf"));
    auto docs = db.getFilesByCategory(SDO::FileCategory::Documents);
    EXPECT_GE(docs.size(), size_t(1));
    for (const auto& f : docs)
        EXPECT_EQ(f.category, SDO::FileCategory::Documents);
}

TEST(test_duplicate_groups) {
    auto& db = SDO::Database::instance();
    // Two files with same hash = duplicate group
    SDO::FileInfo f1 = makeFI("/tmp/sdo_dup_orig.pdf", "deadbeef12345678");
    SDO::FileInfo f2 = makeFI("/tmp/sdo_dup_copy.pdf", "deadbeef12345678");
    db.upsertFile(f1);
    db.upsertFile(f2);

    auto groups = db.getDuplicateGroups();
    bool found = false;
    for (const auto& group : groups) {
        if (group.size() >= 2) {
            bool hasOrig = false, hasCopy = false;
            for (const auto& f : group) {
                if (f.path == "/tmp/sdo_dup_orig.pdf") hasOrig = true;
                if (f.path == "/tmp/sdo_dup_copy.pdf") hasCopy = true;
            }
            if (hasOrig && hasCopy) { found = true; break; }
        }
    }
    EXPECT_TRUE(found);
}

TEST(test_set_duplicate) {
    auto& db = SDO::Database::instance();
    db.upsertFile(makeFI("/tmp/sdo_dup_b.pdf", "cafebabe"));
    EXPECT_TRUE(db.setDuplicate("/tmp/sdo_dup_b.pdf", "/tmp/sdo_orig.pdf"));
    auto r = db.getFile("/tmp/sdo_dup_b.pdf");
    EXPECT_TRUE(r.has_value() && r->isDuplicate);
    EXPECT_EQ(r->duplicateOf, std::string("/tmp/sdo_orig.pdf"));
}

TEST(test_get_duplicates_list) {
    auto& db = SDO::Database::instance();
    auto dups = db.getDuplicates();
    // We set at least one duplicate above
    bool found = false;
    for (const auto& f : dups) {
        if (f.path == "/tmp/sdo_dup_b.pdf") { found = true; break; }
    }
    EXPECT_TRUE(found);
}

TEST(test_large_files_query) {
    auto& db = SDO::Database::instance();
    SDO::FileInfo fi = makeFI("/tmp/sdo_large.iso");
    fi.extension  = "iso";
    fi.category   = SDO::FileCategory::Archives;
    fi.sizeBytes  = 2ULL * 1024 * 1024 * 1024; // 2 GB
    db.upsertFile(fi);

    auto large = db.getLargeFiles(1ULL * 1024 * 1024 * 1024); // threshold 1 GB
    bool found = false;
    for (const auto& f : large)
        if (f.path == "/tmp/sdo_large.iso") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(test_old_files_query) {
    auto& db = SDO::Database::instance();
    SDO::FileInfo fi = makeFI("/tmp/sdo_old.txt");
    fi.extension  = "txt";
    fi.category   = SDO::FileCategory::Documents;
    fi.modifiedAt = std::chrono::system_clock::now() - std::chrono::hours(24 * 60); // 60 days ago
    db.upsertFile(fi);

    auto old = db.getOldFiles(30);
    bool found = false;
    for (const auto& f : old)
        if (f.path == "/tmp/sdo_old.txt") { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(test_search_files) {
    auto& db = SDO::Database::instance();
    db.upsertFile(makeFI("/tmp/sdo_searchable_uniqueXYZ.pdf"));
    auto results = db.searchFiles("uniqueXYZ");
    EXPECT_TRUE(results.size() >= 1);
}

TEST(test_undo_log) {
    auto& db = SDO::Database::instance();

    SDO::UndoEntry e;
    e.id          = "test-undo-001";
    e.action      = SDO::ActionType::Move;
    e.sourcePath  = "/tmp/before.pdf";
    e.destPath    = "/home/user/Documents/after.pdf";
    e.description = "Rule: Documents → Documents";
    e.performedAt = std::chrono::system_clock::now();
    e.canUndo     = true;

    EXPECT_TRUE(db.addUndoEntry(e));

    auto history = db.getUndoHistory(10);
    bool found = false;
    for (const auto& h : history)
        if (h.id == "test-undo-001") { found = true; break; }
    EXPECT_TRUE(found);

    EXPECT_TRUE(db.removeUndoEntry("test-undo-001"));
    auto history2 = db.getUndoHistory(10);
    bool gone = true;
    for (const auto& h : history2)
        if (h.id == "test-undo-001") { gone = false; break; }
    EXPECT_TRUE(gone);
}

TEST(test_statistics) {
    auto& db = SDO::Database::instance();
    auto stats = db.getStatistics();
    EXPECT_GE(stats.totalFiles,    size_t(1));
    EXPECT_GE(stats.totalSize,     uint64_t(0));
    EXPECT_GE(stats.duplicateFiles,size_t(0));
}

// C-2 regression: getStatistics largeFiles respects config.largeSizeThreshold
TEST(test_statistics_large_threshold_respected) {
    auto& db  = SDO::Database::instance();
    auto& cfg = SDO::ConfigManager::instance().config();

    // Insert a file of exactly 10 MB
    SDO::FileInfo fi = makeFI("/tmp/sdo_medium_file.bin");
    fi.sizeBytes = 10ULL * 1024 * 1024;
    db.upsertFile(fi);

    // With threshold = 5 MB the file IS large
    cfg.largeSizeThreshold = 5ULL * 1024 * 1024;
    auto stats5 = db.getStatistics();
    bool countedAt5 = false;
    for (const auto& f : db.getLargeFiles(cfg.largeSizeThreshold))
        if (f.path == "/tmp/sdo_medium_file.bin") { countedAt5 = true; break; }
    EXPECT_TRUE(countedAt5);

    // With threshold = 50 MB the same file is NOT large
    cfg.largeSizeThreshold = 50ULL * 1024 * 1024;
    bool countedAt50 = false;
    for (const auto& f : db.getLargeFiles(cfg.largeSizeThreshold))
        if (f.path == "/tmp/sdo_medium_file.bin") { countedAt50 = true; break; }
    EXPECT_FALSE(countedAt50);

    // Restore default
    cfg.largeSizeThreshold = 500ULL * 1024 * 1024;
    db.deleteFile("/tmp/sdo_medium_file.bin");
}

// H-3 regression: getStatistics oldFiles respects config.oldFileAgeDays
TEST(test_statistics_old_threshold_respected) {
    auto& db  = SDO::Database::instance();
    auto& cfg = SDO::ConfigManager::instance().config();

    // Insert a file modified 20 days ago
    SDO::FileInfo fi = makeFI("/tmp/sdo_20day_file.bin");
    fi.modifiedAt = std::chrono::system_clock::now()
                  - std::chrono::hours(24LL * 20);
    db.upsertFile(fi);

    // With threshold = 10 days: file IS old
    cfg.oldFileAgeDays = 10;
    auto old10 = db.getOldFiles(cfg.oldFileAgeDays);
    bool foundAt10 = false;
    for (const auto& f : old10)
        if (f.path == "/tmp/sdo_20day_file.bin") { foundAt10 = true; break; }
    EXPECT_TRUE(foundAt10);

    // With threshold = 30 days: file is NOT old
    cfg.oldFileAgeDays = 30;
    auto old30 = db.getOldFiles(cfg.oldFileAgeDays);
    bool foundAt30 = false;
    for (const auto& f : old30)
        if (f.path == "/tmp/sdo_20day_file.bin") { foundAt30 = true; break; }
    EXPECT_FALSE(foundAt30);

    // Restore default
    cfg.oldFileAgeDays = 30;
    db.deleteFile("/tmp/sdo_20day_file.bin");
}

TEST(test_vacuum) {
    auto& db = SDO::Database::instance();
    EXPECT_TRUE(db.vacuum());
}

TEST(test_prune_old_undo) {
    auto& db = SDO::Database::instance();
    // Insert an ancient undo entry
    SDO::UndoEntry e;
    e.id          = "old-entry-999";
    e.action      = SDO::ActionType::Copy;
    e.sourcePath  = "/tmp/ancient.pdf";
    e.destPath    = "/tmp/dest.pdf";
    e.performedAt = std::chrono::system_clock::now() - std::chrono::hours(24 * 100);
    e.canUndo     = false;
    db.addUndoEntry(e);
    EXPECT_TRUE(db.pruneOldRecords(90));
    // Entry older than 90 days should be gone
    auto history = db.getUndoHistory(1000);
    for (const auto& h : history)
        EXPECT_FALSE(h.id == std::string("old-entry-999"));
}

TEST(test_get_all_files) {
    auto& db = SDO::Database::instance();
    auto all = db.getAllFiles();
    EXPECT_GE(all.size(), size_t(1));
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "\n=== Database Tests ===\n\n";
    SDO::Logger::instance().init("", SDO::LogLevel::Critical);
    SDO::ConfigManager::instance().load(SDO::ConfigManager::defaultConfigPath());

    // Clean slate
    std::error_code ec; fs::remove(TEST_DB, ec);
    SDO::Database::instance().open(TEST_DB);

    RUN(test_open_close);
    SDO::Database::instance().open(TEST_DB); // reopen after close test
    RUN(test_upsert_and_get);
    RUN(test_get_nonexistent);
    RUN(test_upsert_overwrites);
    RUN(test_delete_file);
    RUN(test_mark_processed);
    RUN(test_get_files_by_category);
    RUN(test_duplicate_groups);
    RUN(test_set_duplicate);
    RUN(test_get_duplicates_list);
    RUN(test_large_files_query);
    RUN(test_old_files_query);
    RUN(test_search_files);
    RUN(test_undo_log);
    RUN(test_statistics);
    RUN(test_statistics_large_threshold_respected);
    RUN(test_statistics_old_threshold_respected);
    RUN(test_vacuum);
    RUN(test_prune_old_undo);
    RUN(test_get_all_files);

    SDO::Database::instance().close();
    fs::remove(TEST_DB, ec);

    std::cout << "\n─────────────────────────────────\n";
    std::cout << "Passed: " << g_passed << "  Failed: " << g_failed << "\n";
    return g_failed == 0 ? 0 : 1;
}
