#include "database.hpp"
#include "config_manager.hpp"
#include "logger.hpp"
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <cstring>

namespace fs = std::filesystem;

namespace SDO {

Database& Database::instance() {
    static Database inst;
    return inst;
}

int64_t Database::toUnixMs(const std::chrono::system_clock::time_point& tp) const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point Database::fromUnixMs(int64_t ms) const {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
}

bool Database::open(const std::string& path) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_db) { sqlite3_close(m_db); m_db = nullptr; }

    m_path = path;
    try {
        fs::create_directories(fs::path(path).parent_path());
    } catch(const std::exception& ex) {
        LOG_ERROR("Cannot create DB dir", ex.what());
    }

    int rc = sqlite3_open_v2(path.c_str(), &m_db,
                              SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                              SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Cannot open database", sqlite3_errmsg(m_db), path);
        m_db = nullptr;
        return false;
    }

    // Performance settings
    sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA cache_size=10000;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA temp_store=MEMORY;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

    if (!createSchema()) {
        LOG_ERROR("Schema creation failed");
        return false;
    }
    LOG_INFO("Database opened", path);
    return true;
}

bool Database::createSchema() {
    const char* ddl = R"SQL(
        CREATE TABLE IF NOT EXISTS files (
            path         TEXT PRIMARY KEY,
            filename     TEXT NOT NULL,
            extension    TEXT,
            mime_type    TEXT,
            size_bytes   INTEGER NOT NULL DEFAULT 0,
            sha256_hash  TEXT,
            md5_hash     TEXT,
            category     INTEGER NOT NULL DEFAULT 11,
            status       INTEGER NOT NULL DEFAULT 0,
            is_processed INTEGER NOT NULL DEFAULT 0,
            is_duplicate INTEGER NOT NULL DEFAULT 0,
            duplicate_of TEXT,
            created_at   INTEGER,
            modified_at  INTEGER,
            accessed_at  INTEGER,
            detected_at  INTEGER
        );

        CREATE INDEX IF NOT EXISTS idx_files_category   ON files(category);
        CREATE INDEX IF NOT EXISTS idx_files_sha256     ON files(sha256_hash);
        CREATE INDEX IF NOT EXISTS idx_files_size       ON files(size_bytes);
        CREATE INDEX IF NOT EXISTS idx_files_modified   ON files(modified_at);
        CREATE INDEX IF NOT EXISTS idx_files_duplicate  ON files(is_duplicate);
        CREATE INDEX IF NOT EXISTS idx_files_ext        ON files(extension);

        CREATE TABLE IF NOT EXISTS undo_log (
            id           TEXT PRIMARY KEY,
            action       INTEGER NOT NULL,
            source_path  TEXT NOT NULL,
            dest_path    TEXT,
            description  TEXT,
            performed_at INTEGER,
            can_undo     INTEGER NOT NULL DEFAULT 1
        );

        CREATE INDEX IF NOT EXISTS idx_undo_time ON undo_log(performed_at);

        CREATE TABLE IF NOT EXISTS statistics (
            key   TEXT PRIMARY KEY,
            value TEXT
        );
    )SQL";

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, ddl, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "unknown";
        sqlite3_free(errMsg);
        LOG_ERROR("Schema creation failed", err);
        return false;
    }
    return true;
}

void Database::close() {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_db) {
        sqlite3_close_v2(m_db);
        m_db = nullptr;
    }
}

bool Database::isOpen() const {
    return m_db != nullptr;
}

bool Database::execSQL(const std::string& sql) {
    if (!m_db) return false;
    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string err = errMsg ? errMsg : "unknown";
        sqlite3_free(errMsg);
        LOG_ERROR("SQL execution failed", err);
        return false;
    }
    return true;
}

std::string Database::escapeStr(const std::string& s) const {
    std::string result;
    result.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '\'') result += "''";
        else result += c;
    }
    return result;
}

