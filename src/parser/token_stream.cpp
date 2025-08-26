#include "parser/token_stream.hpp"
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <iomanip>

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
            size_t back_offset = static_cast<size_t>(-offset);
            if (back_offset > position_)
            {
                return tokens_[0];
            }
            return tokens_[position_ - back_offset];
        }

        size_t target_pos = position_ + static_cast<size_t>(offset);
        if (target_pos >= tokens_.size())
        {
            return tokens_.back(); // Return EOF token
        }
        return tokens_[target_pos];
    }

    const Token &TokenStream::previous() const
    {
        if (position_ == 0)
        {
            return tokens_[0];
        }
        return tokens_[position_ - 1];
    }

    void TokenStream::advance()
    {
        if (!at_end())
        {
            position_++;
        }
    }

    bool TokenStream::at_end() const
    {
        return position_ >= tokens_.size() ||
               (position_ < tokens_.size() && tokens_[position_].kind == TokenKind::EndOfFile);
    }

    bool TokenStream::check(TokenKind kind) const
    {
        if (at_end())
            return false;
        return current().kind == kind;
    }

    bool TokenStream::check_any(std::initializer_list<TokenKind> kinds) const
    {
        if (at_end())
            return false;
        TokenKind current_kind = current().kind;
        return std::any_of(kinds.begin(), kinds.end(),
                           [current_kind](TokenKind k)
                           { return k == current_kind; });
    }

    bool TokenStream::check_sequence(std::initializer_list<TokenKind> sequence) const
    {
        size_t offset = 0;
        for (TokenKind kind : sequence)
        {
            if (peek(offset).kind != kind)
            {
                return false;
            }
            offset++;
        }
        return true;
    }

    bool TokenStream::consume(TokenKind kind)
    {
        if (check(kind))
        {
            advance();
            return true;
        }
        return false;
    }

    bool TokenStream::consume_any(std::initializer_list<TokenKind> kinds)
    {
        if (check_any(kinds))
        {
            advance();
            return true;
        }
        return false;
    }

    TokenKind TokenStream::consume_any_get(std::initializer_list<TokenKind> kinds)
    {
        if (at_end())
            return TokenKind::EndOfFile;

        TokenKind current_kind = current().kind;
        if (std::any_of(kinds.begin(), kinds.end(),
                        [current_kind](TokenKind k)
                        { return k == current_kind; }))
        {
            advance();
            return current_kind;
        }
        return TokenKind::EndOfFile;
    }

    void TokenStream::skip_to(TokenKind kind)
    {
        while (!at_end() && !check(kind))
        {
            advance();
        }
    }

    void TokenStream::skip_to_any(std::initializer_list<TokenKind> kinds)
    {
        while (!at_end() && !check_any(kinds))
        {
            advance();
        }
    }

    void TokenStream::skip_past(TokenKind kind)
    {
        skip_to(kind);
        if (check(kind))
        {
            advance();
        }
    }

    void TokenStream::splitRightShift()
    {
        ensure_valid_position();
        if (tokens_[position_].kind == TokenKind::RightShift)
        {
            // Create a new '>' token with same location as the '>>' token
            Token rightToken = tokens_[position_];
            rightToken.kind = TokenKind::Greater;
            rightToken.text = ">";
            
            // Replace the '>>' with the first '>'
            tokens_[position_].kind = TokenKind::Greater;
            tokens_[position_].text = ">";
            
            // Insert the second '>' right after the current position
            tokens_.insert(tokens_.begin() + position_ + 1, rightToken);
        }
    }

    SourceRange TokenStream::location() const
    {
        ensure_valid_position();
        return tokens_[position_].location;
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