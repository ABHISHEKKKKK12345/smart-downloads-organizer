#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <optional>
#include <sqlite3.h>

namespace SDO {

class Database {
public:
    static Database& instance();

    bool open(const std::string& path);
    void close();
    bool isOpen() const;

    // File records
    bool        upsertFile(const FileInfo& fi);
    bool        deleteFile(const std::string& path);
    bool        markProcessed(const std::string& path);
    std::optional<FileInfo> getFile(const std::string& path);
    std::vector<FileInfo>   getAllFiles();
    std::vector<FileInfo>   getFilesByCategory(FileCategory cat);
    std::vector<FileInfo>   getDuplicates();
    std::vector<FileInfo>   getLargeFiles(uint64_t minSize);
    std::vector<FileInfo>   getOldFiles(int minAgeDays);
    std::vector<FileInfo>   searchFiles(const std::string& query);

    // Duplicate detection
    std::vector<std::vector<FileInfo>> getDuplicateGroups();
    bool setDuplicate(const std::string& path, const std::string& duplicateOf);

    // Undo log
    bool            addUndoEntry(const UndoEntry& entry);
    bool            removeUndoEntry(const std::string& id);
    std::vector<UndoEntry> getUndoHistory(int limit = 100);

    // Statistics
    Statistics getStatistics();
    bool       updateScanTime(double durationMs);

    // Maintenance
    bool vacuum();
    bool pruneOldRecords(int olderThanDays = 90);
    uint64_t getDatabaseSize();

private:
    Database() = default;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool        execSQL(const std::string& sql);
    bool        createSchema();
    FileInfo    rowToFileInfo(sqlite3_stmt* stmt);
    UndoEntry   rowToUndoEntry(sqlite3_stmt* stmt);
    std::string escapeStr(const std::string& s) const;
    int64_t     toUnixMs(const std::chrono::system_clock::time_point& tp) const;
    std::chrono::system_clock::time_point fromUnixMs(int64_t ms) const;

    mutable std::mutex  m_mutex;
    sqlite3*            m_db     = nullptr;
    std::string         m_path;
};

} // namespace SDO
