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

    std::string token_type_to_string(TokenType type);

    struct Token
    {
        TokenType type;
        std::string text; 
        size_t line;
        size_t column;
    };

}