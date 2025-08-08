#pragma once

#include "ast/ast.hpp"
#include "parse_result.hpp"
#include "parser_base.hpp"

namespace Myre
{

    /**
     * ExpressionParser - Handles all expression parsing using Pratt parsing (precedence climbing)
     *
     * Responsibilities:
     * - Parse all expression types (literals, identifiers, binary expressions, etc.)
     * - Handle operator precedence correctly
     * - Provide error recovery for malformed expressions
     * - Support parenthesized expressions and primary expressions
     */
    class ExpressionParser : public ParserBase
    {

    public:
        explicit ExpressionParser(Parser *parser);

        // Main expression parsing entry point
        ParseResult<ExpressionNode> parse_expression(int min_precedence = 0);

    private:
        // Pratt parser implementation for binary expressions
        ParseResult<ExpressionNode> parse_binary_expression(int min_precedence);

        // Primary expression parsing - handles literals, identifiers, parentheses
        ParseResult<ExpressionNode> parse_primary();

        // Literal parsing methods
        ParseResult<ExpressionNode> parse_range_expression(ExpressionNode* left);
        ParseResult<ExpressionNode> parse_prefix_range_expression();
        ParseResult<ExpressionNode> parse_integer_literal();
        ParseResult<ExpressionNode> parse_float_literal();
        ParseResult<ExpressionNode> parse_double_literal();
        ParseResult<ExpressionNode> parse_string_literal();
        ParseResult<ExpressionNode> parse_boolean_literal();
        ParseResult<ExpressionNode> parse_identifier_or_call();
        ParseResult<ExpressionNode> parse_parenthesized_expression();

        // Unary expression parsing
        ParseResult<ExpressionNode> parse_unary_expression();

        // Postfix expression parsing helpers
        ParseResult<ExpressionNode> parse_call_suffix(ExpressionNode *target);
        ParseResult<ExpressionNode> parse_member_access_suffix(ExpressionNode *target);
        ParseResult<ExpressionNode> parse_indexer_suffix(ExpressionNode *target);

        // Operator precedence
        int get_precedence(TokenKind op);

        // Future expression types (to be implemented)
        ParseResult<ExpressionNode> parse_call_expression();
        ParseResult<ExpressionNode> parse_member_access();
        ParseResult<ExpressionNode> parse_new_expression();
        ParseResult<ExpressionNode> parse_match_expression();
        ParseResult<ExpressionNode> parse_enum_variant();
    };

} // namespace Myre