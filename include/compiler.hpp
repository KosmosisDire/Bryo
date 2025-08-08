#pragma once

#include "semantic/symbol_table.hpp"
#include "parser/lexer.hpp"
#include "parser/parser.hpp"
#include "ast/ast.hpp"
#include "ast/ast_printer.hpp"
#include "ast/ast_code_printer.hpp"
#include "semantic/type_resolver.hpp"
#include "semantic/declaration_collector.hpp"
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