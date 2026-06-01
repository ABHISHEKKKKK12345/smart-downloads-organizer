#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <functional>
#include <optional>
#include <cstdint>

namespace SDO {

// ─── Version ─────────────────────────────────────────────────────────────────
constexpr const char* APP_NAME    = "Smart Downloads Organizer";
constexpr const char* APP_VERSION = "2.0.0";
constexpr const char* APP_ID      = "com.sdo.organizer";

// ─── File Categories ──────────────────────────────────────────────────────────
enum class FileCategory : uint8_t {
    Images      = 0,
    Videos      = 1,
    Audio       = 2,
    Documents   = 3,
    Archives    = 4,
    Code        = 5,
    Executables = 6,
    Fonts       = 7,
    Data        = 8,
    Ebooks      = 9,
    Torrents    = 10,
    Unknown     = 11,
    COUNT       = 12
};

// ─── File Status ──────────────────────────────────────────────────────────────
enum class FileStatus : uint8_t {
    Normal      = 0,
    Duplicate   = 1,
    Large       = 2,
    Old         = 3,
    Orphaned    = 4,
    Temporary   = 5
};

// ─── Action Types ─────────────────────────────────────────────────────────────
enum class ActionType : uint8_t {
    Move        = 0,
    Copy        = 1,
    Delete      = 2,
    Rename      = 3,
    Compress    = 4,
    Skip        = 5
};

// ─── Log Levels ───────────────────────────────────────────────────────────────
enum class LogLevel : uint8_t {
    Debug   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3,
    Critical= 4
};

// ─── File Info ────────────────────────────────────────────────────────────────
struct FileInfo {
    std::string   path;
    std::string   filename;
    std::string   extension;
    std::string   mimeType;
    std::string   sha256Hash;
    std::string   md5Hash;
    uint64_t      sizeBytes      = 0;
    FileCategory  category       = FileCategory::Unknown;
    FileStatus    status         = FileStatus::Normal;
    bool          isProcessed    = false;
    bool          isDuplicate    = false;
    std::string   duplicateOf;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point modifiedAt;
    std::chrono::system_clock::time_point accessedAt;
    std::chrono::system_clock::time_point detectedAt;

    // Computed
    std::string sizeHuman() const;
    std::string categoryName() const;
    std::string statusName() const;
    int64_t     ageInDays() const;
};

// ─── Rule Action ─────────────────────────────────────────────────────────────
struct RuleAction {
    ActionType  type            = ActionType::Move;
    std::string targetDirectory;
    std::string renamePattern;   // Supports {name}, {date}, {ext}, {category}
    bool        createSubfolders= true;
    bool        preserveMetadata= true;
};

// ─── Rule Condition ───────────────────────────────────────────────────────────
struct RuleCondition {
    std::string field;           // "extension", "size", "age", "name", "category"
    std::string op;              // "eq", "ne", "gt", "lt", "contains", "regex"
    std::string value;
    bool        caseSensitive   = false;
};

// ─── Organizer Rule ───────────────────────────────────────────────────────────
struct OrganizerRule {
    std::string                  id;
    std::string                  name;
    std::string                  description;
    bool                         enabled     = true;
    int                          priority    = 0;
    std::vector<RuleCondition>   conditions;
    std::string                  conditionLogic; // "AND" or "OR"
    RuleAction                   action;
    uint64_t                     matchCount  = 0;
    std::chrono::system_clock::time_point lastMatched;
};

// ─── Config ───────────────────────────────────────────────────────────────────
struct AppConfig {
    // Watched paths
    std::vector<std::string>  watchPaths;
    std::string               configPath;
    std::string               databasePath;
    std::string               logPath;

    // Behavior
    bool    autoOrganize        = false;
    bool    watchRecursive      = false;
    bool    enableDuplicateDetect = true;
    bool    enableNotifications = true;
    bool    moveToTrash         = true;    // vs permanent delete
    bool    simulateMode        = false;   // dry run
    bool    startMinimized      = false;
    bool    startOnBoot         = false;

    // Thresholds
    uint64_t largeSizeThreshold = 500ULL * 1024 * 1024;  // 500MB
    int      oldFileAgeDays     = 30;
    int      scanIntervalSecs   = 5;
    int      maxUndoHistory     = 100;

    // Auto-cleanup
    bool    suggestCleanup      = true;
    bool    autoCleanTemp       = false;

    // Organizer rules
    std::vector<OrganizerRule>  rules;

    // UI
    int     windowWidth         = 1280;
    int     windowHeight        = 800;
    bool    darkMode            = true;
    std::string  defaultView    = "dashboard";
};

// ─── Statistics ───────────────────────────────────────────────────────────────
struct Statistics {
    uint64_t totalFiles         = 0;
    uint64_t totalSize          = 0;
    uint64_t duplicateFiles     = 0;
    uint64_t duplicateSize      = 0;
    uint64_t largeFiles         = 0;
    uint64_t oldFiles           = 0;
    uint64_t organizedToday     = 0;
    uint64_t organizedTotal     = 0;
    uint64_t spaceSaved         = 0;
    std::map<FileCategory, uint64_t>  countByCategory;
    std::map<FileCategory, uint64_t>  sizeByCategory;
    std::chrono::system_clock::time_point lastScanTime;
    double  scanDurationMs      = 0.0;
};

// ─── Log Entry ───────────────────────────────────────────────────────────────
struct LogEntry {
    LogLevel    level;
    std::string message;
    std::string detail;
    std::chrono::system_clock::time_point timestamp;
    std::string filePath;
};

// ─── Undo Entry ───────────────────────────────────────────────────────────────
struct UndoEntry {
    std::string  id;
    ActionType   action;
    std::string  sourcePath;
    std::string  destPath;
    std::string  description;
    std::chrono::system_clock::time_point performedAt;
    bool         canUndo = true;
};

// ─── Notification ─────────────────────────────────────────────────────────────
struct Notification {
    std::string  title;
    std::string  message;
    LogLevel     level    = LogLevel::Info;
    std::chrono::system_clock::time_point time;
    std::function<void()> action;
};

// ─── Category Metadata ───────────────────────────────────────────────────────
struct CategoryMeta {
    std::string              name;
    std::string              iconName;
    std::string              defaultFolder;
    std::vector<std::string> extensions;
    std::string              color;  // hex
};

// ─── Callbacks / Signals ──────────────────────────────────────────────────────
using FileDetectedCb   = std::function<void(const FileInfo&)>;
using FileChangedCb    = std::function<void(const FileInfo&)>;
using StatsUpdatedCb   = std::function<void(const Statistics&)>;
using LogCb            = std::function<void(const LogEntry&)>;
using NotificationCb   = std::function<void(const Notification&)>;

} // namespace SDO
