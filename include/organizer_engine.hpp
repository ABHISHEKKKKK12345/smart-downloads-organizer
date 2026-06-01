#pragma once

#include "types.hpp"
#include "file_watcher.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <set>

namespace SDO {

struct OrganizeResult {
    bool        success     = false;
    std::string sourcePath;
    std::string destPath;
    std::string ruleName;
    std::string error;
    ActionType  action      = ActionType::Skip;
    uint64_t    sizeBytes   = 0;
};

class OrganizerEngine {
public:
    static OrganizerEngine& instance();

    void setFileDetectedCallback(FileDetectedCb cb);
    void setStatsUpdatedCallback(StatsUpdatedCb cb);
    void setNotificationCallback(NotificationCb cb);

    // Scanning
    void scanDirectory(const std::string& path, bool recurse = false);
    void scanAllWatchPaths();

    // Start/stop background processing
    bool startWatching();
    void stopWatching();
    bool isWatching() const;

    // Rule application
    OrganizeResult applyRules(const FileInfo& fi);
    bool           applyRulesToAll();

    // Duplicate detection
    void runDuplicateScan();
    uint64_t getDuplicateSavings();

    // Undo
    bool undoLastAction();
    bool undoAction(const std::string& undoId);

    // Queue management
    void enqueueFile(const std::string& path);
    size_t queueSize() const;

    // Cleanup suggestions
    std::vector<FileInfo> getCleanupSuggestions();

    // Force rescan
    void rescanAll();

private:
    OrganizerEngine();
    OrganizerEngine(const OrganizerEngine&) = delete;
    OrganizerEngine& operator=(const OrganizerEngine&) = delete;

    void processLoop();
    void onFileEvent(const WatchNotification& notif);
    void processFile(const std::string& path);
    bool matchesRule(const FileInfo& fi, const OrganizerRule& rule) const;
    bool matchesCondition(const FileInfo& fi, const RuleCondition& cond) const;
    std::string resolvePattern(const std::string& pattern, const FileInfo& fi) const;
    std::string buildDestPath(const RuleAction& action, const FileInfo& fi) const;
    bool executeAction(const RuleAction& action, const FileInfo& fi, OrganizeResult& result);
    bool moveFile(const std::string& src, const std::string& dst);
    bool copyFile(const std::string& src, const std::string& dst);
    bool deleteFile(const std::string& path);
    std::string makeUniquePath(const std::string& path) const;
    void saveUndoEntry(const OrganizeResult& result);
    void emitNotification(const std::string& title, const std::string& msg, LogLevel level);

    mutable std::mutex              m_mutex;
    std::condition_variable         m_cv;
    std::queue<std::string>         m_fileQueue;
    std::thread                     m_workerThread;
    std::atomic<bool>               m_running{false};
    std::atomic<bool>               m_scanning{false};

    std::unique_ptr<FileWatcher>    m_watcher;

    FileDetectedCb                  m_fileDetectedCb;
    StatsUpdatedCb                  m_statsUpdatedCb;
    NotificationCb                  m_notificationCb;

    std::set<std::string>           m_processingSet; // dedup in-flight
};

} // namespace SDO
