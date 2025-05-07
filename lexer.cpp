#include "lexer.hpp"
#include <cctype>    // For isalpha, isalnum, isspace
#include <stdexcept> // For errors like unterminated strings
#include <iostream>

namespace Mycelium::UI::Lang
{

    Lexer::Lexer(std::string_view input) : m_input(input), m_position(0) {}

    char Lexer::peek() const
    {
        if (m_position >= m_input.length())
        {
            return '\0'; // End of input character
        }
        return m_input[m_position];
    }

    char Lexer::advance()
    {
        if (m_position >= m_input.length())
        {
            return '\0';
        }
        return m_input[m_position++];
    }

    void Lexer::skipWhitespaceAndComments()
    {
        while (m_position < m_input.length())
        {
            char current = peek();
            if (std::isspace(current))
            {
                advance(); // Consume whitespace
            }
            else if (current == '/' && m_position + 1 < m_input.length() && m_input[m_position + 1] == '/')
            {
                // skip two slashes (//)
                advance();
                advance();

                // skip to the end of the line
                while (peek() != '\n' && peek() != '\0')
                {
                    advance();
                }
                if (peek() == '\n')
                {
                    advance();
                }
            }
            else
            {
                break;
            }
        }
    }

    Token Lexer::readIdentifier()
    {
        size_t start = m_position;
        if (std::isalpha(peek()) || peek() == '_')
        {
            advance();
            while (std::isalnum(peek()) || peek() == '_')
            {
                advance();
            }
        }
        return {TokenType::IDENTIFIER, std::string(m_input.substr(start, m_position - start))};
    }

    Token Lexer::readStringLiteral()
    {
        size_t start = m_position + 1; // Start after the opening quote
        advance();                     // Consume opening '"'

        while (peek() != '"' && peek() != '\0')
        {
            advance();
        }

        if (peek() == '\0')
        {
            // \0 represents EOF, and if we reach EOF before finding a closing quote, we have an unterminated string literal
            std::cerr << "Warning: Unterminated string literal." << std::endl;
            return {TokenType::UNKNOWN, std::string(m_input.substr(start - 1, m_position - (start - 1)))};
        }
        
        // Consume closing '"'
        advance();

        // remove quotes
        std::string text = std::string(m_input.substr(start, m_position - start - 1));
        return {TokenType::STRING_LITERAL, text};
    }

    Token Lexer::readNumber()
    {
        size_t start = m_position;

        while (std::isdigit(peek()))
        {
            advance();
        }

        return {TokenType::NUMBER, std::string(m_input.substr(start, m_position - start))};
    }

    Token Lexer::getNextToken()
    {
        skipWhitespaceAndComments();

        if (m_position >= m_input.length())
        {
            return {TokenType::END_OF_FILE, ""};
        }

        char current_char = peek();

        if (std::isalpha(current_char) || current_char == '_')
        {
            return readIdentifier();
        }

        if (current_char == '"')
        {
            return readStringLiteral();
        }

        if (std::isdigit(current_char))
        {
            return readNumber();
        }

        // Single character tokens
        switch (current_char)
        {
            case '(': advance(); return {TokenType::LPAREN, "("};
            case ')': advance(); return {TokenType::RPAREN, ")"};
            case '{': advance(); return {TokenType::LBRACE, "{"};
            case '}': advance(); return {TokenType::RBRACE, "}"};
            case ':': advance(); return {TokenType::COLON, ":"};
            case ';': advance(); return {TokenType::SEMICOLON, ";"};
            case '%': advance(); return {TokenType::PERCENTAGE_SIGN, "%"};
            case ',': advance(); return {TokenType::COMMA, ","};
        }
        
        // everything should have matched by now, so if we reach here, it's an unknown token
        advance(); 
        return {TokenType::UNKNOWN, std::string(1, current_char)};
    }

    std::vector<Token> Lexer::tokenizeAll()
    {
        std::vector<Token> tokens;
        Token token;
        do
        {
            token = getNextToken();
            tokens.push_back(token);
            if (token.type == TokenType::UNKNOWN && token.text != "")
            {
                std::cerr << "Warning: Lexer encountered unknown token '" << token.text << "'" << std::endl;
            }
        } while (token.type != TokenType::END_OF_FILE);
        return tokens;
    }

}