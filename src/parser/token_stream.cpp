#include "parser/token_stream.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <iostream>

namespace Myre
{
    const Token &TokenStream::current() const
    {
        ensure_valid_position();
        return tokens_[position_];
    }

    const Token &TokenStream::peek(int offset) const
    {
        if (offset < 0)
        {
            // Handle negative offsets (looking backward)
            if (static_cast<size_t>(-offset) > position_)
            {
                // Would go before the beginning, return first token
                return tokens_[0];
            }
            return tokens_[position_ - static_cast<size_t>(-offset)];
        }

        size_t target_pos = position_ + static_cast<size_t>(offset);
        if (target_pos >= tokens_.size())
        {
            // Return last token (should be EOF) if position is beyond available tokens
            return tokens_.back();
        }

        return tokens_[target_pos];
    }

    void TokenStream::advance()
    {
        if (!at_end())
        {
            position_++;
        }
    }

    bool TokenStream::match(TokenKind kind)
    {
        if (check(kind))
        {
            advance();
            return true;
        }
        return false;
    }

    bool TokenStream::match(std::initializer_list<TokenKind> kinds)
    {
        for (TokenKind kind : kinds)
        {
            if (match(kind))
            {
                return true;
            }
        }
        return false;
    }


    bool TokenStream::check_list(const std::vector<PatternPtr> &patterns)
    {
        size_t offset = 0;
        for (size_t i = 0; i < patterns.size(); ++i)
        {
            // Create remaining patterns vector
            std::vector<PatternPtr> remaining;
            for (size_t j = i + 1; j < patterns.size(); ++j)
            {
                remaining.push_back(patterns[j]);
            }

            if (!patterns[i]->match(*this, offset, remaining))
            {
                return false;
            }
        }
        position_ += offset;
        return true;
    }

    bool TokenStream::at_end() const
    {

        return position_ >= tokens_.size() ||
               (position_ < tokens_.size() && tokens_[position_].kind == TokenKind::EndOfFile);
    }

    SourceRange TokenStream::location() const
    {
        ensure_valid_position();
        return tokens_[position_].location;
    }

    bool TokenStream::check(TokenKind kind) const
    {
        return current().kind == kind;
    }

    bool TokenStream::check(std::initializer_list<TokenKind> kinds) const
    {
        TokenKind current_kind = current().kind;
        for (TokenKind kind : kinds)
        {
            if (current_kind == kind)
            {
                return true;
            }
        }
        return false;
    }

    void TokenStream::ensure_valid_position() const
    {
        if (tokens_.empty())
        {
            throw std::runtime_error("TokenStream is empty");
        }
        if (position_ >= tokens_.size())
        {
            throw std::runtime_error("TokenStream position out of bounds");
        }
    }

    std::string TokenStream::to_string() const
    {
        std::ostringstream oss;
        oss << "TokenStream (" << tokens_.size() << " tokens, position=" << position_ << "):\n";

        for (size_t i = 0; i < tokens_.size(); ++i)
        {
            const Token &token = tokens_[i];

            // Mark current position with an arrow
            if (i == position_)
            {
                oss << " --> ";
            }
            else
            {
                oss << "     ";
            }

            // Token index
            oss << "[" << std::setw(3) << i << "] ";

            // Token location
            oss << "(" << std::setw(4) << token.location.start.line
                << ":" << std::setw(3) << token.location.start.column << ") ";

            // Token kind
            oss << std::setw(20) << std::left << token.to_string();

            // Token text (if not too long)
            if (!token.text.empty() && token.text.length() <= 30)
            {
                oss << " \"" << token.text << "\"";
            }
            else if (!token.text.empty())
            {
                oss << " \"" << token.text.substr(0, 27) << "...\"";
            }

            oss << "\n";
        }

        return oss.str();
    }

} // namespace Myre