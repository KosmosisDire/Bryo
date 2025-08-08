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
    }

    if (ast.is_error())
    {
        LOG_ERROR("Failed to parse source file", LogCategory::COMPILER);
        return;
    }
    
    AstPrinter printer;
    printer.visit(ast.get_node());

    AstToCodePrinter code_printer;
    code_printer.visit(ast.get_node());
    code_printer.get_result();

    SymbolTable symbol_table;
    DeclarationCollector collector(symbol_table);

    auto* unit = ast.get_node();
    if (!unit)
    {
        std::cout << "Invalid AST: expected CompilationUnitNode\n";
        return;
    }

    collector.collect(unit);
    
    for (const auto& error : collector.get_errors()) {
        std::cout << "  - " << error << "\n";
    }

    // Run type resolution
    TypeResolver resolver(symbol_table);
    resolver.resolve_types();
    std::cout << resolver.to_string() << "\n";

    // print the symbol table again after type resolution
    std::cout << "\nSymbol Table after Type Resolution:\n";
    std::cout << symbol_table.to_string() << "\n";

    // CodeGenerator codegen(symbol_table);
    // auto commands = codegen.generate_code(compilation_unit);
    
    // if (commands.empty()) {
    //     std::cerr << "Error: No commands generated from AST" << std::endl;
    //     return 1;
    // }
}
