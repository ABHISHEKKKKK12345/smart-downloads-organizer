/**
 * tests/test_organizer_engine.cpp
 *
 * Tests rule matching, condition evaluation, action execution (in simulate mode),
 * and duplicate scanning.  All file-system operations run in simulate mode so no
 * real files are moved.
 */

#include "organizer_engine.hpp"
#include "config_manager.hpp"
#include "database.hpp"
#include "file_analyzer.hpp"
#include "logger.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cassert>
#include <chrono>

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

static const std::string TEST_DB =
    (fs::temp_directory_path() / "sdo_engine_test.db").string();

// ─── Helper: build a FileInfo directly ───────────────────────────────────────
static SDO::FileInfo makeFile(const std::string& path,
                               const std::string& ext,
                               SDO::FileCategory   cat,
                               uint64_t size = 1024,
                               int ageDays   = 0)
{
    SDO::FileInfo fi;
    fi.path       = path;
    fi.filename   = fs::path(path).filename().string();
    fi.extension  = ext;
    fi.category   = cat;
    fi.sizeBytes  = size;
    fi.status     = SDO::FileStatus::Normal;
    fi.detectedAt = std::chrono::system_clock::now();
    fi.modifiedAt = std::chrono::system_clock::now()
                  - std::chrono::hours(24LL * ageDays);
    fi.createdAt  = fi.modifiedAt;
    fi.accessedAt = fi.modifiedAt;
    return fi;
}

// ─── Helper: build a simple rule ─────────────────────────────────────────────
static SDO::OrganizerRule makeRule(const std::string& id,
                                    const std::string& field,
                                    const std::string& op,
                                    const std::string& value,
                                    SDO::ActionType    action = SDO::ActionType::Move,
                                    const std::string& dest   = "/tmp/sdo_dest",
                                    int priority = 10)
{
    SDO::OrganizerRule r;
    r.id      = id;
    r.name    = "Test rule " + id;
    r.enabled = true;
    r.priority= priority;
    r.conditionLogic = "OR";

    SDO::RuleCondition c;
    c.field = field;
    c.op    = op;
    c.value = value;
    r.conditions.push_back(c);

    r.action.type            = action;
    r.action.targetDirectory = dest;
    r.action.createSubfolders= false;
    return r;
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST(test_rule_matches_extension_eq) {
    auto& engine = SDO::OrganizerEngine::instance();
    SDO::FileInfo fi = makeFile("/tmp/test.pdf", "pdf", SDO::FileCategory::Documents);
    SDO::Database::instance().upsertFile(fi);

    // Rule: extension eq pdf → Move
    auto& cfg = SDO::ConfigManager::instance().config();
    cfg.rules.clear();
    cfg.rules.push_back(makeRule("r-pdf", "extension", "eq", "pdf"));
    cfg.autoOrganize = false;
    cfg.simulateMode = true;

    auto result = engine.applyRules(fi);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.action, SDO::ActionType::Move);
    EXPECT_EQ(result.ruleName, std::string("Test rule r-pdf"));
}

TEST(test_rule_no_match_different_ext) {
    auto& engine = SDO::OrganizerEngine::instance();
    SDO::FileInfo fi = makeFile("/tmp/test.mp4", "mp4", SDO::FileCategory::Videos);
    SDO::Database::instance().upsertFile(fi);

    auto& cfg = SDO::ConfigManager::instance().config();
    cfg.rules.clear();
    cfg.rules.push_back(makeRule("r-pdf2", "extension", "eq", "pdf"));

    auto result = engine.applyRules(fi);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.action, SDO::ActionType::Skip);
}

TEST(test_rule_matches_name_contains) {
    auto& engine = SDO::OrganizerEngine::instance();
    SDO::FileInfo fi = makeFile("/tmp/invoice_2024_Q1.pdf", "pdf",
                                 SDO::FileCategory::Documents);
    SDO::Database::instance().upsertFile(fi);

    auto& cfg = SDO::ConfigManager::instance().config();
    cfg.rules.clear();
    cfg.rules.push_back(makeRule("r-invoice", "name", "contains", "invoice"));

    auto result = engine.applyRules(fi);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.action, SDO::ActionType::Move);
}

