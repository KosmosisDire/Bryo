#pragma once

#include "ast/ast.hpp"
#include "parse_result.h"
#include "token_stream.hpp"

namespace Mycelium::Scripting::Lang {

// Forward declarations
class Parser;
class ParseContext;

/**
 * StatementParser - Handles all statement parsing with context management
 * 
 * Responsibilities:
 * - Parse all statement types (variable declarations, blocks, control flow)
 * - Manage parsing context (loop/function contexts for break/continue validation)
 * - Handle statement-level error recovery
 * - Support nested statement structures
 */
class StatementParser {
private:
    Parser* parser_;  // Reference to main parser for shared services
    
    // Helper accessors for parser state
    ParseContext& context();
    ErrorNode* create_error(ErrorKind kind, const char* msg);
    
public:
    explicit StatementParser(Parser* parser);
    
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
    ParseResult<StatementNode> parse_return_statement();
    ParseResult<StatementNode> parse_break_statement();
    ParseResult<StatementNode> parse_continue_statement();
    
private:
    // Helper methods
    bool is_for_in_loop();
    ParseResult<AstNode> parse_for_variable();
};

} // namespace Mycelium::Scripting::Lang