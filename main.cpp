#define NOMINMAX  // Prevent Windows min/max macros
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <filesystem>
#include <signal.h>
#include <atomic>
#include <cstring>
#include <algorithm>

#include "sharpie/common/logger.hpp"
#include "hot_reload.hpp"
#include "platform.hpp"
#include "script_execution_engine.hpp"
#include "test_runner.hpp"

#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/ManagedStatic.h"

using namespace Mycelium::Scripting::Common;
using namespace Mycelium::Execution;
using namespace Mycelium::Testing;

// Global flag for graceful shutdown
std::atomic<bool> should_exit{false};

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    std::cout << "\nReceived shutdown signal (" << signal << "). Exiting gracefully..." << std::endl;
    should_exit.store(true);
}

// C binding functions for runtime logging
extern "C" {
    void runtime_log_debug(const char* message) {
        Logger& logger = Logger::get_instance();
        LOG_DEBUG(std::string(message), "RUNTIME");
    }
    
    void runtime_log_info(const char* message) {
        Logger& logger = Logger::get_instance();
        LOG_INFO(std::string(message), "RUNTIME");
    }
    
    void runtime_log_warn(const char* message) {
        Logger& logger = Logger::get_instance();
        LOG_WARN(std::string(message), "RUNTIME");
    }
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n";
    std::cout << "\nModes:\n";
    std::cout << "  --test                Run test suite only (exit after tests)\n";
    std::cout << "  --execute FILE        Execute a single script file\n";
    std::cout << "  --watch FILE          Watch and hot-reload a single file\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --test-pattern PATTERN   Specify test file pattern (default: tests/*.sp)\n";
    std::cout << "  --verbose                Enable verbose output\n";
    std::cout << "  --silent                 Minimal output (errors only)\n";
    std::cout << "  --log-level LEVEL        Set logging level (NONE|ERROR|WARN|INFO|DEBUG|TRACE)\n";
    std::cout << "  --no-save                Don't save IR files or test results\n";
    std::cout << "  --help                   Show this help message\n";
    std::cout << "\nDefault behavior: Run all tests, then hot-reload tests/test.sp\n";
}

struct CommandLineArgs {
    enum class Mode {
        DEFAULT,        // Run tests then hot-reload
        TEST_ONLY,      // Run tests and exit
        EXECUTE_FILE,   // Execute single file and exit
        WATCH_FILE      // Watch single file for changes
    };
    
    Mode mode = Mode::DEFAULT;
    bool verbose = false;
    bool silent = false;
    bool save_outputs = true;
    bool show_help = false;
    Mycelium::LogLevel log_level = Mycelium::LogLevel::WARN;
    std::vector<std::string> test_patterns = {"tests/*.sp"};
    std::string target_file = "tests/test.sp";
};

Mycelium::LogLevel parse_log_level(const std::string& level_str) {
    std::string upper_level = level_str;
    std::transform(upper_level.begin(), upper_level.end(), upper_level.begin(), ::toupper);
    
    if (upper_level == "NONE") return Mycelium::LogLevel::NONE;
    if (upper_level == "ERROR") return Mycelium::LogLevel::ERR;
    if (upper_level == "WARN") return Mycelium::LogLevel::WARN;
    if (upper_level == "INFO") return Mycelium::LogLevel::INFO;
    if (upper_level == "DEBUG") return Mycelium::LogLevel::DEBUG;
    if (upper_level == "TRACE") return Mycelium::LogLevel::TRACE;
    
    std::cerr << "Warning: Unknown log level '" << level_str << "', using WARN" << std::endl;
    return Mycelium::LogLevel::WARN;
}

