#include "organizer_engine.hpp"
#include "config_manager.hpp"
#include "file_analyzer.hpp"
#include "database.hpp"
#include "logger.hpp"
#include <filesystem>
#include <algorithm>
#include <regex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <uuid/uuid.h>

namespace fs = std::filesystem;

namespace SDO {

// ─── UUID helper ─────────────────────────────────────────────────────────────
static std::string generateUUID() {
    uuid_t uu;
    uuid_generate_random(uu);
    char buf[37];
    uuid_unparse_lower(uu, buf);
    return std::string(buf);
}

// ─── Singleton ───────────────────────────────────────────────────────────────
OrganizerEngine& OrganizerEngine::instance() {
    static OrganizerEngine inst;
    return inst;
}

OrganizerEngine::OrganizerEngine() {
    m_watcher = std::make_unique<FileWatcher>();
    m_watcher->setCallback([this](const WatchNotification& n){ onFileEvent(n); });
}

// ─── Callbacks ────────────────────────────────────────────────────────────────
void OrganizerEngine::setFileDetectedCallback(FileDetectedCb cb)   { m_fileDetectedCb = std::move(cb); }
void OrganizerEngine::setStatsUpdatedCallback(StatsUpdatedCb cb)   { m_statsUpdatedCb = std::move(cb); }
void OrganizerEngine::setNotificationCallback(NotificationCb cb)   { m_notificationCb = std::move(cb); }

// ─── Watching ─────────────────────────────────────────────────────────────────
bool OrganizerEngine::startWatching() {
    const auto& cfg = ConfigManager::instance().config();
    for (const auto& path : cfg.watchPaths) {
        m_watcher->addWatch(path, cfg.watchRecursive);
    }
    if (!m_watcher->start()) {
        LOG_ERROR("Failed to start FileWatcher");
        return false;
    }

    m_running = true;
    m_workerThread = std::thread(&OrganizerEngine::processLoop, this);
    LOG_INFO("OrganizerEngine started watching");

    // Initial scan
    scanAllWatchPaths();
    return true;
}

void OrganizerEngine::stopWatching() {
    m_running = false;
    m_cv.notify_all();
    m_watcher->stop();
    if (m_workerThread.joinable()) m_workerThread.join();
    LOG_INFO("OrganizerEngine stopped");
}

bool OrganizerEngine::isWatching() const { return m_running.load(); }

// ─── File scanning ───────────────────────────────────────────────────────────
void OrganizerEngine::scanDirectory(const std::string& path, bool recurse) {
    if (!fs::exists(path)) {
        LOG_WARN("Scan path does not exist", "", path);
        return;
    }

    auto scanStart = std::chrono::steady_clock::now();
    size_t count = 0;

    auto scanEntry = [&](const fs::directory_entry& entry) {
        if (!entry.is_regular_file()) return;
        try {
            enqueueFile(entry.path().string());
            ++count;
        } catch(const std::exception& ex) {
            LOG_WARN("Scan entry error", ex.what(), entry.path().string());
        }
    };

    try {
        if (recurse) {
            for (auto& e : fs::recursive_directory_iterator(
                    path, fs::directory_options::skip_permission_denied)) {
                scanEntry(e);
            }
        } else {
            for (auto& e : fs::directory_iterator(
                    path, fs::directory_options::skip_permission_denied)) {
                scanEntry(e);
            }
        }
    } catch(const std::exception& ex) {
        LOG_ERROR("scanDirectory error", ex.what(), path);
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - scanStart).count();
    LOG_INFO("Scan complete: " + std::to_string(count) + " files in " +
             std::to_string(elapsed) + "ms", "", path);
}

void OrganizerEngine::scanAllWatchPaths() {
    const auto& cfg = ConfigManager::instance().config();
    for (const auto& path : cfg.watchPaths) {
        scanDirectory(path, cfg.watchRecursive);
    }
}

void OrganizerEngine::enqueueFile(const std::string& path) {
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_processingSet.count(path)) return;
        m_processingSet.insert(path);
        m_fileQueue.push(path);
    }
    m_cv.notify_one();
}

size_t OrganizerEngine::queueSize() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_fileQueue.size();
}

