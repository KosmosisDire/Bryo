#pragma once

#include "common/token.hpp"
#include <vector>
#include <initializer_list>

namespace Fern
{
    class TokenStream
    {
    public:
        TokenStream(std::vector<Token> tokens) : tokens_(std::move(tokens)), position_(0) {}

        // Core navigation
        const Token &current() const;
        const Token &peek(int offset = 1) const;
        const Token &previous() const;
        void advance();
        bool at_end() const;

        // Conditional consumption
        bool check(TokenKind kind) const;
        bool check_any(std::initializer_list<TokenKind> kinds) const;
        bool check_sequence(std::initializer_list<TokenKind> sequence) const;

        bool consume(TokenKind kind);
        bool consume_any(std::initializer_list<TokenKind> kinds);
        TokenKind consume_any_get(std::initializer_list<TokenKind> kinds);

        // Speculative parsing support
        struct Checkpoint
        {
            size_t position;
        };

        Checkpoint checkpoint() const { return {position_}; }
        void restore(Checkpoint cp) { position_ = cp.position; }

        // Skip to recovery points
        void skip_to(TokenKind kind);
        void skip_to_any(std::initializer_list<TokenKind> kinds);
        void skip_past(TokenKind kind);

        // Generic parsing support
        void splitRightShift(); // Split '>>' into '>' + '>'

        // Utility
        SourceRange location() const;
        size_t position() const { return position_; }

        std::string to_string() const;

    private:
        std::vector<Token> tokens_;
        size_t position_;

        void ensure_valid_position() const;
    };
}