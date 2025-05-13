#pragma once

#include "script_token.hpp"
#include "script_parser.hpp" // For ParseError (or a new TokenizerError)
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept> // For std::runtime_error

namespace Mycelium::Scripting::Lang
{

class TokenizerError : public std::runtime_error
{
public:
    TokenizerError(const std::string& message, int line, int column)
        : std::runtime_error(format_message(message, line, column)), m_line(line), m_column(column)
    {
    }

    int get_line() const { return m_line; }
    int get_column() const { return m_column; }

private:
    int m_line;
    int m_column;

    static std::string format_message(const std::string& baseMessage, int line, int col)
    {
        return "Tokenizer Error (Line " + std::to_string(line) +
               ", Col " + std::to_string(col) + "): " + baseMessage;
    }
};


class ScriptTokenizer
{
public:
    ScriptTokenizer(const std::string& source);
    std::vector<Token> tokenize_source();

private:
    const std::string& m_source;
    std::vector<Token> m_tokens;

    size_t m_currentIndex;
    size_t m_currentLineStartOffset;
    int m_currentLine;

    size_t m_startOfLexeme;
    int m_lexemeStartLine;
    int m_lexemeStartColumn;

    static const std::unordered_map<std::string, TokenType> s_keywords;

    char advance_char();
    char peek_char() const;
    char peek_next_char() const;
    bool is_at_end() const;

    void add_token(TokenType type);
    void add_token_internal(TokenType type, const std::string& lexemeText);

    void skip_whitespace_and_comments();
    void scan_single_token();

    void scan_identifier_or_keyword();
    void scan_number();
    void scan_string_literal();
    void scan_char_literal();

    bool is_digit(char c) const;
    bool is_alpha(char c) const;
    bool is_alpha_numeric(char c) const;

    TokenizerError create_tokenizer_error(const std::string& message) const;
};

} // namespace Mycelium::Scripting::Lang