// ─── Background worker ───────────────────────────────────────────────────────
void OrganizerEngine::processLoop() {
    while (m_running.load()) {
        std::string path;
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cv.wait_for(lk, std::chrono::milliseconds(500), [this]{
                return !m_fileQueue.empty() || !m_running.load();
            });
            if (!m_running.load() && m_fileQueue.empty()) break;
            if (m_fileQueue.empty()) continue;
            path = m_fileQueue.front();
            m_fileQueue.pop();
        }

        try {
            processFile(path);
        } catch(const std::exception& ex) {
            LOG_ERROR("processFile exception", ex.what(), path);
        }

        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_processingSet.erase(path);
        }
    }
}

void OrganizerEngine::processFile(const std::string& path) {
    if (!fs::exists(path) || !fs::is_regular_file(path)) return;

    // Skip very small files (lock files, etc.)
    try {
        if (fs::file_size(path) == 0) return;
    } catch(...) { return; }

    const auto& cfg = ConfigManager::instance().config();

    // Analyze the file
    FileInfo fi = cfg.enableDuplicateDetect
                  ? FileAnalyzer::instance().analyze(path)        // full hash
                  : FileAnalyzer::instance().analyzeQuick(path);  // no hash

    if (fi.path.empty()) return;

    // Duplicate detection: check if another record shares same SHA-256
    if (cfg.enableDuplicateDetect && !fi.sha256Hash.empty()) {
        auto existing = Database::instance().getFile(path);
        // If already recorded and hash unchanged, skip expensive re-scan
        if (existing.has_value() && existing->sha256Hash == fi.sha256Hash
                                 && existing->isDuplicate) {
            fi.isDuplicate = existing->isDuplicate;
            fi.duplicateOf = existing->duplicateOf;
            fi.status      = FileStatus::Duplicate;
        } else {
            // Full scan against all known files
            auto all = Database::instance().getAllFiles();
            for (const auto& other : all) {
                if (other.path != path &&
                    !other.sha256Hash.empty() &&
                    other.sha256Hash == fi.sha256Hash) {
                    fi.isDuplicate = true;
                    fi.duplicateOf = other.path;
                    fi.status      = FileStatus::Duplicate;
                    break;
                }
            }
        }
    }

    // Persist
    Database::instance().upsertFile(fi);

    // Notify UI
    if (m_fileDetectedCb) m_fileDetectedCb(fi);

    // Apply rules if auto-organize is on
    if (cfg.autoOrganize) {
        auto result = applyRules(fi);
        if (result.success && result.action != ActionType::Skip) {
            LOG_INFO("Organized: " + fi.filename, "→ " + result.destPath);
            emitNotification("File Organized",
                             fi.filename + " → " +
                             fs::path(result.destPath).parent_path().filename().string(),
                             LogLevel::Info);
            // Post updated stats to UI
            auto stats = Database::instance().getStatistics();
            if (m_statsUpdatedCb) m_statsUpdatedCb(stats);
        }
    }
}

// ─── Rule matching ───────────────────────────────────────────────────────────
bool OrganizerEngine::matchesCondition(const FileInfo& fi, const RuleCondition& cond) const {
    std::string fieldVal;
    if      (cond.field == "extension") fieldVal = fi.extension;
    else if (cond.field == "name")      fieldVal = fi.filename;
    else if (cond.field == "category")  fieldVal = fi.categoryName();
    else if (cond.field == "mime")      fieldVal = fi.mimeType;
    else if (cond.field == "size") {
        uint64_t condSize = std::stoull(cond.value);
        if (cond.op == "gt") return fi.sizeBytes > condSize;
        if (cond.op == "lt") return fi.sizeBytes < condSize;
        if (cond.op == "eq") return fi.sizeBytes == condSize;
        return false;
    } else if (cond.field == "age") {
        int64_t days    = fi.ageInDays();
        int64_t condAge = std::stoll(cond.value);
        if (cond.op == "gt") return days > condAge;
        if (cond.op == "lt") return days < condAge;
        if (cond.op == "eq") return days == condAge;
        return false;
    } else {
        return false;
    }

    std::string a = cond.caseSensitive ? fieldVal : [&]{
        std::string s = fieldVal;
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }();
    std::string b = cond.caseSensitive ? cond.value : [&]{
        std::string s = cond.value;
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }();

    if (cond.op == "eq")       return a == b;
    if (cond.op == "ne")       return a != b;
    if (cond.op == "contains") return a.find(b) != std::string::npos;
    if (cond.op == "startswith") return a.substr(0, b.size()) == b;
    if (cond.op == "endswith") return a.size() >= b.size() && a.substr(a.size()-b.size()) == b;
    if (cond.op == "regex") {
        try {
            std::regex re(b, std::regex::icase);
            return std::regex_search(a, re);
        } catch(...) { return false; }
    }
    return false;
}

