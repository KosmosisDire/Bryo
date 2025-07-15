#include "ast/ast.hpp"
#include "ast/ast_allocator.hpp"
#include "ast/ast_rtti.hpp"
#include "ast/ast_printer.hpp"
#include "semantic/symbol_table.hpp"
#include "codegen/codegen.hpp"
#include "codegen/command_processor.hpp"
#include "codegen/jit_engine.hpp"
#include "common/logger.hpp"
#include "parser/token_stream.hpp"
#include "parser/lexer.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring> // For memcpy

// Bring the AST namespace into scope for convenience.
using namespace Mycelium::Scripting::Lang;
using namespace Mycelium::Scripting::Common;

// --- Helper Functions for Manual AST Construction ---

template<typename T>
SizedArray<T> create_sized_array(AstAllocator& allocator, const std::vector<T>& items) {
    SizedArray<T> arr;
    arr.size = (int)items.size();
    if (arr.size > 0) {
        arr.values = (T*)allocator.alloc_bytes(sizeof(T) * arr.size, alignof(T));
        memcpy(arr.values, items.data(), sizeof(T) * arr.size);
    } else {
        arr.values = nullptr;
    }
    return arr;
}

TokenNode* create_token(AstAllocator& allocator, TokenKind kind, const std::string& text) {
    auto token = allocator.alloc<TokenNode>();
    token->tokenKind = kind;
    char* text_buffer = (char*)allocator.alloc_bytes(text.length() + 1, 1);
    memcpy(text_buffer, text.c_str(), text.length() + 1);
    token->text = std::string_view(text_buffer, text.length());
    return token;
}

IdentifierNode* create_identifier(AstAllocator& allocator, const std::string& name) {
    auto ident = allocator.alloc<IdentifierNode>();
    char* name_buffer = (char*)allocator.alloc_bytes(name.length() + 1, 1);
    memcpy(name_buffer, name.c_str(), name.length() + 1);
    ident->name = std::string_view(name_buffer, name.length());
    return ident;
}

TypeNameNode* create_type_name(AstAllocator& allocator, const std::string& name) {
    auto type_node = allocator.alloc<TypeNameNode>();
    type_node->identifier = create_identifier(allocator, name);
    return type_node;
}

// --- Main Test Function ---

