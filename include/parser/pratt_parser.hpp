#pragma once

#include "parser/parser_base.hpp"
#include "parser/token_stream.hpp"
#include "parser/parser_context.hpp"
#include "ast/ast.hpp"
#include "ast/ast_allocator.hpp"
#include <memory>

namespace Mycelium::Scripting::Parser {

using namespace Mycelium::Scripting::Lang;

// Pratt parser for expressions with operator precedence parsing
class PrattParser : public ParserBase {
public:
    // Constructor
    PrattParser(TokenStream& tokens, ParserContext& context, AstAllocator& allocator);
    
    // Main expression parsing entry point
    ExpressionNode* parse_expression(int min_precedence = 0);
    
    // Parse primary expressions (literals, identifiers, parenthesized expressions)
    ExpressionNode* parse_primary();
    
    // Parse unary expressions (prefix operators)
    ExpressionNode* parse_unary();
    
    // Parse postfix expressions (suffix operators, member access, calls, indexing)
    ExpressionNode* parse_postfix(ExpressionNode* expr);
    
    // Parse binary expressions with precedence climbing
    ExpressionNode* parse_binary(ExpressionNode* left, int min_precedence);
    
    // Parse assignment expressions
    ExpressionNode* parse_assignment();
    
    // Parse conditional (ternary) expressions
    ExpressionNode* parse_conditional(ExpressionNode* condition);
    
    // Parse specific expression types
    ExpressionNode* parse_call_expression(ExpressionNode* target);
    ExpressionNode* parse_member_access(ExpressionNode* target);
    ExpressionNode* parse_indexer_expression(ExpressionNode* target);
    ExpressionNode* parse_cast_expression();
    ExpressionNode* parse_new_expression();
    ExpressionNode* parse_typeof_expression();
    ExpressionNode* parse_sizeof_expression();
    ExpressionNode* parse_when_expression();
    ExpressionNode* parse_range_expression(ExpressionNode* start = nullptr);
    
    // Convert token kinds to AST operator kinds
    UnaryOperatorKind token_to_unary_operator(TokenKind kind) const;
    BinaryOperatorKind token_to_binary_operator(TokenKind kind) const;
    AssignmentOperatorKind token_to_assignment_operator(TokenKind kind) const;
    
    // Debug helpers
    void enable_debug(bool enable = true) { debug_enabled_ = enable; }
    void dump_precedence_info(TokenKind kind) const;
    
private:
    // Expression parsing state
    bool allow_assignments_;
    bool allow_ternary_;
    int expression_depth_;
    bool debug_enabled_;
    
    // Parse grouped expressions (parentheses, braces)
    ExpressionNode* parse_parenthesized_expression();

    // Parse identifier-based expressions
    ExpressionNode* parse_identifier_expression();
    ExpressionNode* parse_this_expression();
    ExpressionNode* parse_enum_member_expression();
    
    // Error recovery for expressions
    ExpressionNode* recover_from_expression_error();
    void skip_to_expression_boundary();
    
    // Context-aware parsing
    void enter_expression_context();
    void exit_expression_context();
    
    // RAII helper for expression context
    class ExpressionContextGuard {
    public:
        explicit ExpressionContextGuard(PrattParser& parser);
        ~ExpressionContextGuard();
        
    private:
        PrattParser& parser_;
    };
    
    // Expression parsing options
    struct ExpressionParsingOptions {
        bool allow_assignments = true;
        bool allow_ternary = true;
        bool allow_ranges = true;
        bool allow_when = true;
        int max_depth = 100;
    };
    
    ExpressionParsingOptions current_options_;
    
};

} // namespace Mycelium::Scripting::Parser