// compiler.hpp
#pragma once
#include "ast/ast.hpp"
#include "semantic/symbol_table.hpp"
#include "compiled_module.hpp"
#include "parser/token_stream.hpp"
#include <string>
#include <memory>

namespace Myre {

class Parser;

struct SourceFile {
    std::string filename;
    std::string source;
};

struct FileCompilationState {
    SourceFile file;
    std::unique_ptr<TokenStream> tokens; // store the token stream here
    std::unique_ptr<Parser> parser; // store the parser since it owns the AST
    std::unique_ptr<SymbolTable> symbolTable; // symbols local to this file
    CompilationUnit* ast; // pointer to the AST root

    std::vector<std::string> errors;

    bool parse_complete = false;
    bool symbols_complete = false;
};

class Compiler {
private:
    // Configuration options
    bool verbose = false;
    bool print_ast = false;
    bool print_symbols = false;
    
public:
    // Main compilation function
    std::unique_ptr<CompiledModule> compile(const std::vector<SourceFile>& source_files);
    std::unique_ptr<CompiledModule> compile(const SourceFile& source) {
        return compile(std::vector<SourceFile>{source});
    }
    
    // Configuration
    void set_verbose(bool v) { verbose = v; }
    void set_print_ast(bool p) { print_ast = p; }
    void set_print_symbols(bool p) { print_symbols = p; }
};

} // namespace Myre