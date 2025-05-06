#include "lexer.hpp"
#include <cctype> // For isalpha, isalnum, isspace
#include <stdexcept> // For errors like unterminated strings
#include <iostream>

namespace Mycelium::UI::Lang {

Lexer::Lexer(std::string_view input) : m_input(input), m_position(0) {}

char Lexer::peek() const {
    if (m_position >= m_input.length()) {
        return '\0'; // End of input character
    }
    return m_input[m_position];
}

char Lexer::advance() {
    if (m_position >= m_input.length()) {
        return '\0';
    }
    return m_input[m_position++];
}


void Lexer::skipWhitespaceAndComments() {
    while (m_position < m_input.length()) {
        char current = peek();
        if (std::isspace(current)) {
            advance(); // Consume whitespace
        } else if (current == '/' && m_position + 1 < m_input.length() && m_input[m_position + 1] == '/') {
            // Single-line comment
            advance(); // Consume '/'
            advance(); // Consume '/'
            while (peek() != '\n' && peek() != '\0') {
                advance(); // Consume until newline or EOF
            }
             if (peek() == '\n') {
                 advance(); // Consume the newline itself
             }
        }
         // Add block comments /* ... */ here if needed later
        else {
            break; // Not whitespace or comment start
        }
    }
}

Token Lexer::readIdentifier() {
     size_t start = m_position;
     // Allow underscore in identifiers after the first char
     if (std::isalpha(peek()) || peek() == '_') { // Start with letter or underscore
         advance();
         while (std::isalnum(peek()) || peek() == '_') {
             advance();
         }
     }
     return {TokenType::IDENTIFIER, std::string(m_input.substr(start, m_position - start))};
}

Token Lexer::readStringLiteral() {
    size_t start = m_position + 1; // Start after the opening quote
    advance(); // Consume opening '"'

    while (peek() != '"' && peek() != '\0') {
        // Add escape sequence handling later (e.g., \\, \") if needed
        advance();
    }

    if (peek() == '\0') {
        // Unterminated string - decide how to handle (error token? throw?)
        // Returning UNKNOWN for now
         std::cerr << "Warning: Unterminated string literal." << std::endl;
         return {TokenType::UNKNOWN, std::string(m_input.substr(start -1 , m_position - (start - 1)))}; // Include quote for context
    }

    advance(); // Consume closing '"'
    // Extract the content *without* the quotes
    std::string text = std::string(m_input.substr(start, m_position - start - 1));
    return {TokenType::STRING_LITERAL, text};
}

Token Lexer::readNumber() {
    size_t start = m_position;
    bool isPercentage = false;

    while (std::isdigit(peek())) {
        advance(); // Consume digits
    }

    return {TokenType::NUMBER, std::string(m_input.substr(start, m_position - start))};
}


Token Lexer::getNextToken() {
    skipWhitespaceAndComments();

    if (m_position >= m_input.length()) {
        return {TokenType::END_OF_FILE, ""};
    }

    char current_char = peek(); // Use peek now

    // Identifiers (start with letter or _, contain letter, number, _)
    if (std::isalpha(current_char) || current_char == '_') {
        return readIdentifier();
    }

    // String Literal
    if (current_char == '"') {
        return readStringLiteral();
    }

    if (std::isdigit(current_char)) {
        return readNumber();
    }

    // Single character tokens
    switch (current_char) {
        case '(': advance(); return {TokenType::LPAREN, "("};
        case ')': advance(); return {TokenType::RPAREN, ")"};
        case '{': advance(); return {TokenType::LBRACE, "{"};
        case '}': advance(); return {TokenType::RBRACE, "}"};
        case ':': advance(); return {TokenType::COLON, ":"};
        case ';': advance(); return {TokenType::SEMICOLON, ";"};
        case '%': advance(); return {TokenType::PERCENTAGE_SIGN, "%"};
    }


    // If nothing matched
    advance(); // Consume the unknown character
    return {TokenType::UNKNOWN, std::string(1, current_char)};
}

std::vector<Token> Lexer::tokenizeAll() {
    std::vector<Token> tokens;
    Token token;
    do {
        token = getNextToken();
        tokens.push_back(token);
        if(token.type == TokenType::UNKNOWN && token.text != "") { // Don't warn for consumed unknown chars if text is empty? Check logic.
             std::cerr << "Warning: Lexer encountered unknown token '" << token.text << "'" << std::endl;
        }
    } while (token.type != TokenType::END_OF_FILE);
    return tokens;
}


} // namespace Mycelium::UI::Lang