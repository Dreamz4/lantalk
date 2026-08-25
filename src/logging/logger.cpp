#include "logger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace lantalk {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    if (fileStream_ && fileStream_->is_open()) {
        fileStream_->close();
    }
}

void Logger::setLevel(LogLevel level) {
    level_ = level;
}

LogLevel Logger::getLevel() const {
    return level_.load();
}

void Logger::setOutputFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fileStream_ && fileStream_->is_open()) {
        fileStream_->close();
    }
    fileStream_.emplace(path, std::ios::app);
}

void Logger::setQuiet(bool quiet) {
    quiet_ = quiet;
}

void Logger::log(LogLevel level, std::string_view message, const std::source_location& loc) {
    if (level < level_.load()) {
        return;
    }

    std::string formatted = formatMessage(level, message, loc);

    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!quiet_.load()) {
        std::string colorPrefix = "";
        std::string colorSuffix = "";
        #ifndef _WIN32
        if (level == LogLevel::ERROR) {
            colorPrefix = "\033[31m"; // Red
            colorSuffix = "\033[0m";
        } else if (level == LogLevel::WARN) {
            colorPrefix = "\033[33m"; // Yellow
            colorSuffix = "\033[0m";
        }
        #endif
        std::cout << colorPrefix << formatted << colorSuffix << std::endl;
    }

    if (fileStream_ && fileStream_->is_open()) {
        *fileStream_ << formatted << std::endl;
    }
}

void Logger::trace(std::string_view msg, const std::source_location& loc) {
    log(LogLevel::TRACE, msg, loc);
}
void Logger::debug(std::string_view msg, const std::source_location& loc) {
    log(LogLevel::DEBUG, msg, loc);
}
void Logger::info(std::string_view msg, const std::source_location& loc) {
    log(LogLevel::INFO, msg, loc);
}
void Logger::warn(std::string_view msg, const std::source_location& loc) {
    log(LogLevel::WARN, msg, loc);
}
void Logger::error(std::string_view msg, const std::source_location& loc) {
    log(LogLevel::ERROR, msg, loc);
}

std::string Logger::formatMessage(LogLevel level, std::string_view message, const std::source_location& loc) const {
    std::ostringstream oss;
    oss << "[" << currentTimestamp() << "] "
        << "[" << levelToString(level) << "] "
        << message << " (" 
        << loc.file_name() << ":" << loc.line() << ")";
    return oss.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::OFF:   return "OFF";
        default:              return "UNKNOWN";
    }
}

std::string Logger::currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm bt{};
#ifdef _WIN32
    localtime_s(&bt, &now_time_t);
#else
    localtime_r(&now_time_t, &bt);
#endif

    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
    return oss.str();
}

} // namespace lantalk
