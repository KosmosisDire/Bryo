#pragma once

#include "common/token.hpp"
#include <vector>
#include <initializer_list>
#include <stdexcept>
#include <memory>
#include <type_traits>

namespace Myre
{
    // Token stream as a data structure containing all tokens from lexical analysis
    class TokenStream
    {
    public:
        // Constructors
        TokenStream() = default;
        explicit TokenStream(std::vector<Token> tokens)
            : tokens_(std::move(tokens)), position_(0) {}

        // Move construction/assignment
        TokenStream(TokenStream &&) = default;
        TokenStream &operator=(TokenStream &&) = default;

        // Copy construction/assignment
        TokenStream(const TokenStream &) = default;
        TokenStream &operator=(const TokenStream &) = default;

        // Core token access
        const Token &current() const;
        const Token &peek(int offset = 1) const;
        void advance();


        bool match(TokenKind kind);
        bool match(std::initializer_list<TokenKind> kinds);
        

        bool check(TokenKind kind) const;
        bool check(std::initializer_list<TokenKind> kinds) const;
        bool check_until(TokenKind kind, std::initializer_list<TokenKind> until) const;

        // Position and state queries
        bool at_end() const;
        size_t position() const { return position_; }
        size_t size() const { return tokens_.size(); }
        SourceRange location() const;

        // Backtracking support
        struct Checkpoint
        {
            size_t position;
        };

        Checkpoint save_checkpoint() const { return {position_}; }
        void restore_checkpoint(const Checkpoint &cp) { position_ = cp.position; }
        bool ahead_of_checkpoint(const Checkpoint &cp) const { return position_ > cp.position; }
        bool at_checkpoint(const Checkpoint &cp) const { return position_ == cp.position; }
        bool behind_checkpoint(const Checkpoint &cp) const { return position_ < cp.position; }

        // Direct access to tokens
        const std::vector<Token> &tokens() const { return tokens_; }

        // Random access by index
        const Token &at(size_t index) const
        {
            if (index >= tokens_.size())
            {
                throw std::out_of_range("TokenStream index out of range");
            }
            return tokens_[index];
        }

        const Token &operator[](size_t index) const
        {
            return tokens_[index]; // No bounds checking for performance
        }

        // String representation
        std::string to_string() const;

    private:
        std::vector<Token> tokens_;
        size_t position_ = 0;

        // Helper to ensure position is valid
        void ensure_valid_position() const;
    };


} // namespace Myre