bool OrganizerEngine::matchesRule(const FileInfo& fi, const OrganizerRule& rule) const {
    if (!rule.enabled || rule.conditions.empty()) return false;

    bool isAnd = (rule.conditionLogic != "OR");

    if (isAnd) {
        for (const auto& cond : rule.conditions) {
            if (!matchesCondition(fi, cond)) return false;
        }
        return true;
    } else {
        for (const auto& cond : rule.conditions) {
            if (matchesCondition(fi, cond)) return true;
        }
        return false;
    }
}

OrganizeResult OrganizerEngine::applyRules(const FileInfo& fi) {
    OrganizeResult result;
    result.sourcePath = fi.path;
    result.action     = ActionType::Skip;

    const auto& rules = ConfigManager::instance().config().rules;
    // Sort by priority descending
    std::vector<const OrganizerRule*> sorted;
    for (const auto& r : rules) sorted.push_back(&r);
    std::sort(sorted.begin(), sorted.end(), [](const OrganizerRule* a, const OrganizerRule* b){
        return a->priority > b->priority;
    });

    for (const auto* rule : sorted) {
        if (matchesRule(fi, *rule)) {
            result.ruleName = rule->name;
            executeAction(rule->action, fi, result);
            if (result.success) {
                Database::instance().markProcessed(fi.path);
                saveUndoEntry(result);
            }
            return result;
        }
    }

    result.success = true; // No rule matched → skip
    return result;
}

// ─── Pattern resolution ───────────────────────────────────────────────────────
std::string OrganizerEngine::resolvePattern(const std::string& pattern, const FileInfo& fi) const {
    using namespace std::chrono;
    auto t = system_clock::to_time_t(fi.modifiedAt);
    std::tm tm_buf{};
    localtime_r(&t, &tm_buf);

    fs::path p(fi.filename);
    std::string stem  = p.stem().string();
    std::string ext   = fi.extension;

    std::string result = pattern;
    auto replace = [&](const std::string& key, const std::string& val) {
        size_t pos = 0;
        while ((pos = result.find(key, pos)) != std::string::npos) {
            result.replace(pos, key.size(), val);
            pos += val.size();
        }
    };

    char dateBuf[32], yearBuf[8], monthBuf[4];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &tm_buf);
    std::strftime(yearBuf, sizeof(yearBuf), "%Y", &tm_buf);
    std::strftime(monthBuf, sizeof(monthBuf), "%m", &tm_buf);

    replace("{name}",     stem);
    replace("{ext}",      ext);
    replace("{date}",     dateBuf);
    replace("{year}",     yearBuf);
    replace("{month}",    monthBuf);
    replace("{category}", fi.categoryName());
    replace("{size}",     fi.sizeHuman());

    return result;
}

std::string OrganizerEngine::buildDestPath(const RuleAction& action, const FileInfo& fi) const {
    fs::path baseDir = action.targetDirectory;

    if (action.createSubfolders) {
        // Create year/month subfolder
        using namespace std::chrono;
        auto t = system_clock::to_time_t(fi.modifiedAt);
        std::tm tm_buf{};
        localtime_r(&t, &tm_buf);
        char sub[32];
        std::strftime(sub, sizeof(sub), "%Y/%m", &tm_buf);
        baseDir /= sub;
    }

    std::string destName = fi.filename;
    if (!action.renamePattern.empty()) {
        destName = resolvePattern(action.renamePattern, fi) + "." + fi.extension;
    }

    return (baseDir / destName).string();
}

std::string OrganizerEngine::makeUniquePath(const std::string& path) const {
    if (!fs::exists(path)) return path;

    fs::path p(path);
    std::string stem  = p.stem().string();
    std::string ext   = p.extension().string();
    fs::path    dir   = p.parent_path();

    for (int i = 1; i < 9999; ++i) {
        fs::path candidate = dir / (stem + "_" + std::to_string(i) + ext);
        if (!fs::exists(candidate)) return candidate.string();
    }
    return path + "_dup";
}

