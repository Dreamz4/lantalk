#pragma once
#include <string>
#include <string_view>
#include <atomic>
#include <mutex>
#include <fstream>
#include <optional>
#include <chrono>
#include <source_location>

namespace lantalk {

enum class LogLevel : int {
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERROR = 4,
    OFF   = 5
};

class Logger {
public:
    static Logger& instance();

    void setLevel(LogLevel level);
    LogLevel getLevel() const;
    void setOutputFile(const std::string& path);
    void setQuiet(bool quiet);

    void log(LogLevel level, std::string_view message,
             const std::source_location& loc = std::source_location::current());

    // Convenience
    void trace(std::string_view msg, const std::source_location& loc = std::source_location::current());
    void debug(std::string_view msg, const std::source_location& loc = std::source_location::current());
    void info(std::string_view msg, const std::source_location& loc = std::source_location::current());
    void warn(std::string_view msg, const std::source_location& loc = std::source_location::current());
    void error(std::string_view msg, const std::source_location& loc = std::source_location::current());

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    mutable std::mutex mutex_;
    std::atomic<LogLevel> level_{LogLevel::INFO};
    std::atomic<bool> quiet_{false};
    std::optional<std::ofstream> fileStream_;

    std::string formatMessage(LogLevel level, std::string_view message,
                               const std::source_location& loc) const;
    static std::string levelToString(LogLevel level);
    static std::string currentTimestamp();
};

// Macros for convenient logging
#define LOG_TRACE(msg) lantalk::Logger::instance().trace(msg)
#define LOG_DEBUG(msg) lantalk::Logger::instance().debug(msg)
#define LOG_INFO(msg)  lantalk::Logger::instance().info(msg)
#define LOG_WARN(msg)  lantalk::Logger::instance().warn(msg)
#define LOG_ERROR(msg) lantalk::Logger::instance().error(msg)

} // namespace lantalk
