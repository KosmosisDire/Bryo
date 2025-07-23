#pragma once

#include "ast/ast.hpp"
#include "parse_result.h"
#include "token_stream.hpp"
#include <vector>
#include <memory>
#include <iostream>
#include <common/logger.hpp>

namespace Mycelium::Scripting::Lang {

// Forward declarations for helper parsers
class ExpressionParser;
class StatementParser;
class DeclarationParser;

// Diagnostic collection for error reporting
struct Diagnostic {
    ErrorKind kind;
    std::string message;
    SourceLocation location;
    int width;

    static Diagnostic from_error_node(ErrorNode* error) {
        return {error->kind, error->error_message, 
                SourceLocation(static_cast<uint32_t>(error->sourceStart), 1, 1), 
                error->sourceLength};
    }

    std::string format() const {
        return "Error at offset " + std::to_string(location.offset) + 
               " (line " + std::to_string(location.line) + 
               ", column " + std::to_string(location.column) + "): " + message;
    }
};

class DiagnosticCollection {
private:
    std::vector<Diagnostic> diagnostics_;

public:
    void add(const Diagnostic& diagnostic) {
        diagnostics_.push_back(diagnostic);
    }

    const std::vector<Diagnostic>& get_diagnostics() const {
        return diagnostics_;
    }

    auto begin() const { return diagnostics_.begin(); }
    auto end() const { return diagnostics_.end(); }
    size_t size() const { return diagnostics_.size(); }
    bool empty() const { return diagnostics_.empty(); }

    void print()
    {
        for (const auto& diag : diagnostics_) {
            LOG_ERROR(diag.format(), LogCategory::PARSER);
        }
    }
};

// High-performance ParseContext with RAII guards
class ParseContext {
private:
    TokenStream& tokens_;
    bool in_loop_context_;
    bool in_function_context_;

public:
    size_t position;  // Public for recovery progress checking
    
    ParseContext(TokenStream& t) 
        : tokens_(t), position(0), in_loop_context_(false), in_function_context_(false) {}
    
    // Hot path methods - zero overhead
    void advance() { 
        if (position < tokens_.size()) {
            position++; 
        }
    }
    
    const Token& current() const { 
        return position < tokens_.size() ? tokens_[position] : eof_token(); 
    }
    
    const Token& peek(int offset = 1) const { 
        size_t target_pos = position + offset;
        return target_pos < tokens_.size() ? tokens_[target_pos] : eof_token();
    }
    
    bool check(TokenKind kind) const { 
        return !at_end() && current().kind == kind; 
    }
    
    bool at_end() const { 
        return position >= tokens_.size() || (position < tokens_.size() && current().kind == TokenKind::EndOfFile); 
    }

private:
    // Static EOF token to avoid repeated allocation
    static const Token& eof_token() {
        static Token eof;
        eof.kind = TokenKind::EndOfFile;
        return eof;
    }

public:
    // Context queries
    bool in_loop() const { return in_loop_context_; }
    bool in_function() const { return in_function_context_; }
    
    // Simple RAII for context
    struct ContextSaver {
        ParseContext& ctx;
        bool old_loop, old_function;
        
        ContextSaver(ParseContext& c) 
            : ctx(c), old_loop(c.in_loop_context_), old_function(c.in_function_context_) {}
        
        ~ContextSaver() { 
            ctx.in_loop_context_ = old_loop; 
            ctx.in_function_context_ = old_function; 
        }
        
        ContextSaver(const ContextSaver&) = delete;
        ContextSaver& operator=(const ContextSaver&) = delete;
    };
    
    // Factory method for context saving
    ContextSaver save_context() { return ContextSaver(*this); }
    
    // Direct context modification
    void set_loop_context(bool value) { in_loop_context_ = value; }
    void set_function_context(bool value) { in_function_context_ = value; }
    
    // Access to underlying token stream
    TokenStream& tokens() { return tokens_; }
};

// Simple recovery system
class SimpleRecovery {
public:
    // Single, well-tested recovery strategy
    void recover_to_safe_point(ParseContext& context) {
        while (!context.at_end()) {
            TokenKind kind = context.current().kind;
            
            // Safe harbors: tokens that usually start new constructs
            if (kind == TokenKind::Semicolon ||       // Statement separator
                kind == TokenKind::LeftBrace ||       // Block start
                kind == TokenKind::RightBrace ||      // Block end
                kind == TokenKind::Fn ||              // Function declaration
                kind == TokenKind::Type ||            // Type declaration
                kind == TokenKind::If ||              // If statement
                kind == TokenKind::While ||           // While statement
                kind == TokenKind::For) {             // For statement
                
                // Skip semicolon, but stop at other safe points
                if (kind == TokenKind::Semicolon) context.advance();
                break;
            }
            context.advance();
        }
    }
    
