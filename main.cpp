#include "compiler.hpp"
#include "semantic/symbol_table.hpp"
#include "semantic/type_system.hpp"
#include "semantic/type_resolver.hpp"
#include "semantic/declaration_collector.hpp"
#include "parser/lexer.hpp"
#include "parser/parser.hpp"
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
    auto source = read_file("test2.myre");
    compiler.compile(source);
    

    return 0;
}