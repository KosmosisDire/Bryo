#define NOMINMAX  // Prevent Windows min/max macros
#include "script_execution_engine.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <numeric>

namespace Mycelium::Execution {

using namespace Mycelium::Scripting::Lang;
using namespace Mycelium::Scripting::Common;

ScriptExecutionEngine::ScriptExecutionEngine(const ExecutionConfig& config) 
    : config_(config) {}

void ScriptExecutionEngine::log_message(LogLevel level, const std::string& message, const std::string& category) {
    if (static_cast<int>(level) > static_cast<int>(config_.log_level)) {
        return; // Don't log if level is below threshold
    }
    
    if (config_.capture_debug_info) {
        // TODO: Capture to debug info instead of direct logging
    }
    
    // Only actually log to console/file if level permits
    Logger& logger = Logger::get_instance();
    switch (level) {
        case LogLevel::ERR:
            // Use LOG_WARN to avoid exceptions
            LOG_WARN("[ERROR] " + message, category);
            break;
        case LogLevel::WARN:
            LOG_WARN(message, category);
            break;
        case LogLevel::INFO:
            LOG_INFO(message, category);
            break;
        case LogLevel::DEBUG:
            LOG_DEBUG(message, category);
            break;
        case LogLevel::TRACE:
            LOG_TRACE(message, category);
            break;
        case LogLevel::NONE:
        default:
            break;
    }
}

void ScriptExecutionEngine::add_timing(ExecutionResult& result, ExecutionPhase phase, std::chrono::milliseconds duration) {
    if (!config_.enable_timing) return;
    
    switch (phase) {
        case ExecutionPhase::PARSING:
            result.timing.parse_time = duration;
            break;
        case ExecutionPhase::COMPILATION:
        case ExecutionPhase::SEMANTIC_ANALYSIS:
            result.timing.compilation_time += duration;
            break;
        case ExecutionPhase::JIT_EXECUTION:
            result.timing.jit_time = duration;
            break;
        default:
            break;
    }
}

void ScriptExecutionEngine::capture_console_output_start() {
    if (!config_.capture_console_output) return;
    // TODO: Implement console capture if needed
    capture_buffer_.clear();
}

std::string ScriptExecutionEngine::capture_console_output_end() {
    if (!config_.capture_console_output) return "";
    // TODO: Return captured console output
    return capture_buffer_;
}

ExecutionResult ScriptExecutionEngine::execute_file(const std::string& file_path) {
    std::string script_name = std::filesystem::path(file_path).stem().string();
    ExecutionResult result(script_name, file_path);
    
    auto total_start = std::chrono::high_resolution_clock::now();
    
    try {
        // Phase 1: Read file
        log_message(LogLevel::DEBUG, "Reading file: " + file_path);
        
        std::ifstream file(file_path);
        if (!file.is_open()) {
            result.errors.emplace_back(ErrorType::FILE_READ_ERROR, 
                "Cannot open file: " + file_path, "", ExecutionPhase::INPUT_READING);
            return result;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source_code = buffer.str();
        
        result.source_chars = source_code.length();
        result.source_lines = std::count(source_code.begin(), source_code.end(), '\n') + 1;
        
        log_message(LogLevel::DEBUG, "File read successfully (" + 
            std::to_string(result.source_chars) + " chars, " + 
            std::to_string(result.source_lines) + " lines)");
        
        // Continue with source execution
        ExecutionResult source_result = execute_source(source_code, script_name);
        
        // Copy results but preserve file-specific info
        source_result.script_path = file_path;
        source_result.source_chars = result.source_chars;
        source_result.source_lines = result.source_lines;
        
        return source_result;
        
    } catch (const std::exception& e) {
        result.errors.emplace_back(ErrorType::FILE_READ_ERROR, 
            "Exception reading file: " + std::string(e.what()), "", ExecutionPhase::INPUT_READING);
        return result;
    }
}

ExecutionResult ScriptExecutionEngine::execute_source(const std::string& source_code, const std::string& script_name) {
    ExecutionResult result(script_name);
    auto total_start = std::chrono::high_resolution_clock::now();
    
    if (result.source_chars == 0) {
        result.source_chars = source_code.length();
        result.source_lines = std::count(source_code.begin(), source_code.end(), '\n') + 1;
    }
    
    try {
        // Phase 2: Parsing
        log_message(LogLevel::DEBUG, "Starting parse phase for: " + script_name);
        auto parse_start = std::chrono::high_resolution_clock::now();
        
        ScriptParser parser(source_code, script_name);
        auto parse_result = parser.parse();
        auto AST = parse_result.first;
        
        auto parse_end = std::chrono::high_resolution_clock::now();
        add_timing(result, ExecutionPhase::PARSING, 
            std::chrono::duration_cast<std::chrono::milliseconds>(parse_end - parse_start));
        
        // Handle parse errors
        for (const auto& error : parse_result.second) {
            result.errors.emplace_back(ErrorType::PARSE_ERROR, 
                error.message, error.location.to_string(), ExecutionPhase::PARSING);
            log_message(LogLevel::WARN, "Parse error: " + error.message + " at " + error.location.to_string());
        }
        
        if (!AST) {
            result.completed_phase = ExecutionPhase::PARSING;
            if (result.errors.empty()) {
                result.errors.emplace_back(ErrorType::PARSE_ERROR, 
                    "Parser returned null AST without explicit errors", "", ExecutionPhase::PARSING);
            }
            return result;
        }
        
        log_message(LogLevel::DEBUG, "Parse phase completed successfully");
        result.completed_phase = ExecutionPhase::PARSING;
        
        // Phase 3: Compilation
        log_message(LogLevel::DEBUG, "Starting compilation phase for: " + script_name);
        auto compile_start = std::chrono::high_resolution_clock::now();
        
        ScriptCompiler compiler;
        
        try {
            compiler.compile_ast(AST, "Module_" + script_name);
            
            auto compile_end = std::chrono::high_resolution_clock::now();
            add_timing(result, ExecutionPhase::COMPILATION, 
                std::chrono::duration_cast<std::chrono::milliseconds>(compile_end - compile_start));
            
            log_message(LogLevel::DEBUG, "Compilation phase completed successfully");
            result.completed_phase = ExecutionPhase::COMPILATION;
            
            // Save IR if requested
            result.output.generated_ir = compiler.get_ir_string();
            if (config_.save_ir_to_file) {
                std::filesystem::create_directories(config_.ir_output_directory);
                std::string ir_file = config_.ir_output_directory + "/" + script_name + ".ll";
                std::ofstream outFile(ir_file);
                if (outFile) {
                    outFile << result.output.generated_ir;
                    outFile.close();
                    log_message(LogLevel::DEBUG, "IR saved to: " + ir_file);
                }
            }
            
            // Phase 4: JIT Execution
            log_message(LogLevel::DEBUG, "Starting JIT execution phase for: " + script_name);
            auto jit_start = std::chrono::high_resolution_clock::now();
            
            capture_console_output_start();
            
            try {
                auto jit_result = compiler.jit_execute_function("Program.Main", {});
                result.output.exit_code = static_cast<int>(jit_result.IntVal.getSExtValue());
                
                auto jit_end = std::chrono::high_resolution_clock::now();
                add_timing(result, ExecutionPhase::JIT_EXECUTION, 
                    std::chrono::duration_cast<std::chrono::milliseconds>(jit_end - jit_start));
                
                result.output.console_output = capture_console_output_end();
                
                log_message(LogLevel::DEBUG, "JIT execution completed, exit code: " + 
                    std::to_string(result.output.exit_code.value_or(-1)));
                
                result.completed_phase = ExecutionPhase::JIT_EXECUTION;
                result.succeeded = (result.output.exit_code.value_or(-1) == 0) && result.errors.empty();
                
            } catch (const std::exception& jit_error) {
                result.errors.emplace_back(ErrorType::JIT_ERROR, 
                    std::string(jit_error.what()), "", ExecutionPhase::JIT_EXECUTION);
                log_message(LogLevel::WARN, "JIT execution failed: " + std::string(jit_error.what()));
            }
            
        } catch (const std::runtime_error& compile_error) {
            auto compile_end = std::chrono::high_resolution_clock::now();
            add_timing(result, ExecutionPhase::COMPILATION, 
                std::chrono::duration_cast<std::chrono::milliseconds>(compile_end - compile_start));
            
            std::string error_msg = std::string(compile_error.what());
            if (error_msg.find("Semantic errors detected") != std::string::npos) {
                result.errors.emplace_back(ErrorType::SEMANTIC_ERROR, 
                    error_msg, "", ExecutionPhase::SEMANTIC_ANALYSIS);
                result.completed_phase = ExecutionPhase::SEMANTIC_ANALYSIS;
            } else {
                result.errors.emplace_back(ErrorType::COMPILATION_ERROR, 
                    error_msg, "", ExecutionPhase::COMPILATION);
                result.completed_phase = ExecutionPhase::COMPILATION;
            }
            log_message(LogLevel::WARN, "Compilation failed: " + error_msg);
        } catch (const std::exception& compile_exception) {
            result.errors.emplace_back(ErrorType::COMPILATION_ERROR, 
                std::string(compile_exception.what()), "", ExecutionPhase::COMPILATION);
            log_message(LogLevel::WARN, "Compilation exception: " + std::string(compile_exception.what()));
        }
        
    } catch (const std::exception& e) {
        result.errors.emplace_back(ErrorType::UNKNOWN_ERROR, 
            std::string(e.what()), "", result.completed_phase);
        log_message(LogLevel::ERR, "Unexpected error: " + std::string(e.what()));
    } catch (...) {
        result.errors.emplace_back(ErrorType::UNKNOWN_ERROR, 
            "Unknown exception occurred", "", result.completed_phase);
        log_message(LogLevel::ERR, "Unknown exception occurred");
    }
    
    auto total_end = std::chrono::high_resolution_clock::now();
    result.timing.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start);
    
    if (result.completed_phase == ExecutionPhase::JIT_EXECUTION) {
        result.completed_phase = ExecutionPhase::COMPLETED;
    }
    
    return result;
}

std::vector<ExecutionResult> ScriptExecutionEngine::execute_multiple_files(const std::vector<std::string>& file_paths) {
    std::vector<ExecutionResult> results;
    results.reserve(file_paths.size());
    
    log_message(LogLevel::INFO, "Starting batch execution of " + std::to_string(file_paths.size()) + " files");
    
    for (const std::string& file_path : file_paths) {
        ExecutionResult result = execute_file(file_path);
        results.push_back(std::move(result));
    }
    
    log_message(LogLevel::INFO, "Batch execution completed");
    return results;
}

void ScriptExecutionEngine::print_execution_result(const ExecutionResult& result, bool verbose) {
    std::cout << std::left << std::setw(25) << result.script_name;
    
    if (result.succeeded) {
        std::cout << "\033[32mPASSED\033[0m";
    } else {
        std::cout << "\033[31mFAILED\033[0m";
    }
    
    std::cout << " (" << result.timing.jit_time.count() << "ms)";
    
    if (result.output.exit_code.has_value()) {
        std::cout << " [exit: " << result.output.exit_code.value() << "]";
    }
    
    std::cout << std::endl;
    
    if (!result.succeeded && !result.errors.empty()) {
        for (const auto& error : result.errors) {
            std::cout << "  \033[31mError\033[0m: " << error.message;
            if (!error.location.empty()) {
                std::cout << " at " << error.location;
            }
            std::cout << std::endl;
        }
    }
    
    if (verbose) {
        if (!result.warnings.empty()) {
            for (const auto& warning : result.warnings) {
                std::cout << "  \033[33mWarning\033[0m: " << warning << std::endl;
            }
        }
        
        std::cout << "  Phase: ";
        switch (result.completed_phase) {
            case ExecutionPhase::INPUT_READING: std::cout << "Input Reading"; break;
            case ExecutionPhase::PARSING: std::cout << "Parsing"; break;
            case ExecutionPhase::SEMANTIC_ANALYSIS: std::cout << "Semantic Analysis"; break;
            case ExecutionPhase::COMPILATION: std::cout << "Compilation"; break;
            case ExecutionPhase::JIT_EXECUTION: std::cout << "JIT Execution"; break;
            case ExecutionPhase::COMPLETED: std::cout << "Completed"; break;
        }
        std::cout << std::endl;
        
        if (result.timing.total_time.count() > 0) {
            std::cout << "  Timing: Parse=" << result.timing.parse_time.count() << "ms, "
                      << "Compile=" << result.timing.compilation_time.count() << "ms, "
                      << "JIT=" << result.timing.jit_time.count() << "ms" << std::endl;
        }
    }
}

void ScriptExecutionEngine::save_execution_result(const ExecutionResult& result, const std::string& output_file) {
    std::ofstream file(output_file);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not save execution result to " << output_file << std::endl;
        return;
    }
    
