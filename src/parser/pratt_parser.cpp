#include "parser/pratt_parser.hpp"
#include "parser/token_stream.hpp"
// Token utilities now in common/token.hpp
#include <algorithm>
#include <iostream>

namespace Mycelium::Scripting::Parser {

PrattParser::PrattParser(TokenStream& tokens, ParserContext& context, AstAllocator& allocator)
    : ParserBase(tokens, context, allocator)
    , allow_assignments_(true)
    , allow_ternary_(true)
    , expression_depth_(0)
    , debug_enabled_(false)
{
}

ExpressionNode* PrattParser::parse_expression(int min_precedence) {
    ExpressionContextGuard guard(*this);
    
    if (expression_depth_ > current_options_.max_depth) {
        report_error("Expression nesting too deep");
        return recover_from_expression_error();
    }
    
    // Parse the left-hand side (primary or unary expression)
    ExpressionNode* left = parse_unary();
    if (!left) {
        return nullptr;
    }
    
    // Parse binary operators with precedence climbing
    return parse_binary(left, min_precedence);
}

ExpressionNode* PrattParser::parse_primary() {
    switch (current().kind) {
        // Literals
        case TokenKind::IntegerLiteral:
        case TokenKind::FloatLiteral:
        case TokenKind::StringLiteral:
        case TokenKind::CharLiteral:
        case TokenKind::BooleanLiteral:
            return parse_literal();
        // Identifiers and keywords
        case TokenKind::Identifier:
            return parse_identifier_expression();
        case TokenKind::This:
            return parse_this_expression();
            
        // Grouped expressions
        case TokenKind::LeftParen:
            return parse_parenthesized_expression();
            
        // Special expressions
        case TokenKind::New:
            return parse_new_expression();
        case TokenKind::Typeof:
            return parse_typeof_expression();
        case TokenKind::Sizeof:
            return parse_sizeof_expression();
        case TokenKind::When:
            return parse_when_expression();
            
        // Enum member access (.MemberName)
        case TokenKind::Dot:
            return parse_enum_member_expression();
            
        default:
            report_unexpected_token("expression");
            return recover_from_expression_error();
    }
}

ExpressionNode* PrattParser::parse_unary() {
    if (current().is_unary_operator()) {
        Token op = current();
        advance();
        
        int precedence = op.get_unary_precedence();
        ExpressionNode* operand = parse_expression(precedence);
        
        if (!operand) {
            return nullptr;
        }
        
        auto unary_expr = create_node<UnaryExpressionNode>();
        unary_expr->opKind = op.to_unary_operator_kind();
        unary_expr->operatorToken = create_node<TokenNode>();
        unary_expr->operatorToken->tokenKind = op.kind;
        unary_expr->operatorToken->text = op.text;
        unary_expr->operand = operand;
        unary_expr->isPostfix = false;
        
        return unary_expr;
    }
    
    return parse_postfix(parse_primary());
}

ExpressionNode* PrattParser::parse_postfix(ExpressionNode* expr) {
    if (!expr) return nullptr;
    
    while (current().is_postfix_operator()) {
        switch (current().kind) {
            case TokenKind::LeftParen:
                expr = parse_call_expression(expr);
                break;
            case TokenKind::Dot:
                expr = parse_member_access(expr);
                break;
            case TokenKind::LeftBracket:
                expr = parse_indexer_expression(expr);
                break;
            case TokenKind::Increment:
            case TokenKind::Decrement: {
                Token op = current();
                advance();
                
                auto postfix_expr = create_node<UnaryExpressionNode>();
                postfix_expr->opKind = (op.kind == TokenKind::Increment) ? UnaryOperatorKind::PostIncrement : UnaryOperatorKind::PostDecrement;
                postfix_expr->operatorToken = create_node<TokenNode>();
                postfix_expr->operatorToken->tokenKind = op.kind;
                postfix_expr->operatorToken->text = op.text;
                postfix_expr->operand = expr;
                postfix_expr->isPostfix = true;
                expr = postfix_expr;
                break;
            }
            default:
                return expr;
        }
        
        if (!expr) break;
    }
    
    return expr;
}

ExpressionNode* PrattParser::parse_binary(ExpressionNode* left, int min_precedence) {
    while (!at_end() && current().is_binary_operator()) {
        auto op = current();
        int precedence = op.get_binary_precedence();
        
        if (precedence < min_precedence) {
            break;
        }
        
        advance();
        
        // Handle right-associative operators
        int next_min_precedence = op.is_right_associative() ? precedence : precedence + 1;
        
        ExpressionNode* right = parse_expression(next_min_precedence);
        if (!right) {
            return left; // Return partial result
        }
        
        // Check if this is an assignment operator and create the appropriate node
        if (op.is_assignment_operator()) {
            // Create assignment expression node
            auto assignment_expr = create_node<AssignmentExpressionNode>();
            assignment_expr->target = left;
            assignment_expr->opKind = op.to_assignment_operator_kind();
            assignment_expr->operatorToken = create_node<TokenNode>();
            assignment_expr->operatorToken->tokenKind = op.kind;
            assignment_expr->operatorToken->text = op.text;
            assignment_expr->source = right;
            left = assignment_expr;
        } else {
            // Create binary expression node
            auto binary_expr = create_node<BinaryExpressionNode>();
            binary_expr->left = left;
            binary_expr->opKind = op.to_binary_operator_kind();
            binary_expr->operatorToken = create_node<TokenNode>();
            binary_expr->operatorToken->tokenKind = op.kind;
            binary_expr->operatorToken->text = op.text;
            binary_expr->right = right;
            left = binary_expr;
        }
    }
    
    // Handle ternary operator
    if (allow_ternary_ && current().kind == TokenKind::Question) {
        return parse_conditional(left);
    }
    
    return left;
}

ExpressionNode* PrattParser::parse_assignment() {
    // This should be called when we're positioned at an assignment operator
    // and left expression is already parsed
    report_error("Assignment parsing not yet implemented");
    return nullptr;
}

ExpressionNode* PrattParser::parse_conditional(ExpressionNode* condition) {
    if (!condition || current().kind != TokenKind::Question) {
        return condition;
    }
    
    Token question_token = consume(TokenKind::Question);
    ExpressionNode* when_true = parse_expression();
    Token colon_token = consume(TokenKind::Colon);
    ExpressionNode* when_false = parse_expression();
    
    auto conditional = create_node<ConditionalExpressionNode>();
    conditional->condition = condition;
    conditional->question = create_node<TokenNode>();
    conditional->question->tokenKind = question_token.kind;
    conditional->question->text = question_token.text;
    conditional->whenTrue = when_true;
    conditional->colon = create_node<TokenNode>();
    conditional->colon->tokenKind = colon_token.kind;
    conditional->colon->text = colon_token.text;
    conditional->whenFalse = when_false;
    
    return conditional;
}

ExpressionNode* PrattParser::parse_call_expression(ExpressionNode* target) {
    if (!target || current().kind != TokenKind::LeftParen) {
        return target;
    }
    
    auto arguments = parse_argument_list();
    
    auto call_expr = create_node<CallExpressionNode>();
    call_expr->target = target;
    // Note: parse_argument_list already handles the parentheses
    // We would need to properly fill in the token fields here
    
    return call_expr;
}

ExpressionNode* PrattParser::parse_member_access(ExpressionNode* target) {
    if (!target || current().kind != TokenKind::Dot) {
        return target;
    }
    
    Token dot_token = consume(TokenKind::Dot);
    
    if (current().kind != TokenKind::Identifier) {
        report_expected(TokenKind::Identifier);
        return target;
    }
    
    auto member = create_identifier(current());
    advance();
    
    auto member_access = create_node<MemberAccessExpressionNode>();
    member_access->target = target;
    member_access->dotToken = create_node<TokenNode>();
    member_access->dotToken->tokenKind = dot_token.kind;
    member_access->dotToken->text = dot_token.text;
    member_access->member = member;
    
    return member_access;
}

ExpressionNode* PrattParser::parse_indexer_expression(ExpressionNode* target) {
    if (!target || current().kind != TokenKind::LeftBracket) {
        return target;
    }
    
    Token open_bracket = consume(TokenKind::LeftBracket);
    ExpressionNode* index = parse_expression();
    Token close_bracket = consume(TokenKind::RightBracket);
    
    auto indexer = create_node<IndexerExpressionNode>();
    indexer->target = target;
    indexer->openBracket = create_node<TokenNode>();
    indexer->openBracket->tokenKind = open_bracket.kind;
    indexer->openBracket->text = open_bracket.text;
    indexer->index = index;
    indexer->closeBracket = create_node<TokenNode>();
    indexer->closeBracket->tokenKind = close_bracket.kind;
    indexer->closeBracket->text = close_bracket.text;
    
    return indexer;
}

ExpressionNode* PrattParser::parse_cast_expression() {
    // Implementation for cast expressions
    report_error("Cast expression parsing not yet implemented");
    return nullptr;
}

ExpressionNode* PrattParser::parse_new_expression() {
    if (current().kind != TokenKind::New) {
        return nullptr;
    }
    
    Token new_token = consume(TokenKind::New);
    TypeNameNode* type = parse_type_name();
    
    auto new_expr = create_node<NewExpressionNode>();
    new_expr->newKeyword = create_node<TokenNode>();
    new_expr->newKeyword->tokenKind = new_token.kind;
    new_expr->newKeyword->text = new_token.text;
    new_expr->type = type;
    
    // Optional constructor call
    if (current().kind == TokenKind::LeftParen) {
        // Would parse constructor arguments here
    }
    
    return new_expr;
}

ExpressionNode* PrattParser::parse_typeof_expression() {
    // Implementation for typeof expressions
    report_error("TypeOf expression parsing not yet implemented");
    return nullptr;
}

ExpressionNode* PrattParser::parse_sizeof_expression() {
    // Implementation for sizeof expressions  
    report_error("SizeOf expression parsing not yet implemented");
    return nullptr;
}

ExpressionNode* PrattParser::parse_when_expression() {
    // Implementation for match expressions
    report_error("When expression parsing not yet implemented");
    return nullptr;
}

ExpressionNode* PrattParser::parse_range_expression(ExpressionNode* start) {
    // Implementation for range expressions
    report_error("Range expression parsing not yet implemented");
    return nullptr;
}

ExpressionNode* PrattParser::parse_identifier_expression() {
    if (current().kind != TokenKind::Identifier) {
        return nullptr;
    }
    
    auto identifier_expr = create_node<IdentifierExpressionNode>();
    identifier_expr->identifier = create_identifier(current());
    advance();
    
    return identifier_expr;
}

ExpressionNode* PrattParser::parse_this_expression() {
    if (current().kind != TokenKind::This) {
        return nullptr;
    }
    
    Token this_token = current();
    advance();
    
    auto this_expr = create_node<ThisExpressionNode>();
    this_expr->thisKeyword = create_node<TokenNode>();
    this_expr->thisKeyword->tokenKind = this_token.kind;
    this_expr->thisKeyword->text = this_token.text;
    
    return this_expr;
}

ExpressionNode* PrattParser::parse_enum_member_expression() {
    if (current().kind != TokenKind::Dot) {
        return nullptr;
    }
    
    Token dot_token = consume(TokenKind::Dot);
    
    if (current().kind != TokenKind::Identifier) {
        report_expected(TokenKind::Identifier);
        return nullptr;
    }
    
    auto member_name = create_identifier(current());
    advance();
    
    auto enum_member = create_node<EnumMemberExpressionNode>();
    enum_member->dot = create_node<TokenNode>();
    enum_member->dot->tokenKind = dot_token.kind;
    enum_member->dot->text = dot_token.text;
    enum_member->memberName = member_name;
    
    return enum_member;
}

ExpressionNode* PrattParser::parse_parenthesized_expression() {
    if (current().kind != TokenKind::LeftParen) {
        return nullptr;
    }
    
    Token open_paren = consume(TokenKind::LeftParen);
    ExpressionNode* expr = parse_expression();
    Token close_paren = consume(TokenKind::RightParen);
    
    auto paren_expr = create_node<ParenthesizedExpressionNode>();
    paren_expr->openParen = create_node<TokenNode>();
    paren_expr->openParen->tokenKind = open_paren.kind;
    paren_expr->openParen->text = open_paren.text;
    paren_expr->expression = expr;
    paren_expr->closeParen = create_node<TokenNode>();
    paren_expr->closeParen->tokenKind = close_paren.kind;
    paren_expr->closeParen->text = close_paren.text;
    
    return paren_expr;
}

ExpressionNode* PrattParser::recover_from_expression_error() {
    skip_to_expression_boundary();
    return nullptr;
}

void PrattParser::skip_to_expression_boundary() {
    synchronize_to({
        TokenKind::Semicolon,
        TokenKind::Comma,
        TokenKind::RightParen,
        TokenKind::RightBrace,
        TokenKind::RightBracket
    });
}

void PrattParser::enter_expression_context() {
    expression_depth_++;
    enter_context(ParsingContext::ExpressionContext);
}

void PrattParser::exit_expression_context() {
    if (expression_depth_ > 0) {
        expression_depth_--;
    }
    exit_context();
}

// ExpressionContextGuard implementation
PrattParser::ExpressionContextGuard::ExpressionContextGuard(PrattParser& parser) 
    : parser_(parser) {
    parser_.enter_expression_context();
}

PrattParser::ExpressionContextGuard::~ExpressionContextGuard() {
    parser_.exit_expression_context();
}

} // namespace Mycelium::Scripting::Parser