TEST(test_rule_or_logic) {
    auto& engine = SDO::OrganizerEngine::instance();
    SDO::FileInfo fi = makeFile("/tmp/photo.jpeg", "jpeg", SDO::FileCategory::Images);
    SDO::Database::instance().upsertFile(fi);

    auto& cfg = SDO::ConfigManager::instance().config();
    cfg.rules.clear();

    SDO::OrganizerRule r;
    r.id = "r-img-or"; r.name = "Images OR"; r.enabled = true; r.priority = 10;
    r.conditionLogic = "OR";
    r.action.type = SDO::ActionType::Move;
    r.action.targetDirectory = "/tmp/sdo_pics";
    r.action.createSubfolders = false;
    for (auto& ext : {"jpg","jpeg","png","gif"}) {
        SDO::RuleCondition c; c.field="extension"; c.op="eq"; c.value=ext;
        r.conditions.push_back(c);
    }
    cfg.rules.push_back(r);

    auto result = engine.applyRules(fi);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.action, SDO::ActionType::Move);
}

TEST(test_rule_and_logic_both_must_match) {
    auto& engine = SDO::OrganizerEngine::instance();
    SDO::FileInfo fi = makeFile("/tmp/big_report.pdf", "pdf",
                                 SDO::FileCategory::Documents,
                                 600ULL * 1024 * 1024); // 600 MB
    SDO::Database::instance().upsertFile(fi);

    auto& cfg = SDO::ConfigManager::instance().config();
    cfg.rules.clear();

    SDO::OrganizerRule r;
    r.id = "r-big-pdf"; r.name = "Big PDFs"; r.enabled = true; r.priority = 10;
    r.conditionLogic = "AND";
    r.action.type = SDO::ActionType::Move;
    r.action.targetDirectory = "/tmp/sdo_bigdocs";
    r.action.createSubfolders = false;

    SDO::RuleCondition c1; c1.field = "extension"; c1.op = "eq"; c1.value = "pdf";
    SDO::RuleCondition c2; c2.field = "size";      c2.op = "gt"; c2.value = "524288000"; // > 500MB
    r.conditions = {c1, c2};
    cfg.rules.push_back(r);

    auto result = engine.applyRules(fi);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.action, SDO::ActionType::Move);
}

TEST(test_rule_and_logic_partial_no_match) {
    auto& engine = SDO::OrganizerEngine::instance();
    // Small PDF — should NOT match the "big PDF" rule
    SDO::FileInfo fi = makeFile("/tmp/small_report.pdf", "pdf",
                                 SDO::FileCategory::Documents, 1024);
    SDO::Database::instance().upsertFile(fi);

    auto& cfg = SDO::ConfigManager::instance().config();
    cfg.rules.clear();

    SDO::OrganizerRule r;
    r.id = "r-big-pdf2"; r.name = "Big PDFs2"; r.enabled = true; r.priority = 10;
    r.conditionLogic = "AND";
    r.action.type = SDO::ActionType::Move;
    r.action.targetDirectory = "/tmp/sdo_bigdocs2";
    r.action.createSubfolders = false;
    SDO::RuleCondition c1; c1.field = "extension"; c1.op = "eq"; c1.value = "pdf";
    SDO::RuleCondition c2; c2.field = "size";      c2.op = "gt"; c2.value = "524288000";
    r.conditions = {c1, c2};
    cfg.rules.push_back(r);

    auto result = engine.applyRules(fi);
    EXPECT_EQ(result.action, SDO::ActionType::Skip);
}

TEST(test_rule_disabled_is_skipped) {
    auto& engine = SDO::OrganizerEngine::instance();
    SDO::FileInfo fi = makeFile("/tmp/disabled_test.pdf", "pdf",
                                 SDO::FileCategory::Documents);
    SDO::Database::instance().upsertFile(fi);

    auto& cfg = SDO::ConfigManager::instance().config();
    cfg.rules.clear();
    auto r = makeRule("r-disabled", "extension", "eq", "pdf");
    r.enabled = false;
    cfg.rules.push_back(r);

    auto result = engine.applyRules(fi);
    EXPECT_EQ(result.action, SDO::ActionType::Skip);
}

TEST(test_rule_priority_order) {
    auto& engine = SDO::OrganizerEngine::instance();
    SDO::FileInfo fi = makeFile("/tmp/priority_test.pdf", "pdf",
                                 SDO::FileCategory::Documents);
    SDO::Database::instance().upsertFile(fi);

    auto& cfg = SDO::ConfigManager::instance().config();
    cfg.rules.clear();

    auto r1 = makeRule("r-low",  "extension", "eq", "pdf", SDO::ActionType::Move,
                       "/tmp/low_priority",  5);
    auto r2 = makeRule("r-high", "extension", "eq", "pdf", SDO::ActionType::Move,
                       "/tmp/high_priority", 50);
    cfg.rules = {r1, r2}; // low first in vector

    auto result = engine.applyRules(fi);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.ruleName, std::string("Test rule r-high")); // higher priority wins
}

