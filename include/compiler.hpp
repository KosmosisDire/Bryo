#pragma once

#include "semantic/symbol_table.hpp"
#include "parser/lexer.hpp"
#include "parser/parser.hpp"
#include "ast/ast_printer.hpp"
// #include "codegen/codegen.hpp"
#include <string>

namespace Myre
{


class Compiler
{

public:

    void compile(const std::string& source_file);
};

}