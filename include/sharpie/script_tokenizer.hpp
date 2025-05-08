#pragma once

#include "script_token.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace Mycelium::Scripting::Lang
{

class ScriptTokenizer
{
public:
    ScriptTokenizer(const std::string& source);
    std::vector<Token> tokenize_source();

private:
    const std::string& m_source;
    std::vector<Token> m_tokens;

    size_t m_currentIndex;          // Current character offset in m_source
    size_t m_currentLineStartOffset; // Offset of the beginning of the current line in m_source
    int m_currentLine;              // Current line number (1-based)

    // For tracking the start of the current lexeme
    size_t m_startOfLexeme;
    int m_lexemeStartLine;
    int m_lexemeStartColumn;

    static const std::unordered_map<std::string, TokenType> s_keywords;

    // Character inspection and consumption
    char advance_char();
    char peek_char() const;
    char peek_next_char() const;
    bool is_at_end() const;

    // Token creation
    void add_token(TokenType type);
    // This overload is if the lexeme text is different from source substring (e.g. unescaped string)
    // For now, Token::lexeme will be the raw source text.
    // void add_token_with_custom_lexeme(TokenType type, const std::string& customLexeme);
    void add_token_internal(TokenType type, const std::string& lexemeText);


    // Main scanning driver and whitespace/comment skipper
    void skip_whitespace_and_comments();
    void scan_single_token();

    // Specific token type scanners
    void scan_identifier_or_keyword();
    void scan_number();
    void scan_string_literal();
    void scan_char_literal();

    // Character classification helpers
    bool is_digit(char c) const;
    bool is_alpha(char c) const;
    bool is_alpha_numeric(char c) const;
};

}
