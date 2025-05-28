#include "sharpie/parser/script_parser.hpp"
#include "sharpie/script_ast.hpp" // Master include for AST nodes
#include <iostream> // For debug prints if any were in parse()

namespace Mycelium::Scripting::Lang
{

    ScriptParser::ScriptParser(std::string_view source_code, std::string_view fileName)
        : sourceCode(source_code),
          fileName(fileName),
          currentCharOffset(0),
          currentLine(1),
          currentColumn(1),
          currentLineStartOffset(0)
    {
        // Initialize currentTokenInfo to something representing "before start" or an initial Error state
        currentTokenInfo.type = TokenType::Error; // Or a special "Start" token
        currentTokenInfo.location.fileName = std::string(fileName);
        previousTokenInfo = currentTokenInfo;
    }

    std::pair<std::shared_ptr<Mycelium::Scripting::Lang::CompilationUnitNode>, std::vector<Mycelium::Scripting::Lang::ParseError>> ScriptParser::parse()
    {
        errors.clear();
        currentCharOffset = 0;
        currentLine = 1;
        currentColumn = 1;
        currentLineStartOffset = 0;

        advance_and_lex(); // Prime the first token

        std::shared_ptr<CompilationUnitNode> compilation_unit_node = parse_compilation_unit();

        // Finalize the location of the compilation unit node if it was created
        if (compilation_unit_node && compilation_unit_node->location.has_value())
        {
            if (previousTokenInfo.type != TokenType::Error && currentTokenInfo.type == TokenType::EndOfFile)
            {
                compilation_unit_node->location->lineEnd = previousTokenInfo.location.lineEnd;
                compilation_unit_node->location->columnEnd = previousTokenInfo.location.columnEnd;
            }
            else if (currentTokenInfo.type == TokenType::EndOfFile)
            { 
                compilation_unit_node->location->lineEnd = currentTokenInfo.location.lineStart;
                compilation_unit_node->location->columnEnd = currentTokenInfo.location.columnStart;
            }
        }
        return {compilation_unit_node, errors};
    }

    // Other ScriptParser methods (parse_*, lex_*, helpers) will be in other files.

} // namespace Mycelium::Scripting::Lang
