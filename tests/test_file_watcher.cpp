/**
 * tests/test_file_watcher.cpp
 *
 * Integration tests for FileWatcher: inotify event delivery for create,
 * modify, delete, and rename events.  Uses real temporary directories and
 * real file-system operations; events are collected with a timed wait.
 */

#include "file_watcher.hpp"
#include "logger.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <type_traits>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

// ─── Test framework ────────────────────────────────────────────────────────
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
#define EXPECT_GE(a, b)  do { \
    if ((a) >= (b)) { ++g_passed; } \
    else { ++g_failed; \
      std::cerr << "[FAIL] " << __FILE__ << ":" << __LINE__ << "  "; \
      printVal(std::cerr, a); std::cerr << " not >= "; printVal(std::cerr, b); \
      std::cerr << "\n"; } \
} while(0)

#define TEST(name) static void name()
#define RUN(name)  do { std::cout << "  " #name " ... "; name(); std::cout << "ok\n"; } while(0)

// ─── Event collector ──────────────────────────────────────────────────────────
struct EventCollector {
    std::mutex                          mtx;
    std::condition_variable             cv;
    std::vector<SDO::WatchNotification> events;

    void clear() {
        std::lock_guard<std::mutex> lk(mtx);
        events.clear();
    }

    // Wait up to `ms` milliseconds for at least `count` events matching predicate
    template<typename Pred>
    bool waitFor(int count, int ms, Pred pred) {
        std::unique_lock<std::mutex> lk(mtx);
        return cv.wait_for(lk, std::chrono::milliseconds(ms), [&]{
            int n = 0;
            for (const auto& e : events) if (pred(e)) ++n;
            return n >= count;
        });
    }

    bool waitForEvent(SDO::WatchEvent type, const std::string& pathSuffix, int ms = 2000) {
        return waitFor(1, ms, [&](const SDO::WatchNotification& n){
            return n.event == type &&
                   n.path.size() >= pathSuffix.size() &&
                   n.path.substr(n.path.size() - pathSuffix.size()) == pathSuffix;
        });
    }

    SDO::WatchCallback makeCallback() {
        return [this](const SDO::WatchNotification& n){
            std::lock_guard<std::mutex> lk(mtx);
            events.push_back(n);
            cv.notify_all();
        };
    }
};

// ─── Scoped temp directory ────────────────────────────────────────────────────
struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() /
               ("sdo_fw_test_" + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    fs::path file(const std::string& name) const { return path / name; }
};

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST(test_add_watch_valid_path) {
    TempDir dir;
    SDO::FileWatcher fw;
    EXPECT_TRUE(fw.addWatch(dir.path.string()));
}

TEST(test_add_watch_missing_path) {
    SDO::FileWatcher fw;
    EXPECT_FALSE(fw.addWatch("/nonexistent/path/xyz_sdo_test"));
}

TEST(test_start_stop) {
    TempDir dir;
    SDO::FileWatcher fw;
    fw.addWatch(dir.path.string());
    EXPECT_TRUE(fw.start());
    EXPECT_TRUE(fw.isRunning());
    fw.stop();
    EXPECT_FALSE(fw.isRunning());
}

TEST(test_double_start_is_safe) {
    TempDir dir;
    SDO::FileWatcher fw;
    fw.addWatch(dir.path.string());
    EXPECT_TRUE(fw.start());
    EXPECT_TRUE(fw.start()); // must not crash or return false
    fw.stop();
}