// ─── File operations ─────────────────────────────────────────────────────────
bool OrganizerEngine::moveFile(const std::string& src, const std::string& dst) {
    if (ConfigManager::instance().simulateMode()) {
        LOG_INFO("[SIMULATE] Would move: " + src + " → " + dst);
        return true;
    }
    try {
        fs::create_directories(fs::path(dst).parent_path());
        std::string finalDst = makeUniquePath(dst);
        fs::rename(src, finalDst);
        return true;
    } catch(const std::exception& ex) {
        // Cross-device move: copy + remove
        try {
            fs::create_directories(fs::path(dst).parent_path());
            std::string finalDst = makeUniquePath(dst);
            fs::copy_file(src, finalDst, fs::copy_options::overwrite_existing);
            fs::remove(src);
            return true;
        } catch(const std::exception& ex2) {
            LOG_ERROR("Move failed", ex2.what(), src);
            return false;
        }
    }
}

bool OrganizerEngine::copyFile(const std::string& src, const std::string& dst) {
    if (ConfigManager::instance().simulateMode()) {
        LOG_INFO("[SIMULATE] Would copy: " + src + " → " + dst);
        return true;
    }
    try {
        fs::create_directories(fs::path(dst).parent_path());
        std::string finalDst = makeUniquePath(dst);
        fs::copy_file(src, finalDst, fs::copy_options::overwrite_existing);
        return true;
    } catch(const std::exception& ex) {
        LOG_ERROR("Copy failed", ex.what(), src);
        return false;
    }
}

bool OrganizerEngine::deleteFile(const std::string& path) {
    if (ConfigManager::instance().simulateMode()) {
        LOG_INFO("[SIMULATE] Would delete: " + path);
        return true;
    }
    try {
        if (ConfigManager::instance().config().moveToTrash) {
            // Move to ~/.local/share/Trash/files/
            const char* home = std::getenv("HOME");
            std::string trashDir = std::string(home) + "/.local/share/Trash/files/";
            fs::create_directories(trashDir);
            std::string trashPath = makeUniquePath(trashDir + fs::path(path).filename().string());
            fs::rename(path, trashPath);
        } else {
            fs::remove(path);
        }
        return true;
    } catch(const std::exception& ex) {
        LOG_ERROR("Delete failed", ex.what(), path);
        return false;
    }
}

bool OrganizerEngine::executeAction(const RuleAction& action, const FileInfo& fi, OrganizeResult& result) {
    result.action    = action.type;
    result.sizeBytes = fi.sizeBytes;

    switch (action.type) {
        case ActionType::Move: {
            result.destPath = buildDestPath(action, fi);
            result.success  = moveFile(fi.path, result.destPath);
            if (result.success) Database::instance().deleteFile(fi.path);
            break;
        }
        case ActionType::Copy: {
            result.destPath = buildDestPath(action, fi);
            result.success  = copyFile(fi.path, result.destPath);
            break;
        }
        case ActionType::Delete: {
            result.success = deleteFile(fi.path);
            if (result.success) Database::instance().deleteFile(fi.path);
            break;
        }
        case ActionType::Rename: {
            if (!action.renamePattern.empty()) {
                std::string newName = resolvePattern(action.renamePattern, fi) + "." + fi.extension;
                result.destPath = (fs::path(fi.path).parent_path() / newName).string();
                result.success  = moveFile(fi.path, result.destPath);
            } else {
                result.success = false;
                result.error   = "No rename pattern specified";
            }
            break;
        }
        case ActionType::Skip:
        default:
            result.success = true;
            result.action  = ActionType::Skip;
            break;
    }
    return result.success;
}

// ─── Undo ─────────────────────────────────────────────────────────────────────
void OrganizerEngine::saveUndoEntry(const OrganizeResult& result) {
    if (result.action == ActionType::Skip) return;
    UndoEntry e;
    e.id          = generateUUID();
    e.action      = result.action;
    e.sourcePath  = result.sourcePath;
    e.destPath    = result.destPath;
    e.description = "Rule: " + result.ruleName;
    e.performedAt = std::chrono::system_clock::now();
    e.canUndo     = (result.action != ActionType::Delete);
    Database::instance().addUndoEntry(e);
}

bool OrganizerEngine::undoLastAction() {
    auto history = Database::instance().getUndoHistory(1);
    if (history.empty()) return false;
    return undoAction(history[0].id);
}

