#pragma once

#include <string>
#include <string_view>

namespace Mycelium::UI::Lang
{

    enum class TokenType
    {
        IDENTIFIER,
        LPAREN,
        RPAREN,
        LBRACE,
        RBRACE,
        COLON,
        SEMICOLON,
        COMMA,
        PERCENTAGE_SIGN,
        NUMBER,
        STRING_LITERAL,
        END_OF_FILE,
        UNKNOWN
    };

    std::string tokenTypeToString(TokenType type);

    struct Token
    {
        TokenType type;
        std::string text; 
        size_t line;
        size_t column;
    };

}