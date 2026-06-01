#pragma once

#include "types.hpp"
#include <sys/inotify.h>
#include <climits>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <map>
#include <set>
#include <memory>

namespace SDO {

enum class WatchEvent {
    Created,
    Modified,
    Deleted,
    Renamed,
    MovedIn,
    MovedOut
};

struct WatchNotification {
    WatchEvent  event;
    std::string path;
    std::string oldPath;  // for Renamed
    bool        isDir    = false;
};

using WatchCallback = std::function<void(const WatchNotification&)>;

class FileWatcher {
public:
    explicit FileWatcher();
    ~FileWatcher();

    // Non-copyable
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    bool addWatch(const std::string& path, bool recursive = false);
    bool removeWatch(const std::string& path);
    void clearWatches();

    void setCallback(WatchCallback cb);

    bool start();
    void stop();
    bool isRunning() const;

    std::vector<std::string> watchedPaths() const;

private:
    void watchLoop();
    bool initInotify();
    void processEvent(const inotify_event* ev);
    void processEvents();
    void cleanupInotify();
    bool addInotifyWatch(const std::string& path);
    void addRecursive(const std::string& path);

    int                                  m_inotifyFd  = -1;
    std::map<int, std::string>           m_wdToPath;   // watch descriptor → path
    std::map<std::string, int>           m_pathToWd;
    std::set<std::string>                m_watchedRoots;
    std::set<std::string>                m_recursivePaths;

    mutable std::mutex                   m_mutex;
    std::thread                          m_thread;
    std::atomic<bool>                    m_running{false};
    WatchCallback                        m_callback;

    // For rename tracking (inotify cookie)
    struct RenameInfo { std::string oldPath; };
    std::map<uint32_t, RenameInfo>       m_renames;
};

} // namespace SDO