int main()
{
    // Initialize logger
    Logger& logger = Logger::get_instance();
    logger.initialize();
    logger.set_console_level(LogLevel::INFO);
    logger.set_enabled_categories(LogCategory::ALL);
    
    // 1. Initialize RTTI
    AstTypeInfo::initialize();
    LOG_INFO("RTTI Initialized. Total types: " + std::to_string(AstNode::sTypeInfo.fullDerivedCount + 1), LogCategory::GENERAL);
    LOG_SEPARATOR();

    std::unique_ptr<Lexer> lexer;
    std::unique_ptr<TokenStream> token_stream;
    std::unique_ptr<ParserContext> context;
    std::unique_ptr<AstAllocator> allocator;
    std::unique_ptr<RecursiveParser> parser;
    std::shared_ptr<PrattParser> expr_parser;

    // 4. Create and run the printer visitor.
    LOG_HEADER("Printing constructed AST", LogCategory::AST);
    AstPrinterVisitor printer;
    compilation_unit->accept(&printer);
    LOG_SEPARATOR('-', 40, LogCategory::AST);

    // 5. Build symbol table from AST
    LOG_HEADER("Building symbol table from AST", LogCategory::SEMANTIC);
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, compilation_unit);
    symbol_table.print_symbol_table();
    LOG_SEPARATOR('-', 40, LogCategory::SEMANTIC);

    // 6. Demonstrate symbol table navigation
    LOG_HEADER("Demonstrating symbol table navigation", LogCategory::SEMANTIC);
    
    // Navigate to Console class
    symbol_table.push_scope("Console");
    symbol_table.print_navigation_state();
    
    // Look up a field in Console
    auto messageCount = symbol_table.lookup_symbol("messageCount");
    if (messageCount) {
        LOG_INFO("Found: " + messageCount->name + " (" + messageCount->data_type + ")", LogCategory::SEMANTIC);
    }
    
    // Navigate to Enemy class
    symbol_table.reset_navigation();
    symbol_table.push_scope("Enemy");
    symbol_table.print_navigation_state();
    
    // Look up GetDamage function
    auto getDamage = symbol_table.lookup_symbol("GetDamage");
    if (getDamage) {
        LOG_INFO("Found: " + getDamage->name + " (" + getDamage->data_type + ")", LogCategory::SEMANTIC);
    }
    
    symbol_table.reset_navigation();
    LOG_SEPARATOR('-', 40, LogCategory::SEMANTIC);

    // 7. Test code generation
    LOG_HEADER("Testing code generation", LogCategory::CODEGEN);
    CodeGenerator code_gen(symbol_table);
    auto commands = code_gen.generate_code(compilation_unit);
    
    // Process commands to generate LLVM IR
    LOG_INFO("Processing commands to LLVM IR...", LogCategory::CODEGEN);
    std::string ir_string = CommandProcessor::process_to_ir_string(commands, "GeneratedModule");
    LOG_SUBHEADER("Generated LLVM IR", LogCategory::CODEGEN);
    LOG_INFO(ir_string, LogCategory::CODEGEN);
    
    // Test JIT execution with the generated IR
    LOG_HEADER("Testing JIT execution", LogCategory::JIT);
    JITEngine jit;
    if (jit.initialize_from_ir(ir_string, "GeneratedModule")) {
        jit.dump_functions();
        int result = jit.execute_function("Main");
        LOG_INFO("JIT execution completed with result: " + std::to_string(result), LogCategory::JIT);
    } else {
        LOG_ERROR("Failed to initialize JIT engine with generated IR", LogCategory::JIT);
    }

    // 8. Test JIT engine with simple hardcoded IR example
    LOG_HEADER("Testing JIT with simple IR example", LogCategory::JIT);
    
    // Simple LLVM IR that adds two numbers and returns the result
    std::string simple_ir = R"(
define i32 @add_numbers() {
entry:
  %a = add i32 10, 20
  %b = add i32 %a, 5
  ret i32 %b
}

define void @simple_test() {
entry:
  ret void
}
)";
    
    JITEngine simple_jit;
    if (simple_jit.initialize_from_ir(simple_ir, "SimpleExample")) {
        simple_jit.dump_functions();
        
        LOG_INFO("Executing add_numbers function...", LogCategory::JIT);
        int add_result = simple_jit.execute_function("add_numbers");
        LOG_INFO("add_numbers() returned: " + std::to_string(add_result), LogCategory::JIT);
        
        LOG_INFO("Executing simple_test function...", LogCategory::JIT);
        int test_result = simple_jit.execute_function("simple_test");
        LOG_INFO("simple_test() returned: " + std::to_string(test_result), LogCategory::JIT);
    } else {
        LOG_ERROR("Failed to initialize JIT engine with simple IR", LogCategory::JIT);
    }

    // 9. Test RTTI on the new AST
    LOG_HEADER("Testing RTTI on new AST", LogCategory::AST);
    if (main_func->is_a<DeclarationNode>()) {
         LOG_INFO("OK: FunctionDeclarationNode is a DeclarationNode.", LogCategory::AST);
    }
    if (!while_stmt->is_a<ExpressionNode>()) {
        LOG_INFO("OK: WhileStatementNode is NOT an ExpressionNode.", LogCategory::AST);
    }
    auto casted_decl = console_messageCount_field->as<DeclarationNode>();
    if (casted_decl && casted_decl->name) {
        LOG_INFO("OK: Cast from FieldDeclarationNode to DeclarationNode successful. Name: " + std::string(casted_decl->name->name), LogCategory::AST);
    } else {
        LOG_ERROR("FAIL: Cast to DeclarationNode failed or name was null.", LogCategory::AST);
    }

    return 0;
}