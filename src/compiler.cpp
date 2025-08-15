#include "compiler.hpp"

#include "common/logger.hpp"
#include "codegen/codegen.hpp"
#include "semantic/symbol_table.hpp"
#include "parser/lexer.hpp"
#include "parser/parser.hpp"
#include "ast/ast.hpp"
#include "ast/ast_printer.hpp"
#include "ast/ast_code_printer.hpp"
#include "semantic/type_resolver.hpp"
#include "semantic/symbol_table_builder.hpp"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Host.h> 
#include <optional>

namespace Myre {

std::unique_ptr<CompiledModule> Compiler::compile(const std::vector<SourceFile>& source_files)
{
    // AstTypeInfo::initialize();

    if (source_files.empty()) {
        return std::make_unique<CompiledModule>();
    }
    
    std::vector<std::string> all_errors;
    std::vector<FileCompilationState> file_states(source_files.size());
    
    // === PHASE 1: Parse all files sequentially ===
    LOG_HEADER("Phase 1: Sequential parsing", LogCategory::COMPILER);
    
    for (size_t i = 0; i < source_files.size(); ++i) {
        auto& state = file_states[i];
        state.file = source_files[i];
        
        LOG_INFO("Parsing: " + state.file.filename, LogCategory::COMPILER);
        
        // Lex and parse
        auto lexer = Lexer(state.file.source);
        auto tokens = lexer.tokenize_all();
        
        if (lexer.has_errors()) {
            for (const auto& error : lexer.get_diagnostics()) {
                state.errors.push_back(state.file.filename + " - Lexer: " + error.message);
            }
            continue;
        }

        state.tokens = std::make_unique<TokenStream>(std::move(tokens));
        state.parser = std::make_unique<Parser>(*state.tokens);
        state.ast = state.parser->parse();

        if (!state.ast) {
            state.errors.push_back(state.file.filename + ": Invalid AST");
            continue;
        }
        
        for (const auto& error : state.parser->getErrors()) {
            state.errors.push_back(state.file.filename + " - Parser: " +
                error.location.start.to_string() + ": " + error.message);
        }

        if (print_ast) {
            LOG_INFO("\nAST for " + state.file.filename + ":\n", LogCategory::COMPILER);
            AstToCodePrinter printer;
            printer.visit(state.ast);
            std::cout << printer.get_result() << "\n";
        }
        
        state.parse_complete = true;
    }
    
    // Collect parsing errors
    for (const auto& state : file_states) {
        all_errors.insert(all_errors.end(), state.errors.begin(), state.errors.end());
    }
    
    if (!all_errors.empty()) {
        LOG_HEADER("Parsing errors encountered", LogCategory::COMPILER);
        for (const auto& error : all_errors) {
            LOG_ERROR(error, LogCategory::COMPILER);
        }
        return std::make_unique<CompiledModule>();
    }
    
    // // === PHASE 2: Build local symbol tables sequentially ===
    LOG_HEADER("Phase 2: Sequential symbol collection", LogCategory::COMPILER);
    
    for (size_t i = 0; i < file_states.size(); ++i) {
        auto& state = file_states[i];
        
        if (!state.parse_complete) continue;
        
        LOG_INFO("Building symbols for: " + state.file.filename, LogCategory::COMPILER);
        
        // Create local symbol table
        state.symbolTable = std::make_unique<SymbolTable>();
        
        // Collect declarations
        SymbolTableBuilder collector(*state.symbolTable);
        collector.collect(state.ast);
        
        for (const auto& error : collector.get_errors()) {
            state.errors.push_back(state.file.filename + " - Declaration: " + error);
        }

        if (print_symbols) {
            LOG_INFO("\nLocal Symbol Table for " + state.file.filename + ":\n", LogCategory::COMPILER);
            LOG_INFO(state.symbolTable->to_string() + "\n", LogCategory::COMPILER);
        }

        state.symbols_complete = true;
    }
    
    // === PHASE 3: Merge symbol tables ===
    LOG_HEADER("Phase 3: Merging symbol tables", LogCategory::COMPILER);
    
    // Create the global symbol table
    auto global_symbols = std::make_unique<SymbolTable>();
    
    // Merge all local symbol tables into global
    for (auto& state : file_states) {
        if (!state.symbols_complete) continue;

        LOG_INFO("Merging symbols from: " + state.file.filename, LogCategory::COMPILER);

        // Merge the local table into global
        auto conflicts = global_symbols->merge(*state.symbolTable);
        
        // Report conflicts as errors
        for (const auto& conflict : conflicts) {
            all_errors.push_back(state.file.filename + " - " + conflict);
        }
    }
    
    // Check for merge conflicts
    if (!all_errors.empty()) {
        LOG_HEADER("Symbol merge conflicts", LogCategory::COMPILER);
        for (const auto& error : all_errors) {
            LOG_ERROR(error, LogCategory::COMPILER);
        }
        return std::make_unique<CompiledModule>();
    }
    
    // === PHASE 4: Type resolution with global symbol table ===
    LOG_HEADER("Phase 4: Type resolution", LogCategory::COMPILER);
    
    TypeResolver resolver(*global_symbols);
    resolver.resolve_types();
    
    for (const auto& error : resolver.get_errors()) {
        all_errors.push_back("Type: " + error);
    }
    
    if (print_symbols) {
        LOG_INFO("\nGlobal Symbol Table after Type Resolution:\n", LogCategory::COMPILER);
        LOG_INFO(global_symbols->to_string(), LogCategory::COMPILER);
    }
    
    if (!all_errors.empty()) {
        LOG_HEADER("Type resolution errors", LogCategory::COMPILER);
        for (const auto& error : all_errors) {
            LOG_ERROR(error, LogCategory::COMPILER);
        }

        return std::make_unique<CompiledModule>();
    }
    
    // === PHASE 5: Code generation with global symbols ===
    LOG_HEADER("Phase 5: Code generation", LogCategory::COMPILER);
    
    auto llvm_context = std::make_unique<llvm::LLVMContext>();
    CodeGenerator codegen(*global_symbols, "MyreProgram", llvm_context.get());
    
    // Quick pass: declare all functions from symbol table
    codegen.declare_all_functions();
    
    // Single AST pass: generate all bodies
    for (const auto& state : file_states) {
        if (!state.ast) continue;
        codegen.generate_definitions(state.ast);
    }
    
    auto llvm_module = codegen.release_module();
    
    for (const auto& error : codegen.get_errors()) {
        all_errors.push_back(error.to_string());
    }
    
    return std::make_unique<CompiledModule>(
        std::move(llvm_context),
        std::move(llvm_module),
        "MyreProgram",
        all_errors
    );
}

} // namespace Myre