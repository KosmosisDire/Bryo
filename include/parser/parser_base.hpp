#pragma once

#include "parser/token_stream.hpp"
#include "parser/parser_context.hpp"
#include "parser/parse_result.hpp"
#include "ast/ast.hpp"
#include "ast/ast_allocator.hpp"
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace Mycelium::Scripting::Parser {

// Forward declarations
class RecursiveParser;
class PrattParser;

using namespace Mycelium::Scripting::Lang;

// Base class for all parsers providing common functionality
class ParserBase {
public:
    // Constructor
    ParserBase(TokenStream& tokens, ParserContext& context, AstAllocator& allocator);
    
    // Destructor
    virtual ~ParserBase() = default;
    
    // Non-copyable but movable
    ParserBase(const ParserBase&) = delete;
    ParserBase& operator=(const ParserBase&) = delete;
    ParserBase(ParserBase&&) = default;
    ParserBase& operator=(ParserBase&&) = default;

protected:
    // Core parser state
    TokenStream& tokens_;
    ParserContext& context_;
    AstAllocator& allocator_;
    
    // Token access and consumption
    const Token& current() const { return tokens_.current(); }
    const Token& peek(int offset = 1) const { return tokens_.peek(offset); }
    void advance() { tokens_.advance(); }
    bool at_end() const { return tokens_.at_end(); }
    
    // Token checking utilities
    bool check(TokenKind kind) const { return tokens_.check(kind); }
    bool check_any(std::initializer_list<TokenKind> kinds) const { return tokens_.check_any(kinds); }
    bool match(TokenKind kind) { return tokens_.match(kind); }
    bool match_any(std::initializer_list<TokenKind> kinds) { return tokens_.match_any(kinds); }
    
    // Token consumption with error handling (Legacy methods - will be deprecated)
    Token consume(TokenKind expected);
    Token consume_any(std::initializer_list<TokenKind> kinds);
    Token consume_with_recovery(TokenKind expected, std::initializer_list<TokenKind> recovery_tokens = {});
    
public:
    // New ParseResult-based token consumption methods (public for testing)
    ParseResult<Token> consume_token(TokenKind expected);
    ParseResult<Token> consume_any_token(std::initializer_list<TokenKind> kinds);
    ParseResult<Token> consume_token_with_recovery(TokenKind expected, std::initializer_list<TokenKind> recovery_tokens = {});
    
protected:
    
    // AST node creation helpers
    template<typename T, typename... Args>
    T* create_node(Args&&... args) {
        static_assert(std::is_base_of_v<AstNode, T>, "T must derive from AstNode");
        return allocator_.alloc<T>(std::forward<Args>(args)...);
    }
    
    // Common AST creation utilities (Legacy methods)
    IdentifierNode* create_identifier(const Token& token);
    IdentifierNode* parse_identifier();
    IdentifierNode* parse_contextual_identifier(); // Allows keywords as identifiers in certain contexts
    TypeNameNode* parse_type_name();
    QualifiedTypeNameNode* parse_qualified_type_name();
    
public:
    // New ParseResult-based AST parsing methods (public for testing)
    ParseResult<IdentifierNode*> parse_identifier_result();
    ParseResult<TypeNameNode*> parse_type_name_result();
    ParseResult<QualifiedTypeNameNode*> parse_qualified_type_name_result();
    
protected:
    
    // Modifier parsing
    std::vector<ModifierKind> parse_modifiers();
    bool is_modifier_token(TokenKind kind) const;
    ModifierKind token_to_modifier(TokenKind kind) const;
    
    // Literal parsing helpers
    LiteralExpressionNode* parse_literal();
    ExpressionNode* parse_primary_expression();
    
    // Parameter and argument list parsing
    std::vector<ParameterNode*> parse_parameter_list();
    std::vector<ExpressionNode*> parse_argument_list();
    
    // Error handling and recovery
    void report_error(const std::string& message);
    void report_error(const std::string& message, SourceRange range);
    void report_expected(TokenKind expected);
    void report_expected_any(std::initializer_list<TokenKind> expected);
    void report_unexpected_token(const std::string& context = "");
    
    // Synchronization for error recovery
    void synchronize();
    void synchronize_to(std::initializer_list<TokenKind> sync_tokens);
    void skip_until(std::initializer_list<TokenKind> stop_tokens);
    void skip_to_matching_delimiter(TokenKind open, TokenKind close);
    
    // Parser state management
    bool in_type_context() const { return context_.in_type_context(); }
    bool in_function_context() const { return context_.in_function_context(); }
    bool in_expression_context() const { return context_.in_expression_context(); }
    
    // Feature availability checks
    bool is_feature_enabled(LanguageFeature feature) const { 
        return context_.is_feature_enabled(feature); 
    }
    
    
    
    // Source range utilities
    SourceRange make_range(const Token& start, const Token& end) const;
    SourceRange make_range(SourceLocation start, SourceLocation end) const;
    SourceLocation current_location() const { return current().location; }
    
    // Comments and trivia handling
    bool has_leading_comments(const Token& token) const;
    bool has_trailing_comments(const Token& token) const;
    std::vector<std::string> extract_doc_comments(const Token& token) const;
    
    // Validation helpers
    bool is_valid_identifier_name(const std::string& name) const;
    bool validate_modifier_combination(const std::vector<ModifierKind>& modifiers) const;

public:
    // Context management helpers (public for guard classes)
    void enter_context(ParsingContext context) { context_.push_context(context); }
    void exit_context() { context_.pop_context(); }
    
    // Backtracking support (public for guard classes)
    TokenStream::Checkpoint save_position() { return tokens_.save_checkpoint(); }
    void restore_position(const TokenStream::Checkpoint& cp) { tokens_.restore_checkpoint(cp); }

private:
    // Internal helper methods
    void report_diagnostic_at_current(DiagnosticLevel level, const std::string& message);
    bool try_recover_at_delimiter(TokenKind delimiter);
    
};

// RAII helper for parser context management
class ParserContextGuard {
public:
    ParserContextGuard(ParserBase& parser, ParsingContext context)
        : parser_(parser) {
        parser_.enter_context(context);
    }
    
    ~ParserContextGuard() {
        parser_.exit_context();
    }
    
    ParserContextGuard(const ParserContextGuard&) = delete;
    ParserContextGuard& operator=(const ParserContextGuard&) = delete;
    
private:
    ParserBase& parser_;
};

// Macro for easy context scoping
#define PARSER_CONTEXT(parser, context) \
    ParserContextGuard _guard(parser, context)

// RAII helper for checkpoint management
class CheckpointGuard {
public:
    explicit CheckpointGuard(ParserBase& parser)
        : parser_(parser), checkpoint_(parser.save_position()), released_(false) {
    }
    
    ~CheckpointGuard() {
        if (!released_) {
            parser_.restore_position(checkpoint_);
        }
    }
    
    void commit() {
        released_ = true;
    }
    
    void rollback() {
        if (!released_) {
            parser_.restore_position(checkpoint_);
            released_ = true;
        }
    }
    
    CheckpointGuard(const CheckpointGuard&) = delete;
    CheckpointGuard& operator=(const CheckpointGuard&) = delete;
    
private:
    ParserBase& parser_;
    TokenStream::Checkpoint checkpoint_;
    bool released_;
};


} // namespace Mycelium::Scripting::Parser