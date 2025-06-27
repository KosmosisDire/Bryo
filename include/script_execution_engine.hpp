#pragma once

#define NOMINMAX  // Prevent Windows min/max macros

#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <optional>
#include <variant>

#include "script_ast.hpp"
#include "parser/script_parser.hpp"
#include "compiler/script_compiler.hpp"
#include "common/logger.hpp"

namespace Mycelium::Execution {
    enum class ExecutionPhase {
        INPUT_READING,
        PARSING,
        SEMANTIC_ANALYSIS,
        COMPILATION,
        JIT_EXECUTION,
        COMPLETED
    };

    enum class ErrorType {
        FILE_READ_ERROR,
        PARSE_ERROR,
        SEMANTIC_ERROR,
        COMPILATION_ERROR,
        JIT_ERROR,
        RUNTIME_ERROR,
        UNKNOWN_ERROR
    };

    struct ExecutionError {
        ErrorType type;
        std::string message;
        std::string location;
        ExecutionPhase phase;
        
        ExecutionError(ErrorType t, const std::string& msg, const std::string& loc = "", ExecutionPhase p = ExecutionPhase::COMPLETED)
            : type(t), message(msg), location(loc), phase(p) {}
    };

    struct ExecutionTiming {
        std::chrono::milliseconds total_time{0};
        std::chrono::milliseconds parse_time{0};
        std::chrono::milliseconds compilation_time{0};
        std::chrono::milliseconds jit_time{0};
    };

    struct ExecutionOutput {
        std::string console_output;       // Captured console output from JIT execution
        std::string generated_ir;        // LLVM IR code
        std::optional<int> exit_code;    // Program exit code (if execution completed)
        std::string debug_info;          // Debug information and logs
    };

    struct ExecutionResult {
        std::string script_name;
        std::string script_path;
        bool succeeded;
        ExecutionPhase completed_phase;
        
        std::vector<ExecutionError> errors;
        std::vector<std::string> warnings;
        ExecutionTiming timing;
        ExecutionOutput output;
        
        // Metadata
        size_t source_lines{0};
        size_t source_chars{0};
        
        ExecutionResult(const std::string& name, const std::string& path = "")
            : script_name(name), script_path(path), succeeded(false), 
              completed_phase(ExecutionPhase::INPUT_READING) {}
        
        bool has_errors() const { return !errors.empty(); }
        bool has_warnings() const { return !warnings.empty(); }
        
        std::vector<ExecutionError> get_errors_by_type(ErrorType type) const {
            std::vector<ExecutionError> filtered;
            for (const auto& error : errors) {
                if (error.type == type) filtered.push_back(error);
            }
            return filtered;
        }
    };

    struct ExecutionConfig {
        LogLevel log_level = LogLevel::WARN;
        bool capture_console_output = true;
        bool save_ir_to_file = false;
        std::string ir_output_directory = "build";
        bool enable_timing = true;
        bool capture_debug_info = false;
        
        ExecutionConfig() = default;
        
        static ExecutionConfig silent() {
            ExecutionConfig config;
            config.log_level = LogLevel::NONE;
            config.capture_debug_info = false;
            return config;
        }
        
        static ExecutionConfig verbose() {
            ExecutionConfig config;
            config.log_level = LogLevel::DEBUG;
            config.capture_debug_info = true;
            return config;
        }
        
        static ExecutionConfig testing() {
            ExecutionConfig config;
            config.log_level = LogLevel::ERR;
            config.save_ir_to_file = true;
            config.capture_console_output = true;
            return config;
        }
    };

    class ScriptExecutionEngine {
    private:
        ExecutionConfig config_;
        std::string capture_buffer_;
        
        void log_message(LogLevel level, const std::string& message, const std::string& category = "EXECUTION");
        void add_timing(ExecutionResult& result, ExecutionPhase phase, std::chrono::milliseconds duration);
        void capture_console_output_start();
        std::string capture_console_output_end();
        
    public:
        explicit ScriptExecutionEngine(const ExecutionConfig& config = ExecutionConfig{});
        
        // Main execution methods
        ExecutionResult execute_file(const std::string& file_path);
        ExecutionResult execute_source(const std::string& source_code, const std::string& script_name = "inline");
        
        // Batch execution
        std::vector<ExecutionResult> execute_multiple_files(const std::vector<std::string>& file_paths);
        
        // Configuration
        void set_config(const ExecutionConfig& config) { config_ = config; }
        const ExecutionConfig& get_config() const { return config_; }
        
        // Utility methods for results
        static void print_execution_result(const ExecutionResult& result, bool verbose = false);
        static void save_execution_result(const ExecutionResult& result, const std::string& output_file);
        static std::string format_execution_summary(const std::vector<ExecutionResult>& results);
    };

} // namespace Mycelium::Execution