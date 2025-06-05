#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <optional>
#include <fstream>
#include <filesystem>

// #include "script_tokenizer.hpp"
#include "sharpie/script_ast.hpp" // Updated path
#include "sharpie/parser/script_parser.hpp" // Updated path
#include "sharpie/compiler/script_compiler.hpp" // Updated path
#include "sharpie/common/logger.hpp" // Add logger include
#include "hot_reload.hpp"
#include "platform.hpp"

#include "llvm/Support/DynamicLibrary.h" // <--- ADD THIS INCLUDE

// using namespace Mycelium::UI::Lang;
using namespace Mycelium::Scripting::Lang;
using namespace Mycelium::Scripting::Common;

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

void Compile(std::string input)
{
    Logger& logger = Logger::get_instance();
    
    // Log the input source code
    LOG_PHASE_BEGIN("Input Source");
    LOG_DEBUG("Source code content:\n" + input, "INPUT");
    LOG_PHASE_END("Input Source", true);

    try
    {
        // Parsing Phase
        LOG_PHASE_BEGIN("Parsing");
        LOG_DEBUG("Creating parser for test.sp", "PARSER");
        ScriptParser parser(input, "test.sp");
        
        LOG_DEBUG("Starting parse operation", "PARSER");
        auto result = parser.parse();
        auto AST = result.first;
        
        // Handle parse errors
        bool has_errors = false;
        for (const auto &error : result.second)
        {
            LOG_ERROR("Parse Error: " + error.message + " at " + error.location.to_string(), "PARSER");
            has_errors = true;
        }
        
        if (!result.second.empty() && !AST) {
            LOG_ERROR("Parsing failed to produce an AST due to errors", "PARSER");
            LOG_PHASE_END("Parsing", false);
            return;
        }
        
        if (!AST) {
            LOG_ERROR("Parsing produced a null AST without explicit errors. Aborting compilation", "PARSER");
            LOG_PHASE_END("Parsing", false);
            return;
        }
        
        LOG_DEBUG("AST created successfully", "PARSER");
        LOG_PHASE_END("Parsing", true);

        // Compilation Phase
        LOG_PHASE_BEGIN("Compilation");
        LOG_DEBUG("Creating compiler instance", "COMPILER");
        ScriptCompiler compiler;
        
        LOG_DEBUG("Compiling AST to LLVM IR", "COMPILER");
        compiler.compile_ast(AST, "MyceliumModule");

        // Save IR to file
        LOG_DEBUG("Saving LLVM IR to tests/build/test.ll", "COMPILER");
        std::ofstream outFile("tests/build/test.ll");
        if (outFile)
        {
            outFile << compiler.get_ir_string();
            outFile.close();
            LOG_DEBUG("LLVM IR saved successfully", "COMPILER");
        }
        else
        {
            LOG_WARN("Failed to save LLVM IR to file", "COMPILER");
        }

        LOG_PHASE_END("Compilation", true);

        // JIT Execution Phase
        LOG_PHASE_BEGIN("JIT Execution");
        LOG_DEBUG("Starting JIT execution of main function", "JIT");
        
        // Capture and redirect JIT output
        auto value = compiler.jit_execute_function("main", {});
        
        LOG_JIT_OUTPUT("Program returned: " + std::to_string(value.IntVal.getSExtValue()));
        LOG_DEBUG("JIT execution completed successfully", "JIT");
        LOG_PHASE_END("JIT Execution", true);

    }
    catch (const std::runtime_error &e)
    {
        LOG_ERROR("Runtime error during compilation/JIT: " + std::string(e.what()), "RUNTIME");
        LOG_PHASE_END("Compilation/JIT", false);
    }
    catch (const std::exception &e)
    {
        LOG_ERROR("Unexpected standard exception: " + std::string(e.what()), "EXCEPTION");
        LOG_PHASE_END("Compilation/JIT", false);
    }
    
    logger.flush();
}

int main()
{
    // --- ADD THIS LINE AT THE VERY BEGINNING OF main() ---
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
    // ------------------------------------------------------

    // Initialize the logger
    Logger& logger = Logger::get_instance();
    
    // Create logs directory if it doesn't exist
    std::filesystem::create_directories("logs");
    
    // Initialize logger with detailed log file
    if (!logger.initialize("logs/sharpie_detailed.log")) {
        std::cerr << "Failed to initialize logger. Continuing without file logging..." << std::endl;
    }
    
    // Set log levels - show only WARN and above on console (errors), TRACE and above in file
    logger.set_console_level(LogLevel::WARN);
    logger.set_file_level(LogLevel::TRACE);
    
    LOG_INFO("Sharpie compiler started", "MAIN");
    LOG_DEBUG("Logger initialized successfully", "MAIN");

    HotReload fileReloader({"tests/test.sp"}, [](const std::string &filePath, const std::string &newContent)
    {
        LOG_INFO("File reloaded: " + filePath, "HOTRELOAD");
        LOG_DEBUG("File content length: " + std::to_string(newContent.length()) + " characters", "HOTRELOAD");
        
        // Display simple console message for user feedback
        std::cout << "\n\033[33m--- " << filePath << " Reloaded ---\033[0m" << std::endl;
        
        Compile(newContent);
        
        std::cout << std::endl; // Add spacing after compilation
    });

    LOG_INFO("Starting hot reload monitoring", "MAIN");
    fileReloader.poll_changes(); // Initial compile

    while (true)
    {
        fileReloader.poll_changes();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
