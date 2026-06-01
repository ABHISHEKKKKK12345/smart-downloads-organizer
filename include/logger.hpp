#pragma once

#include "types.hpp"
#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <memory>
#include <deque>

namespace SDO {

class Logger {
public:
    static Logger& instance();

    void init(const std::string& logPath, LogLevel minLevel = LogLevel::Info);
    void setCallback(LogCb cb);

    void debug   (const std::string& msg, const std::string& detail = "", const std::string& file = "");
    void info    (const std::string& msg, const std::string& detail = "", const std::string& file = "");
    void warning (const std::string& msg, const std::string& detail = "", const std::string& file = "");
    void error   (const std::string& msg, const std::string& detail = "", const std::string& file = "");
    void critical(const std::string& msg, const std::string& detail = "", const std::string& file = "");

    const std::deque<LogEntry>& entries() const;
    void clearEntries();
    void flush();
    std::string getLogPath() const;

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, const std::string& msg, const std::string& detail, const std::string& file);
    std::string levelStr(LogLevel lvl) const;
    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) const;

    mutable std::mutex    m_mutex;
    std::ofstream         m_stream;
    std::string           m_logPath;
    LogLevel              m_minLevel  = LogLevel::Info;
    std::deque<LogEntry>  m_entries;
    LogCb                 m_callback;
    static constexpr size_t MAX_ENTRIES = 5000;
};

// Convenience macros
#define LOG_DEBUG(msg, ...)   SDO::Logger::instance().debug(msg, ##__VA_ARGS__)
#define LOG_INFO(msg, ...)    SDO::Logger::instance().info(msg, ##__VA_ARGS__)
#define LOG_WARN(msg, ...)    SDO::Logger::instance().warning(msg, ##__VA_ARGS__)
#define LOG_ERROR(msg, ...)   SDO::Logger::instance().error(msg, ##__VA_ARGS__)
#define LOG_CRITICAL(msg, ...) SDO::Logger::instance().critical(msg, ##__VA_ARGS__)

} // namespace SDO