TEST(test_regex_condition) {
    auto& engine = SDO::OrganizerEngine::instance();
    SDO::FileInfo fi = makeFile("/tmp/invoice_2024_03.pdf", "pdf",
                                 SDO::FileCategory::Documents);
    SDO::Database::instance().upsertFile(fi);

    auto& cfg = SDO::ConfigManager::instance().config();
    cfg.rules.clear();
    cfg.rules.push_back(makeRule("r-regex", "name", "regex", "invoice_\\d{4}_\\d{2}"));

    auto result = engine.applyRules(fi);
    EXPECT_EQ(result.action, SDO::ActionType::Move);
}

TEST(test_age_condition) {
    auto& engine = SDO::OrganizerEngine::instance();
    // File that is 45 days old
    SDO::FileInfo fi = makeFile("/tmp/old_file.log", "log",
                                 SDO::FileCategory::Unknown, 512, 45);
    SDO::Database::instance().upsertFile(fi);

    auto& cfg = SDO::ConfigManager::instance().config();
    cfg.rules.clear();
    // Rule: age > 30 → Move
    cfg.rules.push_back(makeRule("r-old", "age", "gt", "30"));

    auto result = engine.applyRules(fi);
    EXPECT_EQ(result.action, SDO::ActionType::Move);
}

TEST(test_simulate_mode_no_actual_move) {
    auto& engine = SDO::OrganizerEngine::instance();
    // Create a real temp file to "move"
    fs::path src = fs::temp_directory_path() / "sdo_sim_test.pdf";
    { std::ofstream f(src); f << "test content\n"; }

    SDO::FileInfo fi = makeFile(src.string(), "pdf", SDO::FileCategory::Documents, 512);
    SDO::Database::instance().upsertFile(fi);

    auto& cfg = SDO::ConfigManager::instance().config();
    cfg.rules.clear();
    cfg.simulateMode = true;
    cfg.rules.push_back(makeRule("r-sim", "extension", "eq", "pdf",
                                  SDO::ActionType::Move, "/tmp/sdo_sim_dest"));

    auto result = engine.applyRules(fi);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.action, SDO::ActionType::Move);
    // File must still exist (simulate mode)
    EXPECT_TRUE(fs::exists(src));

    std::error_code ec; fs::remove(src, ec);
    cfg.simulateMode = false;
}

TEST(test_cleanup_suggestions_contain_large_files) {
    auto& engine = SDO::OrganizerEngine::instance();
    auto& cfg    = SDO::ConfigManager::instance().config();
    cfg.largeSizeThreshold = 500ULL * 1024 * 1024;

    SDO::FileInfo fi = makeFile("/tmp/sdo_huge.iso", "iso",
                                 SDO::FileCategory::Archives,
                                 600ULL * 1024 * 1024);
    fi.status = SDO::FileStatus::Large;
    SDO::Database::instance().upsertFile(fi);

    auto suggestions = engine.getCleanupSuggestions();
    bool found = false;
    for (const auto& s : suggestions)
        if (s.path == "/tmp/sdo_huge.iso") { found = true; break; }
    EXPECT_TRUE(found);
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "\n=== OrganizerEngine Tests ===\n\n";
    SDO::Logger::instance().init("", SDO::LogLevel::Critical);
    SDO::ConfigManager::instance().load(SDO::ConfigManager::defaultConfigPath());

    // Enable simulate mode for all tests to avoid real FS mutations
    SDO::ConfigManager::instance().config().simulateMode = true;
    SDO::ConfigManager::instance().config().autoOrganize = false;

    std::error_code ec; fs::remove(TEST_DB, ec);
    SDO::Database::instance().open(TEST_DB);

    RUN(test_rule_matches_extension_eq);
    RUN(test_rule_no_match_different_ext);
    RUN(test_rule_matches_name_contains);
    RUN(test_rule_or_logic);
    RUN(test_rule_and_logic_both_must_match);
    RUN(test_rule_and_logic_partial_no_match);
    RUN(test_rule_disabled_is_skipped);
    RUN(test_rule_priority_order);
    RUN(test_regex_condition);
    RUN(test_age_condition);
    RUN(test_simulate_mode_no_actual_move);
    RUN(test_cleanup_suggestions_contain_large_files);

    SDO::Database::instance().close();
    fs::remove(TEST_DB, ec);

    std::cout << "\n─────────────────────────────────\n";
    std::cout << "Passed: " << g_passed << "  Failed: " << g_failed << "\n";
    return g_failed == 0 ? 0 : 1;
}