CommandLineArgs parse_args(int argc, char* argv[]) {
    CommandLineArgs args;
    
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--test") == 0) {
            args.mode = CommandLineArgs::Mode::TEST_ONLY;
        }
        else if (std::strcmp(argv[i], "--execute") == 0) {
            if (i + 1 < argc) {
                args.mode = CommandLineArgs::Mode::EXECUTE_FILE;
                args.target_file = argv[++i];
            } else {
                std::cerr << "Error: --execute requires a file argument\n";
                args.show_help = true;
            }
        }
        else if (std::strcmp(argv[i], "--watch") == 0) {
            if (i + 1 < argc) {
                args.mode = CommandLineArgs::Mode::WATCH_FILE;
                args.target_file = argv[++i];
            } else {
                std::cerr << "Error: --watch requires a file argument\n";
                args.show_help = true;
            }
        }
        else if (std::strcmp(argv[i], "--test-pattern") == 0) {
            if (i + 1 < argc) {
                args.test_patterns.clear();
                args.test_patterns.push_back(argv[++i]);
            } else {
                std::cerr << "Error: --test-pattern requires a pattern argument\n";
                args.show_help = true;
            }
        }
        else if (std::strcmp(argv[i], "--verbose") == 0) {
            args.verbose = true;
            if (args.log_level < Mycelium::LogLevel::INFO) args.log_level = Mycelium::LogLevel::INFO;
        }
        else if (std::strcmp(argv[i], "--silent") == 0) {
            args.silent = true;
            args.log_level = Mycelium::LogLevel::ERR;
        }
        else if (std::strcmp(argv[i], "--log-level") == 0) {
            if (i + 1 < argc) {
                args.log_level = parse_log_level(argv[++i]);
            } else {
                std::cerr << "Error: --log-level requires a level argument\n";
                args.show_help = true;
            }
        }
        else if (std::strcmp(argv[i], "--no-save") == 0) {
            args.save_outputs = false;
        }
        else if (std::strcmp(argv[i], "--help") == 0) {
            args.show_help = true;
        }
        else {
            std::cerr << "Error: Unknown option " << argv[i] << "\n";
            args.show_help = true;
        }
    }
    
    return args;
}

// RAII wrapper for application resources
class ApplicationContext {
public:
    ApplicationContext() {
        // Initialize LLVM
        llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
        
        // Initialize signal handlers
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        // Initialize logger
        std::filesystem::create_directories("logs");
        Logger& logger = Logger::get_instance();
        
        if (!logger.initialize("logs/sharpie_detailed.log")) {
            std::cerr << "Failed to initialize logger. Continuing without file logging..." << std::endl;
        }
        
        logger.set_console_level(Mycelium::LogLevel::WARN);
        logger.set_file_level(Mycelium::LogLevel::TRACE);
        
        LOG_INFO("Sharpie application started", "MAIN");
        LOG_DEBUG("Logger initialized successfully", "MAIN");
    }
    
    void set_log_level(Mycelium::LogLevel level) {
        Logger& logger = Logger::get_instance();
        
        // Map ExecutionLogLevel to Logger's LogLevel  
        // For NONE, we actually want to suppress all logging, so use ERROR level
        Mycelium::LogLevel logger_level;
        switch (level) {
            case Mycelium::LogLevel::NONE:
                logger_level = Mycelium::LogLevel::ERR;
                break;
            case Mycelium::LogLevel::ERR:
                logger_level = Mycelium::LogLevel::ERR;
                break;
            case Mycelium::LogLevel::WARN:
                logger_level = Mycelium::LogLevel::WARN;
                break;
            case Mycelium::LogLevel::INFO:
                logger_level = Mycelium::LogLevel::INFO;
                break;
            case Mycelium::LogLevel::DEBUG:
                logger_level = Mycelium::LogLevel::DEBUG;
                break;
            case Mycelium::LogLevel::TRACE:
                logger_level = Mycelium::LogLevel::TRACE;
                break;
            default:
                logger_level = Mycelium::LogLevel::WARN;
                break;
        }
        
        logger.set_console_level(logger_level);
    }
    
    ~ApplicationContext() {
        LOG_INFO("Shutting down Sharpie application", "MAIN");
        
        // Cleanup logger
        Logger& logger = Logger::get_instance();
        logger.shutdown();
        
        // Cleanup LLVM
        llvm::llvm_shutdown();
        
        std::cout << "Application shutdown complete." << std::endl;
    }
};

int run_test_suite(const CommandLineArgs& args) {
    TestSuiteConfig test_config;
    test_config.test_patterns = args.test_patterns;
    test_config.log_level = args.log_level;
    test_config.verbose_output = args.verbose;
    test_config.save_individual_results = args.save_outputs;
    test_config.save_summary = args.save_outputs;
    
    if (args.silent) {
        test_config = TestSuiteConfig::silent();
    } else if (args.verbose) {
        test_config.verbose_output = true;
    }
    
    UnifiedTestRunner runner(test_config);
    TestSuiteResult suite = runner.run_test_suite();
    
    if (!args.silent) {
        runner.print_test_suite_results(suite);
    }
    
    if (args.save_outputs) {
        runner.save_test_suite_results(suite);
    }
    
    return suite.all_passed() ? 0 : 1;
}

