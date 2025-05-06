#pragma once

#include <string>
#include <string_view> // Keep using string_view where efficient

namespace Mycelium::UI::Lang {

enum class TokenType {
    IDENTIFIER,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    COLON,
    SEMICOLON,
    NUMBER,
    PERCENTAGE_SIGN,
    STRING_LITERAL,
    END_OF_FILE,
    UNKNOWN
};

// Helper function declaration (implementation can be in a .cpp or stay header-only)
std::string tokenTypeToString(TokenType type);


struct Token {
    TokenType type;
    std::string text; // The actual text matched
    size_t line;
    size_t column;
};

} // namespace Mycelium::UI::Lang