#include "compiler.hpp"
#include "test_runner.hpp"
// #include "semantic/symbol_table.hpp"
// #include "semantic/type_system.hpp"
// #include "semantic/type_resolver.hpp"
// #include "semantic/symbol_table_builder.hpp"
#include "parser/lexer.hpp"
#include "parser/parser.hpp"
#include "common/logger.hpp"
#include "ast/ast.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <algorithm>

using namespace Fern; 

std::string read_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void show_help(const std::string& program_name) {
    std::cout << "Usage: " << program_name << " [options] <source files>\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h          Show this help message\n";
    std::cout << "  --test, -t [dir]    Run tests in the specified directory (default: tests)\n";
    std::cout << "\nIf no source files are provided, the compiler will attempt to compile 'simple.fern'.\n";
}

int main(int argc, char* argv[])
{
    Logger& logger = Logger::get_instance();
    logger.initialize();
    #ifdef FERN_DEBUG
        logger.set_console_level(LogLevel::TRACE);
    #else
        logger.set_console_level(LogLevel::NONE);
    #endif

    #ifdef FERN_DEBUG
    // Check for --test argument
    if (argc > 1 && (std::strcmp(argv[1], "--test") == 0 || std::strcmp(argv[1], "-t") == 0)) {
        // Test mode - run all tests in the tests directory
        std::string test_dir = "tests";
        if (argc > 2) {
            test_dir = argv[2];
        }

        TestRunner runner;
        auto results = runner.run_all_tests(test_dir);
        runner.print_summary(results);

        // Return 0 if all tests passed, 1 otherwise
        bool all_passed = std::all_of(results.begin(), results.end(),
            [](const TestResult& r) { return r.passed; });
        return all_passed ? 0 : 1;
    }
    #endif

    Compiler compiler;
    #ifdef FERN_DEBUG
        compiler.set_print_ast(true);
        compiler.set_print_symbols(true);
        compiler.set_print_hlir(true);
    #endif

    // Use command line arguments if provided, otherwise default to simple.fern
    std::vector<std::string> filenames;
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            filenames.push_back(argv[i]);
        }
    } else
    {
        #ifdef FERN_DEBUG
        filenames = {"minimal.fern", "runtime/basic_print.fern"};
        #else
        // show help
        show_help(argv[0]);
        return 0;
        #endif
    }
    
    std::vector<SourceFile> source_files;
    for (const auto& filename : filenames)
    {
        auto source = read_file(filename);
        source_files.push_back({filename, source});
    }

    auto result = compiler.compile(source_files);

    if (result && result->is_valid())
    {
        #ifdef FERN_DEBUG
            result->dump_ir();
            result->write_object_file("out/output.o");
        #endif
        auto ret = result->execute_jit<float>("Main").value_or(-1.0f);
        std::cout << "\n";
        std::cout << "______________________________\n\n";
        std::cout << "Program returned: " << ret << std::endl;
        return static_cast<int>(ret);
    }
    else if (result)
    {
        std::cerr << "Compilation failed with errors:" << std::endl;
        for (const auto& error : result->get_errors())
        {
            std::cerr << " - " << error << std::endl;
        }
        return 1;
    }
    else    
    {
        std::cerr << "No result from compilation." << std::endl;
        return 1;
    }
}