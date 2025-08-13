#include "compiler.hpp"
// #include "semantic/symbol_table.hpp"
// #include "semantic/type_system.hpp"
// #include "semantic/type_resolver.hpp"
// #include "semantic/declaration_collector.hpp"
#include "parser/lexer.hpp"
#include "parser/parser.hpp"
#include "common/logger.hpp"
#include "ast/ast.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
using namespace Myre; 


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

    Compiler compiler;
    compiler.set_print_ast(true);
    compiler.set_print_symbols(true);

    std::vector<std::string> filenames = {"simple.myre", "simple2.myre", "basic_enum_prop.myre", "test0.myre", "test1.myre", "test2.myre", "test3.myre"};
    std::vector<SourceFile> source_files;
    for (const auto& filename : filenames)
    {
        auto source = read_file(filename);
        source_files.push_back({filename, source});
    }

    auto result = compiler.compile(source_files);

    if (result->is_valid())
    {
        result->dump_ir();
        auto ret = result->execute_jit();
        std::cout << "JIT execution returned: " << ret << std::endl;
    }
    else
    {
        LOG_HEADER("Compilation failed");
        for (const auto& error : result->get_errors())
        {
            std::cerr << "Error: " << error << std::endl;
        }
    }

    return 0;
}