    // Simple error creation
    ErrorNode* create_error(ErrorKind kind, const char* message, ParseContext& context, AstAllocator& allocator) {
        return ErrorNode::create(kind, message, context.current(), allocator);
    }
    
};

// Main Parser class
class Parser {
private:
    AstAllocator allocator_;
    ParseContext context_;
    DiagnosticCollection diagnostics_;
    SimpleRecovery recovery_;
    
    // Forward declarations for helper parsers
    std::unique_ptr<ExpressionParser> expr_parser_;
    std::unique_ptr<StatementParser> stmt_parser_;
    std::unique_ptr<DeclarationParser> decl_parser_;
    
    
public:
    Parser(TokenStream& tokens);
    ~Parser();
    
    // Main parsing entry point
    ParseResult<CompilationUnitNode> parse();
    
    // Token management with synthetic token insertion for recovery
    bool match(TokenKind kind) {
        if (context_.check(kind)) {
            context_.advance();
            return true;
        }
        return false;
    }
    
    bool expect(TokenKind expected, const char* error_msg) {
        if (context_.check(expected)) {
            context_.advance();
            return true;
        }
        
        // Report error and return false
        create_error(ErrorKind::MissingToken, error_msg);
        return false;
    }
    
    // Type parsing helper 
    ParseResult<TypeNameNode> parse_type_expression() {
        if (!context_.check(TokenKind::Identifier)) {
            return ParseResult<TypeNameNode>::error(
                create_error(ErrorKind::MissingToken, "Expected type name"));
        }
        
        const Token& type_token = context_.current();
        context_.advance();
        
        auto* type_name = allocator_.alloc<TypeNameNode>();
        type_name->tokenKind = type_token.kind;
        type_name->contains_errors = false;
        auto* identifier = allocator_.alloc<IdentifierNode>();
        identifier->tokenKind = TokenKind::Identifier;
        identifier->name = type_token.text;
        identifier->contains_errors = false;
        type_name->identifier = identifier;
        
        // Check for array type suffix []
        if (context_.check(TokenKind::LeftBracket)) {
            context_.advance(); // consume [
            
            // For now, we only support empty brackets [] for dynamic arrays
            // Later we can add support for fixed size arrays like [5]
            if (!context_.check(TokenKind::RightBracket)) {
                // Skip to closing bracket
                while (!context_.check(TokenKind::RightBracket) && !context_.at_end()) {
                    context_.advance();
                }
            }
            
            if (context_.check(TokenKind::RightBracket)) {
                context_.advance(); // consume ]
                
                // Create ArrayTypeNameNode
                auto* array_type = allocator_.alloc<ArrayTypeNameNode>();
                array_type->tokenKind = TokenKind::Identifier;
                array_type->contains_errors = false;
                array_type->elementType = type_name;
                
                return ParseResult<TypeNameNode>::success(array_type);
            } else {
                return ParseResult<TypeNameNode>::error(
                    create_error(ErrorKind::MissingToken, "Expected ']' after '['"));
            }
        }
        
        return ParseResult<TypeNameNode>::success(type_name);
    }
    
    // Helper access methods
    AstAllocator& get_allocator() { return allocator_; }
    ParseContext& get_context() { return context_; }
    DiagnosticCollection& get_diagnostics() { return diagnostics_; }
    SimpleRecovery& get_recovery() { return recovery_; }
    ExpressionParser& get_expression_parser() const { return *expr_parser_; }
    StatementParser& get_statement_parser() const { return *stmt_parser_; }
    DeclarationParser& get_declaration_parser() const { return *decl_parser_; }
    
    // Direct error creation for helper parsers
    ErrorNode* create_error(const ErrorKind kind, const char* msg) {
        auto* error = recovery_.create_error(kind, msg, context_, allocator_);
        diagnostics_.add(Diagnostic::from_error_node(error));
        return error;
    }
    
private:
    // Top-level parsing dispatcher
    ParseResult<StatementNode> parse_top_level_construct();
};

} // namespace Mycelium::Scripting::Lang