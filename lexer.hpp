#pragma once

#include "token.hpp" // Needs Token and TokenType
#include <vector>
#include <string>
#include <string_view>

namespace Mycelium::UI::Lang {

class Lexer {
public:
    Lexer(std::string_view input);

    Token getNextToken();
    std::vector<Token> tokenizeAll(); // Keep this helper for now

private:
    void skipWhitespaceAndComments(); // Enhanced skipping
    Token readIdentifier();
    Token readStringLiteral();
    Token readNumber();
    char peek() const; // Helper to look at the next character without consuming
    char advance();   // Helper to consume the current character and move position

    std::string_view m_input;
    size_t m_position;
    // Add line/column tracking later if needed
    size_t m_line;
    size_t m_column_start;
};

} // namespace Mycelium::UI::Lang