bool OrganizerEngine::undoAction(const std::string& undoId) {
    auto history = Database::instance().getUndoHistory(1000);
    for (const auto& e : history) {
        if (e.id != undoId) continue;
        if (!e.canUndo) {
            LOG_WARN("Cannot undo deleted file", e.description);
            return false;
        }
        bool ok = false;
        if (e.action == ActionType::Move || e.action == ActionType::Rename) {
            ok = moveFile(e.destPath, e.sourcePath);
        } else if (e.action == ActionType::Copy) {
            ok = deleteFile(e.destPath);
        }
        if (ok) {
            Database::instance().removeUndoEntry(undoId);
            emitNotification("Undo successful",
                             fs::path(e.sourcePath).filename().string() + " restored",
                             LogLevel::Info);
        }
        return ok;
    }
    return false;
}

// ─── Duplicate scan ───────────────────────────────────────────────────────────
void OrganizerEngine::runDuplicateScan() {
    LOG_INFO("Running duplicate scan...");
    auto groups = Database::instance().getDuplicateGroups();
    int  marked = 0;
    for (const auto& group : groups) {
        // First file in group is the "original"; mark the rest as duplicates
        for (size_t i = 1; i < group.size(); ++i) {
            Database::instance().setDuplicate(group[i].path, group[0].path);
            ++marked;
        }
    }
    LOG_INFO("Duplicate scan complete. Duplicates found: " + std::to_string(marked));
    if (marked > 0) {
        emitNotification("Duplicates Found",
                         std::to_string(marked) + " duplicate files detected",
                         LogLevel::Warning);
    }
}

uint64_t OrganizerEngine::getDuplicateSavings() {
    auto dups = Database::instance().getDuplicates();
    uint64_t total = 0;
    for (const auto& f : dups) total += f.sizeBytes;
    return total;
}

// ─── Cleanup suggestions ─────────────────────────────────────────────────────
std::vector<FileInfo> OrganizerEngine::getCleanupSuggestions() {
    std::vector<FileInfo> suggestions;
    const auto& cfg = ConfigManager::instance().config();

    // Large files
    auto large = Database::instance().getLargeFiles(cfg.largeSizeThreshold);
    suggestions.insert(suggestions.end(), large.begin(), large.end());

    // Old files
    auto old = Database::instance().getOldFiles(cfg.oldFileAgeDays);
    // Merge, dedup
    for (const auto& f : old) {
        bool found = false;
        for (const auto& s : suggestions) { if (s.path == f.path) { found = true; break; } }
        if (!found) suggestions.push_back(f);
    }

    // Duplicates
    auto dups = Database::instance().getDuplicates();
    for (const auto& f : dups) {
        bool found = false;
        for (const auto& s : suggestions) { if (s.path == f.path) { found = true; break; } }
        if (!found) suggestions.push_back(f);
    }

    return suggestions;
}

// ─── Event handling ───────────────────────────────────────────────────────────
void OrganizerEngine::onFileEvent(const WatchNotification& notif) {
    if (notif.isDir) return;

    switch (notif.event) {
        case WatchEvent::Created:
        case WatchEvent::Modified:
        case WatchEvent::MovedIn:
            // Slight delay to ensure file is fully written
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            enqueueFile(notif.path);
            break;
        case WatchEvent::Deleted:
        case WatchEvent::MovedOut:
            Database::instance().deleteFile(notif.path);
            break;
        case WatchEvent::Renamed:
            Database::instance().deleteFile(notif.oldPath);
            enqueueFile(notif.path);
            break;
    }
}

bool OrganizerEngine::applyRulesToAll() {
    auto files = Database::instance().getAllFiles();
    int count = 0;
    for (const auto& fi : files) {
        if (!fi.isProcessed) {
            auto result = applyRules(fi);
            if (result.success && result.action != ActionType::Skip) ++count;
        }
    }
    LOG_INFO("Batch organize complete. Files organized: " + std::to_string(count));
    return true;
}

void OrganizerEngine::rescanAll() {
    LOG_INFO("Rescan all triggered");
    scanAllWatchPaths();
}

void OrganizerEngine::emitNotification(const std::string& title, const std::string& msg, LogLevel level) {
    if (!m_notificationCb) return;
    Notification n;
    n.title   = title;
    n.message = msg;
    n.level   = level;
    n.time    = std::chrono::system_clock::now();
    m_notificationCb(n);
}

} // namespace SDO
