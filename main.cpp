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

using namespace Bryo; 


std::string read_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main(int argc, char* argv[])
{
    Logger& logger = Logger::get_instance();
    logger.initialize();
    logger.set_console_level(LogLevel::TRACE);

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

    Compiler compiler;
    compiler.set_print_ast(true);
    compiler.set_print_symbols(true);
    compiler.set_print_hlir(true);

    // Use command line arguments if provided, otherwise default to simple.bryo
    std::vector<std::string> filenames;
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            filenames.push_back(argv[i]);
        }
    } else {
        filenames = {"minimal.bryo", "runtime/basic_print.bryo"};
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
        result->dump_ir();
        result->write_object_file("out/output.o");
        auto ret = result->execute_jit<float>("Main_f32_").value_or(-1.0f);
        std::cout << "JIT execution returned: " << ret << std::endl;
        return static_cast<int>(ret);
    }
    else if (result)
    {
        LOG_HEADER("Compilation failed with errors:");
        for (const auto& error : result->get_errors())
        {
            LOG_ERROR(error, LogCategory::COMPILER);
        }
        return 1;
    }
    else    
    {
        LOG_HEADER("No result from compilation.");
        return 1;
    }
}