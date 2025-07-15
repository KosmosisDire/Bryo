#pragma once

#include "common/token.hpp"
#include <vector>
#include <initializer_list>
#include <stdexcept>

namespace Mycelium::Scripting::Parser {

// Token stream as a data structure containing all tokens from lexical analysis
class TokenStream {
public:
    // Constructors
    TokenStream() = default;
    explicit TokenStream(std::vector<Token> tokens) 
        : tokens_(std::move(tokens)), position_(0) {}
    
    // Move construction/assignment
    TokenStream(TokenStream&&) = default;
    TokenStream& operator=(TokenStream&&) = default;
    
    // Copy construction/assignment
    TokenStream(const TokenStream&) = default;
    TokenStream& operator=(const TokenStream&) = default;
    
    // Core token access
    const Token& current() const;
    const Token& peek(int offset = 1) const;
    void advance();
    
    // Token matching and consumption
    bool match(TokenKind kind);
    bool match_any(std::initializer_list<TokenKind> kinds);
    Token consume(TokenKind expected);
    Token consume_any(std::initializer_list<TokenKind> kinds);
    
    // Position and state queries
    bool at_end() const;
    size_t position() const { return position_; }
    size_t size() const { return tokens_.size(); }
    SourceLocation location() const;
    
    // Backtracking support
    struct Checkpoint {
        size_t position;
    };
    
    Checkpoint save_checkpoint() const { return {position_}; }
    void restore_checkpoint(const Checkpoint& cp) { position_ = cp.position; }
    
    // Utility methods
    bool check(TokenKind kind) const;
    bool check_any(std::initializer_list<TokenKind> kinds) const;
    bool match_sequence(std::initializer_list<TokenKind> sequence) const;
    
    // Direct access to tokens
    const std::vector<Token>& tokens() const { return tokens_; }
    
    // Random access by index
    const Token& at(size_t index) const {
        if (index >= tokens_.size()) {
            throw std::out_of_range("TokenStream index out of range");
        }
        return tokens_[index];
    }
    
    const Token& operator[](size_t index) const {
        return tokens_[index];  // No bounds checking for performance
    }
    
    // Error reporting helpers
    std::string get_expected_message(TokenKind expected) const;
    std::string get_expected_message(std::initializer_list<TokenKind> expected) const;
    
    // String representation
    std::string to_string() const;
    
private:
    std::vector<Token> tokens_;
    size_t position_ = 0;
    
    // Helper to ensure position is valid
    void ensure_valid_position() const;
};

} // namespace Mycelium::Scripting::Parser