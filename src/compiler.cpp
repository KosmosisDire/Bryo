#include "compiler.hpp"


void Compiler::compile(const std::string& source_file)
{
    AstTypeInfo::initialize();

    auto lexer = Lexer(source_file);
    auto tokens = lexer.tokenize_all();

    if (lexer.has_errors())
    {
        LOG_HEADER("Lexer errors encountered:", LogCategory::COMPILER);
        for (const auto& error : lexer.get_diagnostics())
        {
            LOG_ERROR(error.message, LogCategory::COMPILER);
        }
        return;
    }
    LOG_INFO(tokens.to_string());

    auto parser = Parser(tokens);
    auto ast = parser.parse();

    if (parser.get_diagnostics().size() > 0)
    {
        LOG_HEADER("Parser errors encountered:", LogCategory::COMPILER);
        parser.get_diagnostics().print();
        return;
    }

    AstPrinter printer;
    if (ast.is_error())
    {
        LOG_ERROR("Failed to parse source file", LogCategory::COMPILER);
        return;
    }

    printer.visit(ast.get_node());

    // CodeGenerator codegen(symbol_table);
    // auto commands = codegen.generate_code(compilation_unit);
    
    // if (commands.empty()) {
    //     std::cerr << "Error: No commands generated from AST" << std::endl;
    //     return 1;
    // }
}