bool Database::upsertFile(const FileInfo& fi) {
    if (!m_db) return false;
    std::lock_guard<std::mutex> lk(m_mutex);

    const char* sql = R"SQL(
        INSERT OR REPLACE INTO files
        (path, filename, extension, mime_type, size_bytes, sha256_hash, md5_hash,
         category, status, is_processed, is_duplicate, duplicate_of,
         created_at, modified_at, accessed_at, detected_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )SQL";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR("upsertFile prepare failed", sqlite3_errmsg(m_db));
        return false;
    }

    sqlite3_bind_text(stmt,  1, fi.path.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  2, fi.filename.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  3, fi.extension.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  4, fi.mimeType.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, static_cast<int64_t>(fi.sizeBytes));
    sqlite3_bind_text(stmt,  6, fi.sha256Hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  7, fi.md5Hash.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,   8, static_cast<int>(fi.category));
    sqlite3_bind_int(stmt,   9, static_cast<int>(fi.status));
    sqlite3_bind_int(stmt,  10, fi.isProcessed ? 1 : 0);
    sqlite3_bind_int(stmt,  11, fi.isDuplicate ? 1 : 0);
    sqlite3_bind_text(stmt, 12, fi.duplicateOf.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt,13, toUnixMs(fi.createdAt));
    sqlite3_bind_int64(stmt,14, toUnixMs(fi.modifiedAt));
    sqlite3_bind_int64(stmt,15, toUnixMs(fi.accessedAt));
    sqlite3_bind_int64(stmt,16, toUnixMs(fi.detectedAt));

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

bool Database::deleteFile(const std::string& path) {
    if (!m_db) return false;
    std::lock_guard<std::mutex> lk(m_mutex);
    std::string sql = "DELETE FROM files WHERE path='" + escapeStr(path) + "'";
    return execSQL(sql);
}

bool Database::markProcessed(const std::string& path) {
    if (!m_db) return false;
    std::lock_guard<std::mutex> lk(m_mutex);
    std::string sql = "UPDATE files SET is_processed=1 WHERE path='" + escapeStr(path) + "'";
    return execSQL(sql);
}

FileInfo Database::rowToFileInfo(sqlite3_stmt* stmt) {
    FileInfo fi;
    int col = 0;
    auto getStr = [&](int c) -> std::string {
        const char* v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, c));
        return v ? v : "";
    };
    fi.path        = getStr(col++);
    fi.filename    = getStr(col++);
    fi.extension   = getStr(col++);
    fi.mimeType    = getStr(col++);
    fi.sizeBytes   = static_cast<uint64_t>(sqlite3_column_int64(stmt, col++));
    fi.sha256Hash  = getStr(col++);
    fi.md5Hash     = getStr(col++);
    fi.category    = static_cast<FileCategory>(sqlite3_column_int(stmt, col++));
    fi.status      = static_cast<FileStatus>(sqlite3_column_int(stmt, col++));
    fi.isProcessed = sqlite3_column_int(stmt, col++) != 0;
    fi.isDuplicate = sqlite3_column_int(stmt, col++) != 0;
    fi.duplicateOf = getStr(col++);
    fi.createdAt   = fromUnixMs(sqlite3_column_int64(stmt, col++));
    fi.modifiedAt  = fromUnixMs(sqlite3_column_int64(stmt, col++));
    fi.accessedAt  = fromUnixMs(sqlite3_column_int64(stmt, col++));
    fi.detectedAt  = fromUnixMs(sqlite3_column_int64(stmt, col++));
    return fi;
}

std::vector<FileInfo> Database::getAllFiles() {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<FileInfo> result;
    if (!m_db) return result;

    const char* sql = "SELECT path,filename,extension,mime_type,size_bytes,sha256_hash,md5_hash,"
                      "category,status,is_processed,is_duplicate,duplicate_of,"
                      "created_at,modified_at,accessed_at,detected_at FROM files ORDER BY detected_at DESC";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) result.push_back(rowToFileInfo(stmt));
    sqlite3_finalize(stmt);
    return result;
}

