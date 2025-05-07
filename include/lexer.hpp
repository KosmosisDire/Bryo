#pragma once

#include "token.hpp"
#include <vector>
#include <string>
#include <string_view>

namespace Mycelium::UI::Lang
{

    class Lexer
    {
    public:
        Lexer(std::string_view input);

        Token getNextToken();
        std::vector<Token> tokenizeAll();

    private:
        void skipWhitespaceAndComments();
        Token readIdentifier();
        Token readStringLiteral();
        Token readNumber();
        char peek() const;
        char advance();

        std::string_view m_input;
        size_t m_position;
        size_t m_line;
        size_t m_column_start;
    };

}
