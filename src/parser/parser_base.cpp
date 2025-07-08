#include "parser/parser_base.hpp"
#include "parser/parse_result.hpp"
// Token utilities now in common/token.hpp
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

namespace Mycelium::Scripting::Parser {

// Helper function to describe expected tokens
static std::string describe_expected_tokens(std::initializer_list<TokenKind> kinds) {
    if (kinds.size() == 0) {
        return "nothing";
    }
    
    if (kinds.size() == 1) {
        return std::string(to_string(*kinds.begin()));
    }
    
    std::string result;
    auto it = kinds.begin();
    
    // Add all but the last with commas
    for (size_t i = 0; i < kinds.size() - 1; ++i, ++it) {
        if (i > 0) result += ", ";
        result += to_string(*it);
    }
    
    // Add "or" before the last one
    result += " or ";
    result += to_string(*it);
    
    return result;
}

ParserBase::ParserBase(TokenStream& tokens, ParserContext& context, AstAllocator& allocator)
    : tokens_(tokens), context_(context), allocator_(allocator) {
}

Token ParserBase::consume(TokenKind expected) {
    if (current().kind == expected) {
        Token token = current();
        advance();
        return token;
    }
    
    report_expected(expected);
    return current(); // Return current token even on error
}

Token ParserBase::consume_any(std::initializer_list<TokenKind> kinds) {
    for (TokenKind kind : kinds) {
        if (current().kind == kind) {
            Token token = current();
            advance();
            return token;
        }
    }
    
    report_expected_any(kinds);
    return current();
}

Token ParserBase::consume_with_recovery(TokenKind expected, std::initializer_list<TokenKind> recovery_tokens) {
    if (current().kind == expected) {
        Token token = current();
        advance();
        return token;
    }
    
    report_expected(expected);
    
    // Try to recover by looking for recovery tokens
    if (recovery_tokens.size() > 0) {
        synchronize_to(recovery_tokens);
    }
    
    return current();
}

// New ParseResult-based token consumption methods

ParseResult<Token> ParserBase::consume_token(TokenKind expected) {
    if (current().kind == expected) {
        Token token = current();
        advance();
        return ParseResult<Token>::success(std::move(token));
    }
    
    // Create error diagnostic
    ParserDiagnostic error(DiagnosticLevel::Error, 
                          "Expected '" + std::string(to_string(expected)) + 
                          "' but found '" + std::string(to_string(current().kind)) + "'",
                          current().range());
    
    return ParseResult<Token>::error(std::move(error));
}

ParseResult<Token> ParserBase::consume_any_token(std::initializer_list<TokenKind> kinds) {
    for (TokenKind kind : kinds) {
        if (current().kind == kind) {
            Token token = current();
            advance();
            return ParseResult<Token>::success(std::move(token));
        }
    }
    
    // Create error diagnostic with expected options
    std::string expected_str = describe_expected_tokens(kinds);
    ParserDiagnostic error(DiagnosticLevel::Error,
                          "Expected " + expected_str + 
                          " but found '" + std::string(to_string(current().kind)) + "'",
                          current().range());
    
    return ParseResult<Token>::error(std::move(error));
}

ParseResult<Token> ParserBase::consume_token_with_recovery(TokenKind expected, 
                                                          std::initializer_list<TokenKind> recovery_tokens) {
    if (current().kind == expected) {
        Token token = current();
        advance();
        return ParseResult<Token>::success(std::move(token));
    }
    
    // Create error diagnostic
    ParserDiagnostic error(DiagnosticLevel::Error,
                          "Expected '" + std::string(to_string(expected)) + 
                          "' but found '" + std::string(to_string(current().kind)) + "'",
                          current().range());
    
    // Attempt recovery
    if (recovery_tokens.size() > 0) {
        synchronize_to(recovery_tokens);
        
        // Check if we recovered to a valid state
        for (TokenKind recovery_kind : recovery_tokens) {
            if (current().kind == recovery_kind) {
                Token recovery_token = current();
                advance();
                auto result = ParseResult<Token>::success(std::move(recovery_token));
                result.add_error(std::move(error));
                result.mark_recovered();
                return result;
            }
        }
    }
    
    // Recovery failed, return error with current token
    return ParseResult<Token>::error(std::move(error));
}

// New ParseResult-based AST parsing methods

ParseResult<IdentifierNode*> ParserBase::parse_identifier_result() {
    auto token_result = consume_token(TokenKind::Identifier);
    
    if (!token_result.has_value()) {
        return ParseResult<IdentifierNode*>::errors(token_result.errors());
    }
    
    auto identifier = create_identifier(token_result.value());
    auto result = ParseResult<IdentifierNode*>::success(identifier);
    
    // Propagate any warnings/errors from token parsing
    if (token_result.has_errors()) {
        result.add_errors(token_result.errors());
    }
    
    return result;
}

ParseResult<TypeNameNode*> ParserBase::parse_type_name_result() {
    auto identifier_result = parse_identifier_result();
    
    if (!identifier_result.has_value()) {
        return ParseResult<TypeNameNode*>::errors(identifier_result.errors());
    }
    
    auto type_name = create_node<TypeNameNode>();
    type_name->identifier = identifier_result.value();
    
    auto result = ParseResult<TypeNameNode*>::success(type_name);
    
    // Propagate any errors from identifier parsing
    if (identifier_result.has_errors()) {
        result.add_errors(identifier_result.errors());
    }
    
    return result;
}

ParseResult<QualifiedTypeNameNode*> ParserBase::parse_qualified_type_name_result() {
    auto type_result = parse_type_name_result();
    
    if (!type_result.has_value()) {
        return ParseResult<QualifiedTypeNameNode*>::errors(type_result.errors());
    }
    
    TypeNameNode* left = type_result.value();
    std::vector<ParserDiagnostic> accumulated_errors;
    
    // Add any errors from the initial type name
    if (type_result.has_errors()) {
        accumulated_errors.insert(accumulated_errors.end(), 
                                type_result.errors().begin(), type_result.errors().end());
    }
    
    // Parse qualification chain
    while (match(TokenKind::DoubleColon)) {
        auto identifier_result = parse_identifier_result();
        
        if (!identifier_result.has_value()) {
            // Add errors but continue if possible
            accumulated_errors.insert(accumulated_errors.end(),
                                    identifier_result.errors().begin(), identifier_result.errors().end());
            break;
        }
        
        auto qualified = create_node<QualifiedTypeNameNode>();
        qualified->left = left;
        qualified->right = identifier_result.value();
        
        // Add any errors from identifier parsing
        if (identifier_result.has_errors()) {
            accumulated_errors.insert(accumulated_errors.end(),
                                    identifier_result.errors().begin(), identifier_result.errors().end());
        }
        
        left = qualified;
    }
    
    auto result = ParseResult<QualifiedTypeNameNode*>::success(static_cast<QualifiedTypeNameNode*>(left));
    result.add_errors(accumulated_errors);
    
    return result;
}

IdentifierNode* ParserBase::create_identifier(const Token& token) {
    auto identifier = create_node<IdentifierNode>();
    identifier->name = std::string_view(token.text);
    return identifier;
}

IdentifierNode* ParserBase::parse_identifier() {
    if (current().kind != TokenKind::Identifier) {
        report_expected(TokenKind::Identifier);
        return nullptr;
    }
    
    auto identifier = create_identifier(current());
    advance();
    return identifier;
}

TypeNameNode* ParserBase::parse_type_name() {
    if (current().kind != TokenKind::Identifier) {
        report_expected(TokenKind::Identifier);
        return nullptr;
    }
    
    auto type_name = create_node<TypeNameNode>();
    type_name->identifier = create_identifier(current());
    advance();
    return type_name;
}

QualifiedTypeNameNode* ParserBase::parse_qualified_type_name() {
    auto left = parse_type_name();
    if (!left) return nullptr;
    
    while (match(TokenKind::DoubleColon)) {
        if (current().kind != TokenKind::Identifier) {
            report_expected(TokenKind::Identifier);
            break;
        }
        
        auto qualified = create_node<QualifiedTypeNameNode>();
        qualified->left = left;
        qualified->right = create_identifier(current());
        advance();
        
        left = qualified;
    }
    
    return static_cast<QualifiedTypeNameNode*>(left);
}

std::vector<ModifierKind> ParserBase::parse_modifiers() {
    std::vector<ModifierKind> modifiers;
    
    while (current().is_modifier())
    {
        ModifierKind modifier = current().to_modifier_kind();
        modifiers.push_back(modifier);
        advance();
    }
    
    // Validate modifier combination
    if (!validate_modifier_combination(modifiers)) {
        report_error("Invalid modifier combination");
    }
    
    return modifiers;
}

LiteralExpressionNode* ParserBase::parse_literal() {
    if (!current().is_literal()) {
        report_error("Expected literal");
        return nullptr;
    }
    
    auto literal = create_node<LiteralExpressionNode>();
    literal->token = create_node<TokenNode>();

    literal->token->tokenKind = current().kind;
    literal->token->text = current().text;
    literal->kind = current().to_literal_kind();

    advance();
    return literal;
}

ExpressionNode* ParserBase::parse_primary_expression() {
    switch (current().kind) {
        case TokenKind::IntegerLiteral:
        case TokenKind::FloatLiteral:
        case TokenKind::StringLiteral:
        case TokenKind::CharLiteral:
        case TokenKind::BooleanLiteral:
            return parse_literal();
            
        case TokenKind::Identifier: {
            auto identifier_expr = create_node<IdentifierExpressionNode>();
            identifier_expr->identifier = create_identifier(current());
            advance();
            return identifier_expr;
        }
        
        case TokenKind::This: {
            auto this_expr = create_node<ThisExpressionNode>();
            advance();
            return this_expr;
        }
        
        case TokenKind::LeftParen: {
            advance(); // consume '('
            auto expr = parse_primary_expression(); // This would delegate to expression parser
            consume(TokenKind::RightParen);
            
            auto paren_expr = create_node<ParenthesizedExpressionNode>();
            paren_expr->expression = expr;
            return paren_expr;
        }
        
        default:
            report_unexpected_token("primary expression");
            return nullptr;
    }
}

std::vector<ParameterNode*> ParserBase::parse_parameter_list() {
    std::vector<ParameterNode*> parameters;
    
    if (!match(TokenKind::LeftParen)) {
        report_expected(TokenKind::LeftParen);
        return parameters;
    }
    
    if (!check(TokenKind::RightParen)) {
        do {
            auto param = create_node<ParameterNode>();
            
            // Parse parameter name
            param->name = parse_identifier();
            if (!param->name) break;
            
            // Parse colon
            if (!consume(TokenKind::Colon).is(TokenKind::Colon)) break;
            
            // Parse parameter type
            param->type = parse_type_name();
            if (!param->type) break;
            
            // Parse optional default value
            if (match(TokenKind::Assign)) {
                param->defaultValue = parse_primary_expression();
            }
            
            parameters.push_back(param);
            
        } while (match(TokenKind::Comma));
    }
    
    consume(TokenKind::RightParen);
    return parameters;
}

std::vector<ExpressionNode*> ParserBase::parse_argument_list() {
    std::vector<ExpressionNode*> arguments;
    
    if (!match(TokenKind::LeftParen)) {
        report_expected(TokenKind::LeftParen);
        return arguments;
    }
    
    if (!check(TokenKind::RightParen)) {
        do {
            auto arg = parse_primary_expression();
            if (arg) {
                arguments.push_back(arg);
            }
        } while (match(TokenKind::Comma));
    }
    
    consume(TokenKind::RightParen);
    return arguments;
}

void ParserBase::report_error(const std::string& message) {
    context_.report_error(message, current().location);
}

void ParserBase::report_error(const std::string& message, SourceRange range) {
    context_.report_error(message, range);
}

void ParserBase::report_expected(TokenKind expected) {
    context_.report_missing_token(expected, current().location);
}

void ParserBase::report_expected_any(std::initializer_list<TokenKind> expected) {
    std::string message = "Expected " + describe_expected_tokens(expected);
    message += " but found '" + std::string(to_string(current().kind)) + "'";
    report_error(message);
}

void ParserBase::report_unexpected_token(const std::string& context) {
    std::string message = "Unexpected token '" + std::string(to_string(current().kind)) + "'";
    if (!context.empty()) {
        message += " in " + context;
    }
    report_error(message);
}

void ParserBase::synchronize() {
    // Skip to a likely synchronization point
    synchronize_to({
        TokenKind::Semicolon,
        TokenKind::LeftBrace,
        TokenKind::RightBrace,
        TokenKind::Fn,
        TokenKind::Type,
        TokenKind::Interface,
        TokenKind::Enum,
        TokenKind::If,
        TokenKind::While,
        TokenKind::For,
        TokenKind::Return
    });
}

void ParserBase::synchronize_to(std::initializer_list<TokenKind> sync_tokens) {
    while (!at_end()) {
        for (TokenKind token : sync_tokens) {
            if (current().kind == token) {
                return;
            }
        }
        advance();
    }
}

void ParserBase::skip_until(std::initializer_list<TokenKind> stop_tokens) {
    while (!at_end()) {
        for (TokenKind token : stop_tokens) {
            if (current().kind == token) {
                return;
            }
        }
        advance();
    }
}

void ParserBase::skip_to_matching_delimiter(TokenKind open, TokenKind close) {
    if (current().kind != open) return;
    
    int depth = 1;
    advance(); // skip opening delimiter
    
    while (!at_end() && depth > 0) {
        if (current().kind == open) {
            depth++;
        } else if (current().kind == close) {
            depth--;
        }
        advance();
    }
}


SourceRange ParserBase::make_range(const Token& start, const Token& end) const {
    return SourceRange(start.location, end.location + end.width);
}

SourceRange ParserBase::make_range(SourceLocation start, SourceLocation end) const {
    return SourceRange(start, end);
}

bool ParserBase::has_leading_comments(const Token& token) const {
    return !token.leading_trivia.empty();
}

bool ParserBase::has_trailing_comments(const Token& token) const {
    return !token.trailing_trivia.empty();
}

std::vector<std::string> ParserBase::extract_doc_comments(const Token& token) const {
    std::vector<std::string> doc_comments;
    
    // Extract from leading trivia
    for (const auto& trivia : token.leading_trivia) {
        if (trivia.kind == TriviaKind::DocComment) {
            // Would extract the actual comment text here
            doc_comments.push_back("/* doc comment */");
        }
    }
    
    return doc_comments;
}

bool ParserBase::is_valid_identifier_name(const std::string& name) const {
    if (name.empty()) return false;
    
    // Check first character
    if (!std::isalpha(name[0]) && name[0] != '_') return false;
    
    // Check remaining characters
    for (size_t i = 1; i < name.size(); ++i) {
        if (!std::isalnum(name[i]) && name[i] != '_') return false;
    }
    
    return true;
}


bool ParserBase::validate_modifier_combination(const std::vector<ModifierKind>& modifiers) const {
    // Check for conflicting modifiers
    bool has_public = false, has_private = false, has_protected = false;
    bool has_static = false, has_virtual = false;
    
    for (ModifierKind modifier : modifiers) {
        switch (modifier) {
            case ModifierKind::Public:
                if (has_private || has_protected) return false;
                has_public = true;
                break;
            case ModifierKind::Private:
                if (has_public || has_protected) return false;
                has_private = true;
                break;
            case ModifierKind::Protected:
                if (has_public || has_private) return false;
                has_protected = true;
                break;
            case ModifierKind::Static:
                has_static = true;
                break;
            case ModifierKind::Virtual:
                has_virtual = true;
                break;
            default:
                break;
        }
    }
    
    // Static and virtual are mutually exclusive
    if (has_static && has_virtual) return false;
    
    return true;
}



} // namespace Mycelium::Scripting::Parser