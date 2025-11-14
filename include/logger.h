//
// Created by bytebard on 10/23/25.
//

#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <chrono>
#include <thread>

#include "utils.h"

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

inline const char * level_to_string(const LogLevel level) {
    switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::DEBUG: return "DEBUG";
    }
    return "UNKNOWN";
}

inline bool should_log(LogLevel current, LogLevel level) {
    return static_cast<int>(level) >= static_cast<int>(current);
}

class logger final {
private:
    LogLevel level = LogLevel::ERROR;
    std::string location = "Logger";
    std::ostream& stream = std::cout;
    std::mutex mutex_;

    static std::string get_current_thread_id() {
        std::ostringstream os;
        os << std::this_thread::get_id();
        return os.str();
    }
public:
    explicit logger(const LogLevel level = LogLevel::ERROR, std::string loc = "Logger", std::ostream& s = std::cout)
        : level(level), location(std::move(loc)), stream(s) {}
    virtual ~logger() = default;

    void log(const LogLevel level, const std::string& message) {
        /*if (!should_log(level, this->level)) {
            return;
        }
        const std::string formatted_message = format(level, message);
        std::lock_guard<std::mutex> lock(mutex_);
        stream << formatted_message << std::endl;*/
    }

    std::string format(const LogLevel level, const std::string &message) {
        auto now = std::chrono::system_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto f_time = std::format("{:%Y-%m-%d %H:%M:%S}.{:03}", now, ms.count());

        return std::format("{} [location:{}] [thread:{}] [{}] {}",
                           f_time, location, get_current_thread_id(), level_to_string(level), message);
    }

    void info(const std::string& message) {
        log(LogLevel::INFO, message);
    }

    template<typename... Args>
    void info(const std::string& message, Args... args) {
        std::string builder = " ";
        ((builder += to_string(args) + " "), ...);

        const auto log_line = message + builder;
        log(LogLevel::INFO, log_line);
    }

    void debug(const std::string& message) {
        log(LogLevel::DEBUG, message);
    }

    void error(const std::string& message) {
        log(LogLevel::ERROR, message);
    }

    void warn(const std::string& message) {
        log(LogLevel::WARN, message);
    }
};

#endif