std::vector<FileInfo> Database::getFilesByCategory(FileCategory cat) {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<FileInfo> result;
    if (!m_db) return result;

    std::string sql = "SELECT path,filename,extension,mime_type,size_bytes,sha256_hash,md5_hash,"
                      "category,status,is_processed,is_duplicate,duplicate_of,"
                      "created_at,modified_at,accessed_at,detected_at FROM files "
                      "WHERE category=" + std::to_string(static_cast<int>(cat)) +
                      " ORDER BY size_bytes DESC";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) result.push_back(rowToFileInfo(stmt));
    sqlite3_finalize(stmt);
    return result;
}

std::vector<FileInfo> Database::getDuplicates() {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<FileInfo> result;
    if (!m_db) return result;

    const char* sql = "SELECT path,filename,extension,mime_type,size_bytes,sha256_hash,md5_hash,"
                      "category,status,is_processed,is_duplicate,duplicate_of,"
                      "created_at,modified_at,accessed_at,detected_at FROM files "
                      "WHERE is_duplicate=1 ORDER BY sha256_hash, modified_at";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) result.push_back(rowToFileInfo(stmt));
    sqlite3_finalize(stmt);
    return result;
}

std::vector<std::vector<FileInfo>> Database::getDuplicateGroups() {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<std::vector<FileInfo>> groups;
    if (!m_db) return groups;

    // Find all sha256 hashes that appear more than once
    const char* hashSql = "SELECT sha256_hash FROM files "
                           "WHERE sha256_hash IS NOT NULL AND sha256_hash != '' "
                           "GROUP BY sha256_hash HAVING COUNT(*) > 1";
    sqlite3_stmt* hashStmt = nullptr;
    if (sqlite3_prepare_v2(m_db, hashSql, -1, &hashStmt, nullptr) != SQLITE_OK) return groups;

    std::vector<std::string> hashes;
    while (sqlite3_step(hashStmt) == SQLITE_ROW) {
        const char* h = reinterpret_cast<const char*>(sqlite3_column_text(hashStmt, 0));
        if (h) hashes.emplace_back(h);
    }
    sqlite3_finalize(hashStmt);

    for (const auto& hash : hashes) {
        const char* fileSql = "SELECT path,filename,extension,mime_type,size_bytes,sha256_hash,md5_hash,"
                              "category,status,is_processed,is_duplicate,duplicate_of,"
                              "created_at,modified_at,accessed_at,detected_at FROM files "
                              "WHERE sha256_hash=? ORDER BY detected_at ASC";
        sqlite3_stmt* fileStmt = nullptr;
        if (sqlite3_prepare_v2(m_db, fileSql, -1, &fileStmt, nullptr) != SQLITE_OK) continue;
        sqlite3_bind_text(fileStmt, 1, hash.c_str(), -1, SQLITE_TRANSIENT);

        std::vector<FileInfo> group;
        while (sqlite3_step(fileStmt) == SQLITE_ROW) group.push_back(rowToFileInfo(fileStmt));
        sqlite3_finalize(fileStmt);

        if (group.size() > 1) groups.push_back(std::move(group));
    }
    return groups;
}

std::vector<FileInfo> Database::getLargeFiles(uint64_t minSize) {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<FileInfo> result;
    if (!m_db) return result;

    std::string sql = "SELECT path,filename,extension,mime_type,size_bytes,sha256_hash,md5_hash,"
                      "category,status,is_processed,is_duplicate,duplicate_of,"
                      "created_at,modified_at,accessed_at,detected_at FROM files "
                      "WHERE size_bytes>=" + std::to_string(minSize) + " ORDER BY size_bytes DESC";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) result.push_back(rowToFileInfo(stmt));
    sqlite3_finalize(stmt);
    return result;
}

