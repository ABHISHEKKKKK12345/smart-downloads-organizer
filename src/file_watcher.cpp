#include "file_watcher.hpp"
#include "logger.hpp"
#include <sys/select.h>
#include <unistd.h>
#include <filesystem>
#include <cstring>
#include <stdexcept>
#include <climits>

namespace fs = std::filesystem;

namespace SDO {

constexpr uint32_t WATCH_FLAGS =
    IN_CREATE | IN_CLOSE_WRITE | IN_DELETE |
    IN_MOVED_FROM | IN_MOVED_TO | IN_MODIFY;

// ─── Constructor / Destructor ─────────────────────────────────────────────────
FileWatcher::FileWatcher() : m_inotifyFd(-1) {}

FileWatcher::~FileWatcher() {
    stop();
    cleanupInotify();
}

// ─── inotify lifecycle ────────────────────────────────────────────────────────
bool FileWatcher::initInotify() {
    if (m_inotifyFd >= 0) return true;
    m_inotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (m_inotifyFd < 0) {
        LOG_ERROR("inotify_init1 failed", std::string(strerror(errno)));
        return false;
    }
    return true;
}

void FileWatcher::cleanupInotify() {
    if (m_inotifyFd >= 0) {
        close(m_inotifyFd);
        m_inotifyFd = -1;
    }
    m_wdToPath.clear();
    m_pathToWd.clear();
}

// ─── Watch management ─────────────────────────────────────────────────────────
bool FileWatcher::addInotifyWatch(const std::string& path) {
    // Avoid double-watching
    if (m_pathToWd.count(path)) return true;

    int wd = inotify_add_watch(m_inotifyFd, path.c_str(), WATCH_FLAGS);
    if (wd < 0) {
        if (errno != ENOENT && errno != EACCES)
            LOG_WARN("inotify_add_watch failed", strerror(errno), path);
        return false;
    }
    m_wdToPath[wd]   = path;
    m_pathToWd[path] = wd;
    return true;
}

void FileWatcher::addRecursive(const std::string& rootPath) {
    addInotifyWatch(rootPath);
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(
            rootPath,
            fs::directory_options::skip_permission_denied, ec)) {
        if (ec) { ec.clear(); continue; }
        if (entry.is_directory(ec)) {
            addInotifyWatch(entry.path().string());
        }
    }
}

bool FileWatcher::addWatch(const std::string& path, bool recursive) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!initInotify()) return false;

    std::error_code ec;
    if (!fs::exists(path, ec) || ec) {
        LOG_WARN("Watch path does not exist", "", path);
        return false;
    }

    m_watchedRoots.insert(path);
    if (recursive) {
        m_recursivePaths.insert(path);
        addRecursive(path);
    } else {
        addInotifyWatch(path);
    }
    LOG_INFO("Watching: " + path + (recursive ? " (recursive)" : ""));
    return true;
}

bool FileWatcher::removeWatch(const std::string& path) {
    std::lock_guard<std::mutex> lk(m_mutex);
    auto it = m_pathToWd.find(path);
    if (it == m_pathToWd.end()) return false;
    inotify_rm_watch(m_inotifyFd, it->second);
    m_wdToPath.erase(it->second);
    m_pathToWd.erase(it);
    m_watchedRoots.erase(path);
    m_recursivePaths.erase(path);
    return true;
}

void FileWatcher::clearWatches() {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto& [wd, _] : m_wdToPath)
        inotify_rm_watch(m_inotifyFd, wd);
    m_wdToPath.clear();
    m_pathToWd.clear();
    m_watchedRoots.clear();
    m_recursivePaths.clear();
}

void FileWatcher::setCallback(WatchCallback cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_callback = std::move(cb);
}

// ─── Start / Stop ─────────────────────────────────────────────────────────────
bool FileWatcher::start() {
    if (m_running.load()) return true;
    if (!initInotify()) return false;
    m_running = true;
    m_thread  = std::thread(&FileWatcher::watchLoop, this);
    LOG_INFO("FileWatcher started");
    return true;
}