TEST(test_watched_paths_list) {
    TempDir d1, d2;
    SDO::FileWatcher fw;
    fw.addWatch(d1.path.string());
    fw.addWatch(d2.path.string());
    auto paths = fw.watchedPaths();
    EXPECT_GE(paths.size(), size_t(2));
    bool found1 = false, found2 = false;
    for (const auto& p : paths) {
        if (p == d1.path.string()) found1 = true;
        if (p == d2.path.string()) found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

TEST(test_detects_file_create) {
    TempDir dir;
    EventCollector col;
    SDO::FileWatcher fw;
    fw.setCallback(col.makeCallback());
    fw.addWatch(dir.path.string());
    fw.start();

    // Write a file after watcher is running
    std::this_thread::sleep_for(50ms);
    { std::ofstream f(dir.file("created.txt")); f << "hello\n"; }

    bool got = col.waitForEvent(SDO::WatchEvent::Created, "created.txt", 3000);
    fw.stop();
    EXPECT_TRUE(got);
}

TEST(test_detects_file_modify) {
    TempDir dir;
    // Pre-create the file
    { std::ofstream f(dir.file("modify.txt")); f << "initial\n"; }

    EventCollector col;
    SDO::FileWatcher fw;
    fw.setCallback(col.makeCallback());
    fw.addWatch(dir.path.string());
    fw.start();
    std::this_thread::sleep_for(50ms);

    // Modify it
    { std::ofstream f(dir.file("modify.txt"), std::ios::app); f << "appended\n"; }

    bool got = col.waitForEvent(SDO::WatchEvent::Modified, "modify.txt", 3000);
    fw.stop();
    EXPECT_TRUE(got);
}

TEST(test_detects_file_delete) {
    TempDir dir;
    fs::path target = dir.file("delete_me.txt");
    { std::ofstream f(target); f << "bye\n"; }

    EventCollector col;
    SDO::FileWatcher fw;
    fw.setCallback(col.makeCallback());
    fw.addWatch(dir.path.string());
    fw.start();
    std::this_thread::sleep_for(50ms);

    std::error_code ec;
    fs::remove(target, ec);

    bool got = col.waitForEvent(SDO::WatchEvent::Deleted, "delete_me.txt", 3000);
    fw.stop();
    EXPECT_TRUE(got);
}

TEST(test_detects_file_rename) {
    TempDir dir;
    fs::path src = dir.file("before.txt");
    fs::path dst = dir.file("after.txt");
    { std::ofstream f(src); f << "rename test\n"; }

    EventCollector col;
    SDO::FileWatcher fw;
    fw.setCallback(col.makeCallback());
    fw.addWatch(dir.path.string());
    fw.start();
    std::this_thread::sleep_for(50ms);

    std::error_code ec;
    fs::rename(src, dst, ec);

    // We should receive either a Renamed or MovedIn/MovedOut pair
    bool got = col.waitFor(1, 3000, [&](const SDO::WatchNotification& n){
        return n.event == SDO::WatchEvent::Renamed ||
               n.event == SDO::WatchEvent::MovedIn ||
               n.event == SDO::WatchEvent::MovedOut;
    });
    fw.stop();
    EXPECT_TRUE(got);
}

TEST(test_remove_watch) {
    TempDir dir;
    SDO::FileWatcher fw;
    fw.addWatch(dir.path.string());
    EXPECT_TRUE(fw.removeWatch(dir.path.string()));
    EXPECT_FALSE(fw.removeWatch(dir.path.string())); // already removed
    auto paths = fw.watchedPaths();
    bool found = false;
    for (const auto& p : paths)
        if (p == dir.path.string()) { found = true; break; }
    EXPECT_FALSE(found);
}

TEST(test_clear_watches) {
    TempDir d1, d2;
    SDO::FileWatcher fw;
    fw.addWatch(d1.path.string());
    fw.addWatch(d2.path.string());
    fw.clearWatches();
    EXPECT_EQ(fw.watchedPaths().size(), size_t(0));
}

TEST(test_multiple_files_detected) {
    TempDir dir;
    EventCollector col;
    SDO::FileWatcher fw;
    fw.setCallback(col.makeCallback());
    fw.addWatch(dir.path.string());
    fw.start();
    std::this_thread::sleep_for(50ms);

    for (int i = 0; i < 5; ++i) {
        std::ofstream f(dir.file("multi_" + std::to_string(i) + ".txt"));
        f << "content " << i << "\n";
        std::this_thread::sleep_for(10ms);
    }

    bool got = col.waitFor(5, 5000, [](const SDO::WatchNotification& n){
        return n.event == SDO::WatchEvent::Created ||
               n.event == SDO::WatchEvent::Modified;
    });
    fw.stop();
    EXPECT_TRUE(got);
}

TEST(test_recursive_watch_detects_subdir_file) {
    TempDir dir;
    fs::path subdir = dir.path / "subdir";
    fs::create_directories(subdir);

    EventCollector col;
    SDO::FileWatcher fw;
    fw.setCallback(col.makeCallback());
    fw.addWatch(dir.path.string(), /*recursive=*/true);
    fw.start();
    std::this_thread::sleep_for(100ms);

    { std::ofstream f(subdir / "deep_file.txt"); f << "deep\n"; }

    bool got = col.waitForEvent(SDO::WatchEvent::Created, "deep_file.txt", 4000);
    fw.stop();
    EXPECT_TRUE(got);
}

TEST(test_callback_not_called_after_stop) {
    TempDir dir;
    EventCollector col;
    SDO::FileWatcher fw;
    fw.setCallback(col.makeCallback());
    fw.addWatch(dir.path.string());
    fw.start();
    std::this_thread::sleep_for(50ms);
    fw.stop();

    size_t eventsBefore = [&]{
        std::lock_guard<std::mutex> lk(col.mtx);
        return col.events.size();
    }();

    // Write after stop — should not trigger callback
    { std::ofstream f(dir.file("after_stop.txt")); f << "no event\n"; }
    std::this_thread::sleep_for(300ms);

    size_t eventsAfter = [&]{
        std::lock_guard<std::mutex> lk(col.mtx);
        return col.events.size();
    }();

    EXPECT_EQ(eventsBefore, eventsAfter);
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main() {
    std::cout << "\n=== FileWatcher Tests ===\n\n";
    SDO::Logger::instance().init("", SDO::LogLevel::Critical);

    RUN(test_add_watch_valid_path);
    RUN(test_add_watch_missing_path);
    RUN(test_start_stop);
    RUN(test_double_start_is_safe);
    RUN(test_watched_paths_list);
    RUN(test_detects_file_create);
    RUN(test_detects_file_modify);
    RUN(test_detects_file_delete);
    RUN(test_detects_file_rename);
    RUN(test_remove_watch);
    RUN(test_clear_watches);
    RUN(test_multiple_files_detected);
    RUN(test_recursive_watch_detects_subdir_file);
    RUN(test_callback_not_called_after_stop);

    std::cout << "\n─────────────────────────────────\n";
    std::cout << "Passed: " << g_passed << "  Failed: " << g_failed << "\n";
    return g_failed == 0 ? 0 : 1;
}
