#pragma once

#include "ast/ast.hpp"
#include "parse_result.hpp"
#include "parser_base.hpp"

namespace Myre
{

    /**
     * StatementParser - Handles all statement parsing with context management
     *
     * Responsibilities:
     * - Parse all statement types (variable declarations, blocks, control flow)
     * - Manage parsing context (loop/function contexts for break/continue validation)
     * - Handle statement-level error recovery
     * - Support nested statement structures
     */
    class StatementParser : public ParserBase
    {

    public:
        explicit StatementParser(Parser *parser);

        // Main statement parsing entry point
        ParseResult<StatementNode> parse_statement();

        // Specific statement type parsers
        ParseResult<StatementNode> parse_block_statement();
        ParseResult<StatementNode> parse_expression_statement();

        // Future statement types (to be implemented)
        ParseResult<StatementNode> parse_if_statement();
        ParseResult<StatementNode> parse_while_statement();
        ParseResult<StatementNode> parse_for_statement();
        ParseResult<StatementNode> parse_for_in_statement();
        ParseResult<StatementNode> parse_for_variable_declaration();
        ParseResult<StatementNode> parse_return_statement();
        ParseResult<StatementNode> parse_break_statement();
        ParseResult<StatementNode> parse_continue_statement();

    private:
        // Helper methods
        bool is_for_in_loop();
        ParseResult<AstNode> parse_for_variable();
    };

} // namespace Myre