#include "common/logger.hpp"
#include <filesystem>

namespace Mycelium::Scripting::Common {

std::unique_ptr<Logger> Logger::instance_ = nullptr;
std::mutex Logger::instance_mutex_;

Logger& Logger::get_instance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = std::unique_ptr<Logger>(new Logger());
    }
    return *instance_;
}

bool Logger::initialize(const std::string& log_file_path) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    // Create directory if it doesn't exist
    auto parent_path = std::filesystem::path(log_file_path).parent_path();
    if (!parent_path.empty()) {
        std::filesystem::create_directories(parent_path);
    }
    
    // Open log file
    log_file_.open(log_file_path, std::ios::out | std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log file: " << log_file_path << std::endl;
        return false;
    }
    
    initialized_ = true;
    
    // Log initialization
    log_file_ << "\n" << std::string(80, '=') << "\n";
    log_file_ << "LOGGER INITIALIZED AT " << get_timestamp() << "\n";
    log_file_ << std::string(80, '=') << "\n\n";
    log_file_.flush();
    
    return true;
}

std::string Logger::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
#ifdef _WIN32
    // Use localtime_s on Windows to avoid deprecation warning
    std::tm tm_buf;
    localtime_s(&tm_buf, &time_t);
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
#else
    // Use localtime_r on POSIX systems
    std::tm tm_buf;
    localtime_r(&time_t, &tm_buf);
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
#endif
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string Logger::level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::RUNTIME: return "RUNTIME";
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::get_color_code(LogLevel level) const {
    switch (level) {
        case LogLevel::RUNTIME: return "\033[90m";  // Bright Black (Gray)
        case LogLevel::TRACE: return "\033[37m";   // White
        case LogLevel::DEBUG: return "\033[36m";   // Cyan
        case LogLevel::INFO:  return "\033[32m";   // Green
        case LogLevel::WARN:  return "\033[33m";   // Yellow
        case LogLevel::ERR: return "\033[31m";   // Red
        case LogLevel::FATAL: return "\033[35m";   // Magenta
        default: return "";
    }
}

std::string Logger::get_reset_color() const {
    return "\033[0m";
}

void Logger::log(LogLevel level, const std::string& message, const std::string& category) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    std::string timestamp = get_timestamp();
    std::string level_str = level_to_string(level);
    std::string category_str = category.empty() ? "" : " [" + category + "]";
    
    // Log to file if initialized and level is sufficient
    if (initialized_ && level >= min_file_level_) {
        log_file_ << level_str << category_str << ": " << message << "\n";
        log_file_.flush();
    }
    
    // Log to console if level is sufficient
    if (level >= min_console_level_) {
        std::string color = get_color_code(level);
        std::string reset = get_reset_color();
        
        std::ostream& output = (level >= LogLevel::ERR) ? std::cerr : std::cout;
        if (!category.empty()) {
            output << color << category_str << ": " << message << reset << std::endl;
        } else {
            output << color << message << reset << std::endl;
        }
        
    }
}

void Logger::runtime(const std::string& message, const std::string& category) {
    log(LogLevel::RUNTIME, message, category);
}

void Logger::trace(const std::string& message, const std::string& category) {
    log(LogLevel::TRACE, message, category);
}

void Logger::debug(const std::string& message, const std::string& category) {
    log(LogLevel::DEBUG, message, category);
}

void Logger::info(const std::string& message, const std::string& category) {
    log(LogLevel::INFO, message, category);
}

void Logger::warn(const std::string& message, const std::string& category) {
    log(LogLevel::WARN, message, category);
}

void Logger::error(const std::string& message, const std::string& category) {
    log(LogLevel::ERR, message, category);
}

void Logger::fatal(const std::string& message, const std::string& category) {
    log(LogLevel::FATAL, message, category);
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (initialized_) {
        log_file_.flush();
    }
    std::cout.flush();
    std::cerr.flush();
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (initialized_ && log_file_.is_open()) {
        log_file_ << "\n" << std::string(80, '=') << "\n";
        log_file_ << "LOGGER EXPLICIT SHUTDOWN AT " << get_timestamp() << "\n";
        log_file_ << std::string(80, '=') << "\n\n";
        log_file_.flush();
        log_file_.close();
        initialized_ = false;
    }
}

Logger::~Logger() {
    // If shutdown() wasn't called explicitly, do cleanup
    if (initialized_ && log_file_.is_open()) {
        log_file_ << "\n" << std::string(80, '=') << "\n";
        log_file_ << "LOGGER DESTRUCTOR SHUTDOWN AT " << get_timestamp() << "\n";
        log_file_ << std::string(80, '=') << "\n\n";
        log_file_.close();
    }
}

} // namespace Mycelium::Scripting::Common
