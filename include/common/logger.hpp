#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Mycelium
{
    enum class LogLevel
    {
        TRACE = 1,
        DEBUG = 2,
        INFO = 3,
        WARN = 4,
        ERR = 5,
        FATAL = 6,
        RUNTIME = 7, // Special level for runtime logs
        NONE = 8
    };
}

namespace Mycelium::Scripting::Common {



class Logger {
private:
    static std::unique_ptr<Logger> instance_;
    static std::mutex instance_mutex_;
    
    std::ofstream log_file_;
    std::mutex log_mutex_;
    LogLevel min_console_level_;
    LogLevel min_file_level_;
    bool initialized_;

    Logger() : min_console_level_(LogLevel::INFO), min_file_level_(LogLevel::TRACE), initialized_(false) {}

    std::string get_timestamp() const;
    std::string level_to_string(LogLevel level) const;
    std::string get_color_code(LogLevel level) const;
    std::string get_reset_color() const;

public:
    static Logger& get_instance();
    
    // Initialize the logger with a log file path
    bool initialize(const std::string& log_file_path);
    
    // Set minimum log levels for console and file output
    void set_console_level(LogLevel level) { min_console_level_ = level; }
    void set_file_level(LogLevel level) { min_file_level_ = level; }
    
    // Core logging function
    void log(LogLevel level, const std::string& message, const std::string& category = "");
    
    // Convenience methods for different log levels
    void runtime(const std::string& message, const std::string& category = "");
    void trace(const std::string& message, const std::string& category = "");
    void debug(const std::string& message, const std::string& category = "");
    void info(const std::string& message, const std::string& category = "");
    void warn(const std::string& message, const std::string& category = "");
    void error(const std::string& message, const std::string& category = "");
    void fatal(const std::string& message, const std::string& category = "");
    
    // Flush all outputs
    void flush();
    
    // Explicit shutdown method for graceful cleanup
    void shutdown();
    
    // Cleanup
    ~Logger();
};

// Convenience macros for easier logging
#define LOG_RUNTIME(msg, ...) Mycelium::Scripting::Common::Logger::get_instance().runtime(msg, ##__VA_ARGS__)
#define LOG_TRACE(msg, ...) Mycelium::Scripting::Common::Logger::get_instance().trace(msg, ##__VA_ARGS__)
#define LOG_DEBUG(msg, ...) Mycelium::Scripting::Common::Logger::get_instance().debug(msg, ##__VA_ARGS__)
#define LOG_INFO(msg, ...) Mycelium::Scripting::Common::Logger::get_instance().info(msg, ##__VA_ARGS__)
#define LOG_WARN(msg, ...) Mycelium::Scripting::Common::Logger::get_instance().warn(msg, ##__VA_ARGS__)
#define LOG_ERROR(msg, ...) Mycelium::Scripting::Common::Logger::get_instance().error(msg, ##__VA_ARGS__)
#define LOG_FATAL(msg, ...) Mycelium::Scripting::Common::Logger::get_instance().fatal(msg, ##__VA_ARGS__)

} // namespace Mycelium::Scripting::Common
