#include "logger.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace SDO {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(const std::string& logPath, LogLevel minLevel) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_logPath  = logPath;
    m_minLevel = minLevel;

    if (!logPath.empty()) {
        m_stream.open(logPath, std::ios::app);
        if (!m_stream.is_open()) {
            std::cerr << "[Logger] Could not open log file: " << logPath << "\n";
        }
    }
    // Write header
    if (m_stream.is_open()) {
        m_stream << "\n=== Smart Downloads Organizer Log Started ===\n";
        m_stream.flush();
    }
}

void Logger::setCallback(LogCb cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_callback = std::move(cb);
}

std::string Logger::levelStr(LogLevel lvl) const {
    switch (lvl) {
        case LogLevel::Debug:    return "DEBUG   ";
        case LogLevel::Info:     return "INFO    ";
        case LogLevel::Warning:  return "WARNING ";
        case LogLevel::Error:    return "ERROR   ";
        case LogLevel::Critical: return "CRITICAL";
        default:                 return "UNKNOWN ";
    }
}

std::string Logger::formatTimestamp(const std::chrono::system_clock::time_point& tp) const {
    auto t  = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  tp.time_since_epoch()) % 1000;
    std::tm tm_buf{};
    localtime_r(&t, &tm_buf);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void Logger::log(LogLevel level, const std::string& msg,
                 const std::string& detail, const std::string& file) {
    if (level < m_minLevel) return;

    LogEntry entry;
    entry.level     = level;
    entry.message   = msg;
    entry.detail    = detail;
    entry.filePath  = file;
    entry.timestamp = std::chrono::system_clock::now();

    std::string line;
    {
        std::ostringstream oss;
        oss << "[" << formatTimestamp(entry.timestamp) << "] "
            << "[" << levelStr(level) << "] " << msg;
        if (!detail.empty()) oss << " | " << detail;
        if (!file.empty())   oss << " | file=" << file;
        line = oss.str();
    }

    std::lock_guard<std::mutex> lk(m_mutex);

    // File log
    if (m_stream.is_open()) {
        m_stream << line << "\n";
        if (level >= LogLevel::Error) m_stream.flush();
    }

    // Console (errors always)
    if (level >= LogLevel::Warning) {
        std::cerr << line << "\n";
    }

    // Memory ring-buffer
    if (m_entries.size() >= MAX_ENTRIES) m_entries.pop_front();
    m_entries.push_back(entry);

    // UI callback (fire without holding lock to avoid deadlock)
    if (m_callback) {
        auto cb   = m_callback;
        auto ent  = entry;
        // unlock before calling
        m_mutex.unlock();
        cb(ent);
        m_mutex.lock();
    }
}

void Logger::debug   (const std::string& m, const std::string& d, const std::string& f) { log(LogLevel::Debug,    m, d, f); }
void Logger::info    (const std::string& m, const std::string& d, const std::string& f) { log(LogLevel::Info,     m, d, f); }
void Logger::warning (const std::string& m, const std::string& d, const std::string& f) { log(LogLevel::Warning,  m, d, f); }
void Logger::error   (const std::string& m, const std::string& d, const std::string& f) { log(LogLevel::Error,    m, d, f); }
void Logger::critical(const std::string& m, const std::string& d, const std::string& f) { log(LogLevel::Critical, m, d, f); }

const std::deque<LogEntry>& Logger::entries() const {
    return m_entries;
}

void Logger::clearEntries() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_entries.clear();
}

void Logger::flush() {
    std::lock_guard<std::mutex> lk(m_mutex);
    if (m_stream.is_open()) m_stream.flush();
}

std::string Logger::getLogPath() const {
    return m_logPath;
}

} // namespace SDO