std::vector<FileInfo> Database::getOldFiles(int minAgeDays) {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<FileInfo> result;
    if (!m_db) return result;

    using namespace std::chrono;
    auto cutoff = system_clock::now() - hours(24LL * minAgeDays);
    int64_t cutoffMs = duration_cast<milliseconds>(cutoff.time_since_epoch()).count();

    std::string sql = "SELECT path,filename,extension,mime_type,size_bytes,sha256_hash,md5_hash,"
                      "category,status,is_processed,is_duplicate,duplicate_of,"
                      "created_at,modified_at,accessed_at,detected_at FROM files "
                      "WHERE modified_at<" + std::to_string(cutoffMs) + " ORDER BY modified_at ASC";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) result.push_back(rowToFileInfo(stmt));
    sqlite3_finalize(stmt);
    return result;
}

std::vector<FileInfo> Database::searchFiles(const std::string& query) {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<FileInfo> result;
    if (!m_db) return result;

    const char* sql = "SELECT path,filename,extension,mime_type,size_bytes,sha256_hash,md5_hash,"
                      "category,status,is_processed,is_duplicate,duplicate_of,"
                      "created_at,modified_at,accessed_at,detected_at FROM files "
                      "WHERE filename LIKE ? OR path LIKE ? ORDER BY detected_at DESC LIMIT 500";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;
    std::string pattern = "%" + query + "%";
    sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) result.push_back(rowToFileInfo(stmt));
    sqlite3_finalize(stmt);
    return result;
}

bool Database::setDuplicate(const std::string& path, const std::string& duplicateOf) {
    if (!m_db) return false;
    std::lock_guard<std::mutex> lk(m_mutex);
    const char* sql = "UPDATE files SET is_duplicate=1, duplicate_of=?, status=1 WHERE path=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, duplicateOf.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, path.c_str(),        -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool Database::addUndoEntry(const UndoEntry& entry) {
    if (!m_db) return false;
    std::lock_guard<std::mutex> lk(m_mutex);
    const char* sql = "INSERT OR REPLACE INTO undo_log "
                      "(id, action, source_path, dest_path, description, performed_at, can_undo) "
                      "VALUES (?,?,?,?,?,?,?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt,  1, entry.id.c_str(),          -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,   2, static_cast<int>(entry.action));
    sqlite3_bind_text(stmt,  3, entry.sourcePath.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  4, entry.destPath.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,  5, entry.description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 6, toUnixMs(entry.performedAt));
    sqlite3_bind_int(stmt,   7, entry.canUndo ? 1 : 0);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool Database::removeUndoEntry(const std::string& id) {
    if (!m_db) return false;
    std::lock_guard<std::mutex> lk(m_mutex);
    std::string sql = "DELETE FROM undo_log WHERE id='" + escapeStr(id) + "'";
    return execSQL(sql);
}

UndoEntry Database::rowToUndoEntry(sqlite3_stmt* stmt) {
    UndoEntry e;
    auto getStr = [&](int c) -> std::string {
        const char* v = reinterpret_cast<const char*>(sqlite3_column_text(stmt, c));
        return v ? v : "";
    };
    e.id          = getStr(0);
    e.action      = static_cast<ActionType>(sqlite3_column_int(stmt, 1));
    e.sourcePath  = getStr(2);
    e.destPath    = getStr(3);
    e.description = getStr(4);
    e.performedAt = fromUnixMs(sqlite3_column_int64(stmt, 5));
    e.canUndo     = sqlite3_column_int(stmt, 6) != 0;
    return e;
}

std::vector<UndoEntry> Database::getUndoHistory(int limit) {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<UndoEntry> result;
    if (!m_db) return result;
    std::string sql = "SELECT id,action,source_path,dest_path,description,performed_at,can_undo "
                      "FROM undo_log ORDER BY performed_at DESC LIMIT " + std::to_string(limit);
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) result.push_back(rowToUndoEntry(stmt));
    sqlite3_finalize(stmt);
    return result;
}

