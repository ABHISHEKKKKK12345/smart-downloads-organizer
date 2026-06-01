/**
 * tests/test_config_manager.cpp
 *
 * Tests for ConfigManager: load/save, defaults, rule add/update/delete,
 * watch path management, and JSON round-trip correctness.
 */

#include "config_manager.hpp"
#include "logger.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cassert>

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

static const fs::path TEST_CFG = fs::temp_directory_path() / "sdo_test_config.json";

// ─── Helpers ─────────────────────────────────────────────────────────────────
static SDO::OrganizerRule makeRule(const std::string& id, const std::string& name,
                                    bool enabled = true, int priority = 10) {
    SDO::OrganizerRule r;
    r.id       = id;
    r.name     = name;
    r.enabled  = enabled;
    r.priority = priority;
    r.conditionLogic = "OR";
    SDO::RuleCondition c;
    c.field = "extension"; c.op = "eq"; c.value = "pdf";
    r.conditions.push_back(c);
    r.action.type            = SDO::ActionType::Move;
    r.action.targetDirectory = "/tmp/test_dest";
    return r;
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST(test_default_config_has_watch_path) {
    auto cfg = SDO::ConfigManager::defaultConfig();
    EXPECT_FALSE(cfg.watchPaths.empty());
}

TEST(test_default_config_has_builtin_rules) {
    auto cfg = SDO::ConfigManager::defaultConfig();
    EXPECT_GE(cfg.rules.size(), size_t(1));
}

TEST(test_default_category_meta_count) {
    auto metas = SDO::ConfigManager::defaultCategoryMeta();
    // Should have entries for all FileCategory values
    EXPECT_GE(metas.size(), size_t(10));
    // Each must have a non-empty name
    for (const auto& m : metas) EXPECT_FALSE(m.name.empty());
}

TEST(test_load_missing_file_uses_defaults) {
    std::error_code ec; fs::remove(TEST_CFG, ec);
    auto& mgr = SDO::ConfigManager::instance();
    bool ok = mgr.load(TEST_CFG.string());
    EXPECT_TRUE(ok); // missing file → defaults, returns true
    EXPECT_FALSE(mgr.config().watchPaths.empty());
}

TEST(test_save_and_reload) {
    auto& mgr = SDO::ConfigManager::instance();
    mgr.load(TEST_CFG.string());

    // Modify a value
    mgr.config().oldFileAgeDays   = 42;
    mgr.config().largeSizeThreshold = 999ULL * 1024 * 1024;
    mgr.config().autoOrganize     = true;
    mgr.config().simulateMode     = true;

    EXPECT_TRUE(mgr.save());
    EXPECT_TRUE(fs::exists(TEST_CFG));

    // Reload and verify
    EXPECT_TRUE(mgr.load(TEST_CFG.string()));
    EXPECT_EQ(mgr.config().oldFileAgeDays,    42);
    EXPECT_EQ(mgr.config().largeSizeThreshold, 999ULL * 1024 * 1024);
    EXPECT_TRUE(mgr.config().autoOrganize);
    EXPECT_TRUE(mgr.config().simulateMode);
}

TEST(test_save_as) {
    auto& mgr = SDO::ConfigManager::instance();
    mgr.load(TEST_CFG.string());
    fs::path alt = fs::temp_directory_path() / "sdo_alt_config.json";
    EXPECT_TRUE(mgr.saveAs(alt.string()));
    EXPECT_TRUE(fs::exists(alt));
    std::error_code ec; fs::remove(alt, ec);
}

TEST(test_add_rule) {
    auto& mgr = SDO::ConfigManager::instance();
    mgr.load(TEST_CFG.string());
    size_t before = mgr.config().rules.size();

    auto r = makeRule("unit-test-add", "Unit Test Add Rule");
    EXPECT_TRUE(mgr.addRule(r));
    EXPECT_EQ(mgr.config().rules.size(), before + 1);

    // Adding same id again should fail
    EXPECT_FALSE(mgr.addRule(r));
}

TEST(test_update_rule) {
    auto& mgr = SDO::ConfigManager::instance();
    auto r = makeRule("unit-test-update", "Before Update");
    mgr.addRule(r);

    r.name    = "After Update";
    r.enabled = false;
    EXPECT_TRUE(mgr.updateRule(r));

    // Find it
    bool found = false;
    for (const auto& rule : mgr.config().rules) {
        if (rule.id == "unit-test-update") {
            EXPECT_EQ(rule.name, std::string("After Update"));
            EXPECT_FALSE(rule.enabled);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(test_update_nonexistent_rule) {
    auto& mgr = SDO::ConfigManager::instance();
    auto r = makeRule("nonexistent-id-xyz", "Ghost Rule");
    EXPECT_FALSE(mgr.updateRule(r));
}

TEST(test_delete_rule) {
    auto& mgr = SDO::ConfigManager::instance();
    auto r = makeRule("unit-test-delete", "Delete Me");
    mgr.addRule(r);
    size_t before = mgr.config().rules.size();
    EXPECT_TRUE(mgr.deleteRule("unit-test-delete"));
    EXPECT_EQ(mgr.config().rules.size(), before - 1);

    // Deleting again should fail
    EXPECT_FALSE(mgr.deleteRule("unit-test-delete"));
}

TEST(test_reorder_rules) {
    auto& mgr = SDO::ConfigManager::instance();
    mgr.config().rules.clear();

    mgr.addRule(makeRule("r-A", "Alpha",   true, 10));
    mgr.addRule(makeRule("r-B", "Beta",    true, 20));
    mgr.addRule(makeRule("r-C", "Charlie", true, 30));

    // Reorder to C, A, B
    EXPECT_TRUE(mgr.reorderRules({"r-C", "r-A", "r-B"}));
    EXPECT_EQ(mgr.config().rules[0].id, std::string("r-C"));
    EXPECT_EQ(mgr.config().rules[1].id, std::string("r-A"));
    EXPECT_EQ(mgr.config().rules[2].id, std::string("r-B"));
}

TEST(test_reorder_wrong_ids_fails) {
    auto& mgr = SDO::ConfigManager::instance();
    // Pass a non-matching ID set
    EXPECT_FALSE(mgr.reorderRules({"r-A", "r-B", "DOES-NOT-EXIST"}));
}

TEST(test_add_watch_path) {
    auto& mgr = SDO::ConfigManager::instance();
    size_t before = mgr.config().watchPaths.size();
    EXPECT_TRUE(mgr.addWatchPath("/tmp/sdo_watch_test"));
    EXPECT_EQ(mgr.config().watchPaths.size(), before + 1);

    // Adding same path again should fail
    EXPECT_FALSE(mgr.addWatchPath("/tmp/sdo_watch_test"));
}

TEST(test_remove_watch_path) {
    auto& mgr = SDO::ConfigManager::instance();
    mgr.addWatchPath("/tmp/sdo_watch_remove");
    size_t before = mgr.config().watchPaths.size();
    EXPECT_TRUE(mgr.removeWatchPath("/tmp/sdo_watch_remove"));
    EXPECT_EQ(mgr.config().watchPaths.size(), before - 1);

    EXPECT_FALSE(mgr.removeWatchPath("/tmp/sdo_watch_remove")); // already gone
}

TEST(test_default_paths_non_empty) {
    EXPECT_FALSE(SDO::ConfigManager::defaultConfigPath().empty());
    EXPECT_FALSE(SDO::ConfigManager::defaultDatabasePath().empty());
    EXPECT_FALSE(SDO::ConfigManager::defaultLogPath().empty());
}

TEST(test_config_booleans_persist) {
    auto& mgr = SDO::ConfigManager::instance();
    mgr.load(TEST_CFG.string());
    mgr.config().enableDuplicateDetect = false;
    mgr.config().moveToTrash           = false;
    mgr.config().enableNotifications   = false;
    mgr.config().watchRecursive        = true;
    mgr.save();
    mgr.load(TEST_CFG.string());
    EXPECT_FALSE(mgr.config().enableDuplicateDetect);
    EXPECT_FALSE(mgr.config().moveToTrash);
    EXPECT_FALSE(mgr.config().enableNotifications);
    EXPECT_TRUE(mgr.config().watchRecursive);
}

TEST(test_change_callback_fires) {
    auto& mgr = SDO::ConfigManager::instance();
    bool fired = false;
    mgr.setChangeCallback([&]{ fired = true; });
    mgr.notifyChanged();
    EXPECT_TRUE(fired);
    mgr.setChangeCallback(nullptr); // reset
}

TEST(test_saved_file_is_valid_json_like) {
    auto& mgr = SDO::ConfigManager::instance();
    mgr.load(TEST_CFG.string());
    mgr.save();
    std::ifstream f(TEST_CFG.string());
    EXPECT_TRUE(f.is_open());
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    // Basic sanity: must be non-empty, start with '{' and end with '}'
    EXPECT_FALSE(content.empty());
    size_t first = content.find_first_not_of(" \t\n\r");
    EXPECT_TRUE(first != std::string::npos && content[first] == '{');
    size_t last = content.find_last_not_of(" \t\n\r");
    EXPECT_TRUE(last != std::string::npos && content[last] == '}');
}

// ─── Regression tests for targeted fixes ─────────────────────────────────────

// C-1: watchPaths must survive a save/load round-trip
TEST(test_watchpaths_persist_across_reload) {
    auto& mgr = SDO::ConfigManager::instance();
    mgr.load(TEST_CFG.string());

    // Add two custom paths
    mgr.config().watchPaths = { "/tmp/sdo_watch_a", "/tmp/sdo_watch_b" };
    EXPECT_TRUE(mgr.save());

    // Reload from disk and verify both paths are restored
    EXPECT_TRUE(mgr.load(TEST_CFG.string()));
    EXPECT_EQ(mgr.config().watchPaths.size(), size_t(2));
    bool foundA = false, foundB = false;
    for (const auto& p : mgr.config().watchPaths) {
        if (p == "/tmp/sdo_watch_a") foundA = true;
        if (p == "/tmp/sdo_watch_b") foundB = true;
    }
    EXPECT_TRUE(foundA);
    EXPECT_TRUE(foundB);
}

// C-1: watchPaths with special characters (spaces, unicode) round-trip correctly
TEST(test_watchpaths_special_chars_persist) {
    auto& mgr = SDO::ConfigManager::instance();
    mgr.load(TEST_CFG.string());

    mgr.config().watchPaths = { "/home/user/My Downloads", "/tmp/sdo_test" };
    EXPECT_TRUE(mgr.save());
    EXPECT_TRUE(mgr.load(TEST_CFG.string()));

    bool found = false;
    for (const auto& p : mgr.config().watchPaths)
        if (p == "/home/user/My Downloads") { found = true; break; }
    EXPECT_TRUE(found);
}

// C-1: Empty watchPaths array in JSON keeps defaults (not an empty vector)
TEST(test_watchpaths_empty_json_keeps_defaults) {
    auto& mgr = SDO::ConfigManager::instance();
    // Write a config with an empty watchPaths array
    std::ofstream f(TEST_CFG.string());
    f << "{\"watchPaths\": []}\n";
    f.close();
    EXPECT_TRUE(mgr.load(TEST_CFG.string()));
    // empty array in JSON → keep defaults (Downloads is always present)
    EXPECT_FALSE(mgr.config().watchPaths.empty());
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "\n=== ConfigManager Tests ===\n\n";
    SDO::Logger::instance().init("", SDO::LogLevel::Critical);

    RUN(test_default_config_has_watch_path);
    RUN(test_default_config_has_builtin_rules);
    RUN(test_default_category_meta_count);
    RUN(test_load_missing_file_uses_defaults);
    RUN(test_save_and_reload);
    RUN(test_save_as);
    RUN(test_add_rule);
    RUN(test_update_rule);
    RUN(test_update_nonexistent_rule);
    RUN(test_delete_rule);
    RUN(test_reorder_rules);
    RUN(test_reorder_wrong_ids_fails);
    RUN(test_add_watch_path);
    RUN(test_remove_watch_path);
    RUN(test_default_paths_non_empty);
    RUN(test_config_booleans_persist);
    RUN(test_change_callback_fires);
    RUN(test_saved_file_is_valid_json_like);
    RUN(test_watchpaths_persist_across_reload);
    RUN(test_watchpaths_special_chars_persist);
    RUN(test_watchpaths_empty_json_keeps_defaults);

    std::error_code ec; fs::remove(TEST_CFG, ec);

    std::cout << "\n─────────────────────────────────\n";
    std::cout << "Passed: " << g_passed << "  Failed: " << g_failed << "\n";
    return g_failed == 0 ? 0 : 1;
}
