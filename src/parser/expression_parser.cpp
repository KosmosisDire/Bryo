#include "parser/expression_parser.h"
#include "parser/parser.h"

namespace Mycelium::Scripting::Lang {

ExpressionParser::ExpressionParser(Parser* parser) : parser_(parser) {
}

// Helper accessors for parser state
ParseContext& ExpressionParser::context() {
    return parser_->get_context();
}

ErrorNode* ExpressionParser::create_error(ErrorKind kind, const char* msg) {
    return parser_->create_error(kind, msg);
}

// Main expression parsing entry point
ParseResult<ExpressionNode> ExpressionParser::parse_expression(int min_precedence) {
    return parse_binary_expression(min_precedence);
}

// Pratt parser implementation for binary expressions
ParseResult<ExpressionNode> ExpressionParser::parse_binary_expression(int min_precedence) {
    auto left_result = parse_primary();
    if (!left_result.is_success()) {
        return left_result;
    }
    
    auto* left = left_result.get_node();
    
    while (!context().at_end()) {
        const Token& op_token = context().current();
        TokenKind op_kind = op_token.kind;
        int precedence = get_precedence(op_kind);
        
        if (precedence == 0 || precedence < min_precedence) break;
        
        context().advance(); // consume operator
        
        auto right_result = parse_binary_expression(precedence + 1);
        if (!right_result.is_success()) {
            // Error recovery: create binary expr with error on right side
            auto* error = create_error(ErrorKind::MissingToken, "Expected right operand");
            auto* binary = parser_->get_allocator().alloc<BinaryExpressionNode>();
            binary->left = left;
            binary->right = error; // Use AstNode* directly for error integration
            binary->opKind = op_token.to_binary_operator_kind();
            binary->contains_errors = true;
            return ParseResult<ExpressionNode>::success(binary);
        }
        
        auto* binary = parser_->get_allocator().alloc<BinaryExpressionNode>();
        binary->left = left;
        binary->right = right_result.get_node();
        binary->opKind = op_token.to_binary_operator_kind();
        binary->contains_errors = ast_has_errors(left) || ast_has_errors(right_result.get_node());
        
        left = binary;
    }
    
    return ParseResult<ExpressionNode>::success(left);
}

// Primary expression parsing - handles literals, identifiers, parentheses
ParseResult<ExpressionNode> ExpressionParser::parse_primary() {
    auto& ctx = context();
    
    if (ctx.check(TokenKind::IntegerLiteral)) {
        return parse_integer_literal();
    }
    
    if (ctx.check(TokenKind::StringLiteral)) {
        return parse_string_literal();
    }
    
    if (ctx.check(TokenKind::BooleanLiteral)) {
        return parse_boolean_literal();
    }
    
    if (ctx.check(TokenKind::Identifier)) {
        return parse_identifier();
    }
    
    if (ctx.check(TokenKind::LeftParen)) {
        return parse_parenthesized_expression();
    }
    
    return ParseResult<ExpressionNode>::error(
        create_error(ErrorKind::UnexpectedToken, "Expected expression"));
}

// Literal parsing methods
ParseResult<ExpressionNode> ExpressionParser::parse_integer_literal() {
    const Token& token = context().current();
    context().advance();
    
    auto* literal = parser_->get_allocator().alloc<LiteralExpressionNode>();
    literal->kind = LiteralKind::Integer;
    literal->contains_errors = false;
    
    auto* token_node = parser_->get_allocator().alloc<TokenNode>();
    token_node->text = token.text;
    token_node->contains_errors = false;
    literal->token = token_node;
    
    return ParseResult<ExpressionNode>::success(literal);
}

ParseResult<ExpressionNode> ExpressionParser::parse_string_literal() {
    const Token& token = context().current();
    context().advance();
    
    auto* literal = parser_->get_allocator().alloc<LiteralExpressionNode>();
    literal->kind = LiteralKind::String;
    literal->contains_errors = false;
    
    auto* token_node = parser_->get_allocator().alloc<TokenNode>();
    token_node->text = token.text;
    token_node->contains_errors = false;
    literal->token = token_node;
    
    return ParseResult<ExpressionNode>::success(literal);
}

ParseResult<ExpressionNode> ExpressionParser::parse_boolean_literal() {
    const Token& token = context().current();
    context().advance();
    
    auto* literal = parser_->get_allocator().alloc<LiteralExpressionNode>();
    literal->kind = LiteralKind::Boolean;
    literal->contains_errors = false;
    
    auto* token_node = parser_->get_allocator().alloc<TokenNode>();
    token_node->text = token.text;
    token_node->contains_errors = false;
    literal->token = token_node;
    
    return ParseResult<ExpressionNode>::success(literal);
}

ParseResult<ExpressionNode> ExpressionParser::parse_identifier() {
    const Token& token = context().current();
    context().advance();
    
    auto* identifier_expr = parser_->get_allocator().alloc<IdentifierExpressionNode>();
    identifier_expr->contains_errors = false;
    auto* identifier = parser_->get_allocator().alloc<IdentifierNode>();
    identifier->name = token.text;
    identifier->contains_errors = false;
    identifier_expr->identifier = identifier;
    
    return ParseResult<ExpressionNode>::success(identifier_expr);
}

ParseResult<ExpressionNode> ExpressionParser::parse_parenthesized_expression() {
    context().advance(); // consume '('
    auto expr_result = parse_expression();
    parser_->expect(TokenKind::RightParen, "Expected ')' after expression");
    return expr_result;
}

// Operator precedence table for Myre language (following design document)
int ExpressionParser::get_precedence(TokenKind op) {
    switch (op) {
        case TokenKind::Or:                     return 1;   // ||
        case TokenKind::And:                    return 2;   // &&
        case TokenKind::Equal:            
        case TokenKind::NotEqual:               return 3;   // ==, !=
        case TokenKind::Less:
        case TokenKind::LessEqual:
        case TokenKind::Greater:
        case TokenKind::GreaterEqual:           return 4;   // <, <=, >, >=
        case TokenKind::DotDot:
        case TokenKind::DotDotEquals:           return 5;   // .. (range)
        case TokenKind::Plus:
        case TokenKind::Minus:                  return 6;   // +, -
        case TokenKind::Asterisk:
        case TokenKind::Slash:
        case TokenKind::Percent:                return 7;   // *, /, %
        case TokenKind::Dot:                    return 9;   // . (member access)
        case TokenKind::LeftBracket:            return 10;  // [] (indexing)
        case TokenKind::LeftParen:              return 10;  // () (function call)
        default:                                return 0;   // Not a binary operator
    }
}


// Future expression types (placeholders for now)
ParseResult<ExpressionNode> ExpressionParser::parse_call_expression() {
    return ParseResult<ExpressionNode>::error(
        create_error(ErrorKind::UnexpectedToken, "Call expressions not implemented yet"));
}

ParseResult<ExpressionNode> ExpressionParser::parse_member_access() {
    return ParseResult<ExpressionNode>::error(
        create_error(ErrorKind::UnexpectedToken, "Member access not implemented yet"));
}

ParseResult<ExpressionNode> ExpressionParser::parse_new_expression() {
    return ParseResult<ExpressionNode>::error(
        create_error(ErrorKind::UnexpectedToken, "New expressions not implemented yet"));
}

ParseResult<ExpressionNode> ExpressionParser::parse_match_expression() {
    return ParseResult<ExpressionNode>::error(
        create_error(ErrorKind::UnexpectedToken, "Match expressions not implemented yet"));
}

ParseResult<ExpressionNode> ExpressionParser::parse_enum_variant() {
    return ParseResult<ExpressionNode>::error(
        create_error(ErrorKind::UnexpectedToken, "Enum variants not implemented yet"));
}

} // namespace Mycelium::Scripting::Lang