Statistics Database::getStatistics() {
    std::lock_guard<std::mutex> lk(m_mutex);
    Statistics st;
    if (!m_db) return st;

    auto queryInt = [&](const char* sql) -> int64_t {
        sqlite3_stmt* s = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &s, nullptr) != SQLITE_OK) return 0;
        int64_t val = 0;
        if (sqlite3_step(s) == SQLITE_ROW) val = sqlite3_column_int64(s, 0);
        sqlite3_finalize(s);
        return val;
    };

    st.totalFiles    = queryInt("SELECT COUNT(*) FROM files");
    st.totalSize     = queryInt("SELECT COALESCE(SUM(size_bytes),0) FROM files");
    st.duplicateFiles= queryInt("SELECT COUNT(*) FROM files WHERE is_duplicate=1");
    st.duplicateSize = queryInt("SELECT COALESCE(SUM(size_bytes),0) FROM files WHERE is_duplicate=1");

    // C-2 fix: read threshold from user config instead of hardcoded 524288000
    const auto& cfg = ConfigManager::instance().config();
    std::string largeSql = "SELECT COUNT(*) FROM files WHERE size_bytes>="
                         + std::to_string(cfg.largeSizeThreshold);
    st.largeFiles    = queryInt(largeSql.c_str());

    // H-3 fix: read age threshold from user config instead of hardcoded 30 days
    using namespace std::chrono;
    auto cutoff = system_clock::now() - hours(24LL * cfg.oldFileAgeDays);
    int64_t cMs = duration_cast<milliseconds>(cutoff.time_since_epoch()).count();
    st.oldFiles = queryInt(("SELECT COUNT(*) FROM files WHERE modified_at<" + std::to_string(cMs)).c_str());

    // Per-category counts and sizes
    const char* catSql = "SELECT category, COUNT(*), COALESCE(SUM(size_bytes),0) FROM files GROUP BY category";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, catSql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto cat = static_cast<FileCategory>(sqlite3_column_int(stmt, 0));
            st.countByCategory[cat] = sqlite3_column_int64(stmt, 1);
            st.sizeByCategory[cat]  = sqlite3_column_int64(stmt, 2);
        }
        sqlite3_finalize(stmt);
    }

    st.organizedTotal = queryInt("SELECT COUNT(*) FROM files WHERE is_processed=1");
    return st;
}

bool Database::updateScanTime(double durationMs) {
    if (!m_db) return false;
    std::lock_guard<std::mutex> lk(m_mutex);
    std::string sql = "INSERT OR REPLACE INTO statistics (key,value) VALUES "
                      "('last_scan_duration', '" + std::to_string(durationMs) + "')";
    return execSQL(sql);
}

bool Database::vacuum() {
    if (!m_db) return false;
    std::lock_guard<std::mutex> lk(m_mutex);
    return execSQL("VACUUM");
}

bool Database::pruneOldRecords(int olderThanDays) {
    if (!m_db) return false;
    std::lock_guard<std::mutex> lk(m_mutex);
    using namespace std::chrono;
    auto cutoff = system_clock::now() - hours(24LL * olderThanDays);
    int64_t cMs = duration_cast<milliseconds>(cutoff.time_since_epoch()).count();
    std::string sql = "DELETE FROM undo_log WHERE performed_at<" + std::to_string(cMs);
    return execSQL(sql);
}

uint64_t Database::getDatabaseSize() {
    if (m_path.empty()) return 0;
    try {
        return fs::file_size(m_path);
    } catch(...) { return 0; }
}

std::optional<FileInfo> Database::getFile(const std::string& path) {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (!m_db) return std::nullopt;
    const char* sql = "SELECT path,filename,extension,mime_type,size_bytes,sha256_hash,md5_hash,"
                      "category,status,is_processed,is_duplicate,duplicate_of,"
                      "created_at,modified_at,accessed_at,detected_at FROM files WHERE path=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return std::nullopt;
    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        FileInfo fi = rowToFileInfo(stmt);
        sqlite3_finalize(stmt);
        return fi;
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

} // namespace SDO
