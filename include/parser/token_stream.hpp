#pragma once

#include "common/token.hpp"
#include <vector>
#include <initializer_list>
#include <stdexcept>
#include <memory>
#include <type_traits>

namespace Myre
{
    // Base pattern class - now uses shared_ptr for easier syntax
    class Pattern;
    using PatternPtr = std::shared_ptr<Pattern>;

    
    
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
        template<typename... Args>
        requires (std::is_convertible_v<Args, PatternPtr> && ...)
        bool check(Args... patterns)
        {
            std::vector<PatternPtr> pattern_list = {patterns...};
            return check_list(pattern_list);
        }
        bool check_list(const std::vector<PatternPtr> &patterns);

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

    class Pattern
    {
    public:
        virtual ~Pattern() = default;
        // Now takes remaining patterns for lookahead
        virtual bool match(const TokenStream &stream, size_t &offset,
                           const std::vector<PatternPtr> &remaining) const = 0;
    };

    // Single token
    class TokenPattern : public Pattern
    {
        TokenKind kind_;

    public:
        TokenPattern(TokenKind kind) : kind_(kind) {}
        bool match(const TokenStream &stream, size_t &offset,
                   const std::vector<PatternPtr> &) const override
        {
            if (stream.peek(offset).kind == kind_)
            {
                offset++;
                return true;
            }
            return false;
        }
    };

    // Any token(s)
    class AnyPattern : public Pattern
    {
        std::vector<TokenKind> kinds_;
        bool match_any_;

    public:
        AnyPattern() : match_any_(true) {} // No args = match any token
        AnyPattern(std::initializer_list<TokenKind> kinds)
            : kinds_(kinds), match_any_(false) {}

        bool match(const TokenStream &stream, size_t &offset,
                   const std::vector<PatternPtr> &) const override
        {
            if (match_any_)
            {
                offset++;
                return true;
            }
            TokenKind current = stream.peek(offset).kind;
            for (TokenKind kind : kinds_)
            {
                if (current == kind)
                {
                    offset++;
                    return true;
                }
            }
            return false;
        }
    };

    // One or more (with lookahead for non-greedy matching)
    class OneOrMorePattern : public Pattern
    {
        PatternPtr pattern_;

    public:
        OneOrMorePattern(PatternPtr pattern) : pattern_(pattern) {}

        bool match(const TokenStream &stream, size_t &offset,
                   const std::vector<PatternPtr> &remaining) const override
        {
            // Must match at least once
            if (!pattern_->match(stream, offset, {}))
            {
                return false;
            }

            // Keep matching until we can't or next pattern matches
            while (true)
            {
                // Check if remaining patterns would match at current position
                if (!remaining.empty())
                {
                    size_t test_offset = offset;
                    bool can_continue = true;
                    for (const auto &next : remaining)
                    {
                        if (!next->match(stream, test_offset, {}))
                        {
                            can_continue = false;
                            break;
                        }
                    }
                    if (can_continue)
                    {
                        break; // Stop here, let remaining patterns match
                    }
                }

                // Try to match pattern again
                if (!pattern_->match(stream, offset, {}))
                {
                    break;
                }
            }
            return true;
        }
    };

    // Zero or more
    class ZeroOrMorePattern : public Pattern
    {
        PatternPtr pattern_;

    public:
        ZeroOrMorePattern(PatternPtr pattern) : pattern_(pattern) {}

        bool match(const TokenStream &stream, size_t &offset,
                   const std::vector<PatternPtr> &remaining) const override
        {
            while (true)
            {
                // Check lookahead
                if (!remaining.empty())
                {
                    size_t test_offset = offset;
                    bool can_continue = true;
                    for (const auto &next : remaining)
                    {
                        if (!next->match(stream, test_offset, {}))
                        {
                            can_continue = false;
                            break;
                        }
                    }
                    if (can_continue)
                        break;
                }

                if (!pattern_->match(stream, offset, {}))
                    break;
            }
            return true;
        }
    };

    // Optional
    class OptionalPattern : public Pattern
    {
        PatternPtr pattern_;

    public:
        OptionalPattern(PatternPtr pattern) : pattern_(pattern) {}

        bool match(const TokenStream &stream, size_t &offset,
                   const std::vector<PatternPtr> &remaining) const override
        {
            pattern_->match(stream, offset, remaining);
            return true;
        }
    };

    // Factory functions for clean syntax
    inline PatternPtr Tok(TokenKind kind)
    {
        return std::make_shared<TokenPattern>(kind);
    }

    inline PatternPtr Any()
    {
        return std::make_shared<AnyPattern>();
    }

    inline PatternPtr Any(std::initializer_list<TokenKind> kinds)
    {
        return std::make_shared<AnyPattern>(kinds);
    }

    inline PatternPtr OnePlus(PatternPtr p)
    {
        return std::make_shared<OneOrMorePattern>(p);
    }

    inline PatternPtr ZeroPlus(PatternPtr p)
    {
        return std::make_shared<ZeroOrMorePattern>(p);
    }

    inline PatternPtr Opt(PatternPtr p)
    {
        return std::make_shared<OptionalPattern>(p);
    }

} // namespace Myre