int execute_single_file(const std::string& file_path, const CommandLineArgs& args) {
    ExecutionConfig config;
    config.log_level = args.log_level;
    config.save_ir_to_file = args.save_outputs;
    config.capture_console_output = true;
    
    ScriptExecutionEngine engine(config);
    ExecutionResult result = engine.execute_file(file_path);
    
    if (!args.silent) {
        ScriptExecutionEngine::print_execution_result(result, args.verbose);
        
        if (!result.output.console_output.empty()) {
            std::cout << "\nProgram Output:" << std::endl;
            std::cout << result.output.console_output << std::endl;
        }
    }
    
    if (args.save_outputs) {
        std::string result_file = "build/" + result.script_name + "_result.txt";
        std::filesystem::create_directories("build");
        ScriptExecutionEngine::save_execution_result(result, result_file);
        
        if (args.verbose) {
            std::cout << "Execution result saved to: " << result_file << std::endl;
        }
    }
    
    return result.succeeded ? 0 : 1;
}

void watch_file_for_changes(const std::string& file_path, const CommandLineArgs& args) {
    ExecutionConfig config;
    config.log_level = args.log_level;
    config.save_ir_to_file = args.save_outputs;
    config.capture_console_output = true;
    
    ScriptExecutionEngine engine(config);
    
    HotReload fileReloader({file_path}, [&engine, &args](const std::string& filePath, const std::string& newContent) {
        LOG_INFO("File reloaded: " + filePath, "HOTRELOAD");
        
        if (!args.silent) {
            std::cout << "\n\033[33m--- " << filePath << " Reloaded ---\033[0m" << std::endl;
        }
        
        ExecutionResult result = engine.execute_source(newContent, 
            std::filesystem::path(filePath).stem().string());
        
        if (!args.silent) {
            ScriptExecutionEngine::print_execution_result(result, args.verbose);
            
            if (!result.output.console_output.empty()) {
                std::cout << "\nProgram Output:" << std::endl;
                std::cout << result.output.console_output << std::endl;
            }
        }
        
        std::cout << std::endl; // Add spacing after execution
    });

    LOG_INFO("Starting hot reload monitoring for: " + file_path, "MAIN");
    if (!args.silent) {
        std::cout << "Watching " << file_path << " for changes (Ctrl+C to exit)..." << std::endl;
    }
    
    fileReloader.poll_changes(); // Initial execution

    // Main event loop with graceful shutdown support
    while (!should_exit.load()) {
        fileReloader.poll_changes();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main(int argc, char* argv[]) {
    try {
        ApplicationContext app_context;
        
        // Parse command line arguments
        CommandLineArgs args = parse_args(argc, argv);
        
        if (args.show_help) {
            print_usage(argv[0]);
            return 0;
        }
        
        // Set logging level
        app_context.set_log_level(args.log_level);
        
        switch (args.mode) {
            case CommandLineArgs::Mode::TEST_ONLY: {
                if (!args.silent) {
                    std::cout << "Running Sharpie Test Suite...\n" << std::endl;
                }
                return run_test_suite(args);
            }
            
            case CommandLineArgs::Mode::EXECUTE_FILE: {
                if (!args.silent) {
                    std::cout << "Executing: " << args.target_file << "\n" << std::endl;
                }
                return execute_single_file(args.target_file, args);
            }
            
            case CommandLineArgs::Mode::WATCH_FILE: {
                watch_file_for_changes(args.target_file, args);
                return 0;
            }
            
            case CommandLineArgs::Mode::DEFAULT: {
                // First run test suite
                if (!args.silent) {
                    std::cout << "Running Sharpie Test Suite...\n" << std::endl;
                }
                
                int test_result = run_test_suite(args);
                
                if (!args.silent) {
                    std::cout << "\nTransitioning to hot-reload mode for " << args.target_file << "...\n" << std::endl;
                }
                
                // Then start hot-reload mode
                watch_file_for_changes(args.target_file, args);
                
                return test_result; // Return test results as exit code
            }
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error in main" << std::endl;
        return 1;
    }
}