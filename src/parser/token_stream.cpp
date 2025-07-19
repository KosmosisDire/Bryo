#include "parser/token_stream.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <iostream>

namespace Mycelium::Scripting::Lang {

const Token& TokenStream::current() const {
    ensure_valid_position();
    return tokens_[position_];
}

const Token& TokenStream::peek(int offset) const {
    if (offset < 0) {
        // Handle negative offsets (looking backward)
        if (static_cast<size_t>(-offset) > position_) {
            // Would go before the beginning, return first token
            return tokens_[0];
        }
        return tokens_[position_ - static_cast<size_t>(-offset)];
    }
    
    size_t target_pos = position_ + static_cast<size_t>(offset);
    if (target_pos >= tokens_.size()) {
        // Return last token (should be EOF) if position is beyond available tokens
        return tokens_.back();
    }
    
    return tokens_[target_pos];
}

void TokenStream::advance() {
    if (!at_end()) {
        position_++;
    }
}

bool TokenStream::match(TokenKind kind) {
    if (current().kind == kind) {
        advance();
        return true;
    }
    return false;
}

bool TokenStream::match_any(std::initializer_list<TokenKind> kinds) {
    for (TokenKind kind : kinds) {
        if (current().kind == kind) {
            advance();
            return true;
        }
    }
    return false;
}

Token TokenStream::consume(TokenKind expected) {
    const Token& token = current();
    if (token.kind == expected) {
        advance();
        return token;
    }
    
    // In a real implementation, this would report through a diagnostic system
    throw std::runtime_error(get_expected_message(expected));
}

Token TokenStream::consume_any(std::initializer_list<TokenKind> kinds) {
    const Token& token = current();
    for (TokenKind kind : kinds) {
        if (token.kind == kind) {
            advance();
            return token;
        }
    }
    
    // In a real implementation, this would report through a diagnostic system
    throw std::runtime_error(get_expected_message(kinds));
}

bool TokenStream::at_end() const {

    return position_ >= tokens_.size() ||
           (position_ < tokens_.size() && tokens_[position_].kind == TokenKind::EndOfFile);
}

SourceLocation TokenStream::location() const {
    ensure_valid_position();
    return tokens_[position_].location;
}

bool TokenStream::check(TokenKind kind) const {
    return current().kind == kind;
}

bool TokenStream::check_any(std::initializer_list<TokenKind> kinds) const {
    TokenKind current_kind = current().kind;
    for (TokenKind kind : kinds) {
        if (current_kind == kind) {
            return true;
        }
    }
    return false;
}

bool TokenStream::match_sequence(std::initializer_list<TokenKind> sequence) const {
    size_t offset = 0;
    for (TokenKind kind : sequence) {
        if (peek(offset).kind != kind) {
            return false;
        }
        offset++;
    }
    return true;
}

std::string TokenStream::get_expected_message(TokenKind expected) const {
    std::ostringstream oss;
    oss << "Expected " << Mycelium::Scripting::to_string(expected) 
        << " but found " << Mycelium::Scripting::to_string(current().kind);
    return oss.str();
}

std::string TokenStream::get_expected_message(std::initializer_list<TokenKind> expected) const {
    std::ostringstream oss;
    oss << "Expected ";
    
    if (expected.size() == 1) {
        oss << Mycelium::Scripting::to_string(*expected.begin());
    } else {
        bool first = true;
        for (TokenKind kind : expected) {
            if (!first) {
                oss << ", ";
            }
            oss << Mycelium::Scripting::to_string(kind);
            first = false;
        }
    }
    
    oss << " but found " << Mycelium::Scripting::to_string(current().kind);
    return oss.str();
}

void TokenStream::ensure_valid_position() const {
    if (tokens_.empty()) {
        throw std::runtime_error("TokenStream is empty");
    }
    if (position_ >= tokens_.size()) {
        throw std::runtime_error("TokenStream position out of bounds");
    }
}

std::string TokenStream::to_string() const {
    std::ostringstream oss;
    oss << "TokenStream (" << tokens_.size() << " tokens, position=" << position_ << "):\n";
    
    for (size_t i = 0; i < tokens_.size(); ++i) {
        const Token& token = tokens_[i];
        
        // Mark current position with an arrow
        if (i == position_) {
            oss << " --> ";
        } else {
            oss << "     ";
        }
        
        // Token index
        oss << "[" << std::setw(3) << i << "] ";
        
        // Token location
        oss << "(" << std::setw(4) << token.location.line 
            << ":" << std::setw(3) << token.location.column << ") ";
        
        // Token kind
        oss << std::setw(20) << std::left << token.to_string();
        
        // Token text (if not too long)
        if (!token.text.empty() && token.text.length() <= 30) {
            oss << " \"" << token.text << "\"";
        } else if (!token.text.empty()) {
            oss << " \"" << token.text.substr(0, 27) << "...\"";
        }
        
        oss << "\n";
    }
    
    return oss.str();
}

} // namespace Mycelium::Scripting::Lang