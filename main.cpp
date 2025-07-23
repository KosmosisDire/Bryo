#include "parser/parser.h"
#include "parser/lexer.hpp"
#include "parser/token_stream.hpp"
#include "codegen/codegen.hpp"
#include "codegen/command_processor.hpp"
#include "codegen/jit_engine.hpp"
#include "semantic/symbol_table.hpp"
#include "common/logger.hpp"
#include "ast/ast_rtti.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

#include "ast/ast_printer.hpp"

using namespace Mycelium::Scripting::Lang;
using namespace Mycelium;
using namespace Mycelium::Scripting::Common;

// Diagnostic sink for lexer errors
class ConsoleLexerDiagnosticSink : public LexerDiagnosticSink {
public:
    void report_diagnostic(const LexerDiagnostic& diagnostic) override {
        std::cerr << "Lexer Error: " << diagnostic.message << " at line " << diagnostic.location.line << ", column " << diagnostic.location.column << std::endl;
    }
};

// Helper function to read file contents
std::string read_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filepath);
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return content;
}

// Main scripting engine function
int run_script(const std::string& filepath) {
    try {

        // Read the script file
        std::string source_code = read_file(filepath);
        std::cout << "Executing script: " << filepath << std::endl;
        
        // Step 1: Lexical analysis
        ConsoleLexerDiagnosticSink diagnostic_sink;
        Lexer lexer(source_code, {}, &diagnostic_sink);
        TokenStream token_stream = lexer.tokenize_all();
        
        if (token_stream.size() == 0) {
            std::cerr << "Error: No tokens generated from source file" << std::endl;
            return 1;
        }

        // LOG_INFO(token_stream.to_string(), LogCategory::PARSER);

        // Step 2: Parse the source code
        Parser parser(token_stream);
        auto parse_result = parser.parse();
        
        if (!parse_result.is_success()) {
            std::cerr << "Parse Error: Failed to parse " << filepath << std::endl;
            return 1;
        }

        parser.get_diagnostics().print();
        
        auto* compilation_unit = parse_result.get_node();
        if (!compilation_unit) {
            std::cerr << "Error: No AST generated" << std::endl;
            return 1;
        }

        AstPrinterVisitor printer;
        compilation_unit->accept(&printer);
        
        // Step 3: Build symbol table
        SymbolTable symbol_table;
        build_symbol_table(symbol_table, compilation_unit);
        
        // Debug: Print symbol table
        LOG_HEADER("Symbol Table", LogCategory::SEMANTIC);
        symbol_table.print_symbol_table();
        
        // Step 4: Generate code
        CodeGenerator codegen(symbol_table);
        auto commands = codegen.generate_code(compilation_unit);
        
        if (commands.empty()) {
            std::cerr << "Error: No commands generated from AST" << std::endl;
            return 1;
        }
        
        // Debug: Show generated commands with readable format
        // std::cout << "=== Generated Commands ===" << std::endl;
        // for (size_t i = 0; i < commands.size(); ++i) {
        //     std::cout << "[" << i << "] " << commands[i].to_string() << std::endl;
        // }
        // std::cout << "=== End Commands ===" << std::endl;
        
        // Step 5: Generate LLVM IR
        std::string ir = CommandProcessor::process_to_ir_string(commands, "ScriptModule");
        
        if (ir.empty()) {
            std::cerr << "Error: No IR generated from commands" << std::endl;
            return 1;
        }
        
        // Debug: Show generated IR
        std::cout << "=== Generated IR ===" << std::endl;
        std::cout << ir << std::endl;
        std::cout << "=== End IR ===" << std::endl;
        
        // Step 6: Execute with JIT
        JITEngine jit;
        if (!jit.initialize_from_ir(ir, "ScriptModule")) {
            std::cerr << "Error: Failed to initialize JIT engine" << std::endl;
            return 1;
        }
        
        // Step 7: Execute the main function
        try {
            int result = jit.execute_function("main");
            std::cout << "Script executed successfully. Return value: " << result << std::endl;
            return result;
        } catch (const std::exception& e) {
            std::cerr << "Execution Error: " << e.what() << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

void print_usage(const std::string& program_name) {
    std::cout << "Myre Scripting Engine" << std::endl;
    std::cout << "Usage: " << program_name << " <script.myre>" << std::endl;
    std::cout << "Example: " << program_name << " test.myre" << std::endl;
}

int main(int argc, char* argv[]) {
    // Initialize logger (minimal output for cleaner IR output)
    Logger& logger = Logger::get_instance();
    logger.initialize();
    logger.set_console_level(LogLevel::TRACE); // Only show errors for cleaner output
    
    // Initialize RTTI system
    AstTypeInfo::initialize();
    
    // Check command line arguments
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string script_path = argv[1];
    
    // Check if file exists
    if (!std::filesystem::exists(script_path)) {
        std::cerr << "Error: File does not exist: " << script_path << std::endl;
        return 1;
    }
    
    // Check if file has .myre extension
    if (!script_path.ends_with(".myre")) {
        std::cerr << "Warning: File does not have .myre extension" << std::endl;
    }
    
    // Run the script
    return run_script(script_path);
}