    file << "Execution Result: " << result.script_name << std::endl;
    file << "Path: " << result.script_path << std::endl;
    file << "Status: " << (result.succeeded ? "PASSED" : "FAILED") << std::endl;
    file << "Total Time: " << result.timing.total_time.count() << "ms" << std::endl;
    
    if (result.output.exit_code.has_value()) {
        file << "Exit Code: " << result.output.exit_code.value() << std::endl;
    }
    
    file << "Completed Phase: ";
    switch (result.completed_phase) {
        case ExecutionPhase::INPUT_READING: file << "Input Reading"; break;
        case ExecutionPhase::PARSING: file << "Parsing"; break;
        case ExecutionPhase::SEMANTIC_ANALYSIS: file << "Semantic Analysis"; break;
        case ExecutionPhase::COMPILATION: file << "Compilation"; break;
        case ExecutionPhase::JIT_EXECUTION: file << "JIT Execution"; break;
        case ExecutionPhase::COMPLETED: file << "Completed"; break;
    }
    file << std::endl;
    
    if (!result.errors.empty()) {
        file << "\nErrors:" << std::endl;
        for (const auto& error : result.errors) {
            file << "  " << error.message;
            if (!error.location.empty()) {
                file << " at " << error.location;
            }
            file << std::endl;
        }
    }
    
    if (!result.warnings.empty()) {
        file << "\nWarnings:" << std::endl;
        for (const auto& warning : result.warnings) {
            file << "  " << warning << std::endl;
        }
    }
    
    if (!result.output.console_output.empty()) {
        file << "\nConsole Output:" << std::endl;
        file << result.output.console_output << std::endl;
    }
    
    file.close();
}

std::string ScriptExecutionEngine::format_execution_summary(const std::vector<ExecutionResult>& results) {
    std::ostringstream summary;
    
    int passed = 0, failed = 0;
    std::chrono::milliseconds total_time{0};
    
    for (const auto& result : results) {
        if (result.succeeded) passed++;
        else failed++;
        total_time += result.timing.total_time;
    }
    
    summary << std::string(60, '=') << std::endl;
    summary << "Execution Summary" << std::endl;
    summary << std::string(60, '=') << std::endl;
    summary << "Total: " << results.size() << " scripts, ";
    summary << "\033[32m" << passed << " passed\033[0m, ";
    summary << "\033[31m" << failed << " failed\033[0m" << std::endl;
    summary << "Total time: " << total_time.count() << "ms" << std::endl;
    summary << std::string(60, '=') << std::endl;
    
    return summary.str();
}

} // namespace Mycelium::Execution