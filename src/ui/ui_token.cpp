#include "ui_token.hpp"

namespace Mycelium::UI::Lang
{

    std::string token_type_to_string(TokenType type)
    {
        switch (type)
        {
            case TokenType::LPAREN: return "LPAREN";
            case TokenType::RPAREN: return "RPAREN";
            case TokenType::LBRACE: return "LBRACE";
            case TokenType::RBRACE: return "RBRACE";
            case TokenType::COLON: return "COLON";
            case TokenType::SEMICOLON: return "SEMICOLON";
            case TokenType::PERCENTAGE_SIGN: return "PERCENTAGE_SIGN";
            case TokenType::COMMA: return "COMMA";
            case TokenType::NUMBER: return "NUMBER";
            case TokenType::IDENTIFIER: return "IDENTIFIER";
            case TokenType::STRING_LITERAL: return "STRING_LITERAL";
            case TokenType::END_OF_FILE: return "END_OF_FILE";
            default: return "UNKNOWN";
        }
    }

}