void FileWatcher::stop() {
    if (!m_running.exchange(false)) return;
    if (m_thread.joinable()) m_thread.join();
    LOG_INFO("FileWatcher stopped");
}

bool FileWatcher::isRunning() const { return m_running.load(); }

std::vector<std::string> FileWatcher::watchedPaths() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return { m_watchedRoots.begin(), m_watchedRoots.end() };
}

// ─── Watch loop ───────────────────────────────────────────────────────────────
void FileWatcher::watchLoop() {
    // Buffer sized for up to 256 events
    constexpr size_t BUF_LEN = 256 * (sizeof(inotify_event) + NAME_MAX + 1);
    std::vector<char> buf(BUF_LEN);

    while (m_running.load()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(m_inotifyFd, &rfds);
        struct timeval tv{ 0, 200'000 }; // 200 ms poll

        int ret = select(m_inotifyFd + 1, &rfds, nullptr, nullptr, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            LOG_ERROR("select() error in FileWatcher", strerror(errno));
            break;
        }
        if (ret == 0) continue; // timeout — loop and re-check m_running

        ssize_t len = read(m_inotifyFd, buf.data(), BUF_LEN);
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            LOG_ERROR("inotify read failed", strerror(errno));
            break;
        }
        if (len == 0) continue;

        const char* ptr = buf.data();
        const char* end = buf.data() + len;
        while (ptr < end) {
            const auto* ev = reinterpret_cast<const inotify_event*>(ptr);
            processEvent(ev);
            ptr += sizeof(inotify_event) + ev->len;
        }
    }
}

// ─── Event processing ─────────────────────────────────────────────────────────
void FileWatcher::processEvent(const inotify_event* ev) {
    std::string dirPath;
    WatchCallback cb;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_wdToPath.find(ev->wd);
        if (it == m_wdToPath.end()) return;
        dirPath = it->second;
        cb = m_callback;
    }

    if (!cb) return;

    std::string name     = (ev->len > 0 && ev->name[0] != '\0') ? ev->name : "";
    std::string fullPath = name.empty() ? dirPath : (dirPath + "/" + name);
    bool        isDir    = (ev->mask & IN_ISDIR) != 0;

    // If a new directory appeared under a recursive root, auto-watch it
    if (isDir && (ev->mask & IN_CREATE)) {
        std::lock_guard<std::mutex> lk(m_mutex);
        for (const auto& rp : m_recursivePaths) {
            if (fullPath.size() > rp.size() &&
                fullPath.substr(0, rp.size()) == rp) {
                addInotifyWatch(fullPath);
                break;
            }
        }
    }

    WatchNotification notif;
    notif.path  = fullPath;
    notif.isDir = isDir;

    if (ev->mask & IN_CREATE) {
        notif.event = WatchEvent::Created;
        cb(notif);
    } else if (ev->mask & (IN_CLOSE_WRITE | IN_MODIFY)) {
        notif.event = WatchEvent::Modified;
        cb(notif);
    } else if (ev->mask & IN_DELETE) {
        notif.event = WatchEvent::Deleted;
        cb(notif);
    } else if (ev->mask & IN_MOVED_FROM) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_renames[ev->cookie] = { fullPath };
    } else if (ev->mask & IN_MOVED_TO) {
        std::string oldPath;
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it2 = m_renames.find(ev->cookie);
            if (it2 != m_renames.end()) {
                oldPath = it2->second.oldPath;
                m_renames.erase(it2);
            }
        }
        if (!oldPath.empty()) {
            notif.event   = WatchEvent::Renamed;
            notif.oldPath = oldPath;
        } else {
            notif.event = WatchEvent::MovedIn;
        }
        cb(notif);
    }
}

// ─── Stub required by header (logic is inline in watchLoop) ───────────────────
void FileWatcher::processEvents() { /* unused — see watchLoop */ }

} // namespace SDO
