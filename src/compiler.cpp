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
    
    AstPrinter printer;
    printer.visit(ast.get());

    AstToCodePrinter code_printer;
    code_printer.visit(ast.get());
    code_printer.get_result();

    SymbolTable symbol_table;
    DeclarationCollector collector(symbol_table);

    auto* unit = ast.get();
    if (!unit)
    {
        std::cout << "Invalid AST: expected CompilationUnitNode\n";
        return;
    }

    auto parsing_errs = parser.get_errors();
    if (!parsing_errs.empty())
    {
        LOG_HEADER("Parser errors encountered:", LogCategory::COMPILER);
        for (const auto& error : parsing_errs)
        {
            LOG_ERROR(error.location.start.to_string() + ": " + error.message, LogCategory::COMPILER);
        }
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

    if (!parsing_errs.empty())
    {
        LOG_HEADER("Parser errors encountered:", LogCategory::COMPILER);
        for (const auto& error : parsing_errs)
        {
            LOG_ERROR(error.location.start.to_string() + ": " + error.message, LogCategory::COMPILER);
        }
    }
}
