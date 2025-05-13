#include "script_tokenizer.hpp"
#include <cctype> // For std::tolower, std::toupper

namespace Mycelium::Scripting::Lang
{

std::string token_type_to_string(TokenType type)
{
    switch (type)
    {
        case TokenType::Namespace: return "Namespace";
        case TokenType::Using: return "Using";
        case TokenType::Public: return "Public";
        case TokenType::Private: return "Private";
        case TokenType::Protected: return "Protected";
        case TokenType::Internal: return "Internal";
        case TokenType::Static: return "Static";
        case TokenType::Readonly: return "Readonly";
        case TokenType::Class: return "Class";
        case TokenType::Struct: return "Struct";
        case TokenType::Void: return "Void";
        case TokenType::Int: return "Int";
        case TokenType::Long: return "Long";
        case TokenType::Double: return "Double";
        case TokenType::Bool: return "Bool";
        case TokenType::Char: return "Char";
        case TokenType::String: return "String";
        case TokenType::If: return "If";
        case TokenType::Else: return "Else";
        case TokenType::While: return "While";
        case TokenType::For: return "For";
        case TokenType::ForEach: return "ForEach";
        case TokenType::In: return "In";
        case TokenType::Return: return "Return";
        case TokenType::Break: return "Break";
        case TokenType::Continue: return "Continue";
        case TokenType::New: return "New";
        case TokenType::This: return "This";
        case TokenType::Null: return "Null";
        case TokenType::True: return "True";
        case TokenType::False: return "False";
        case TokenType::Var: return "Var";
        case TokenType::Identifier: return "Identifier";
        case TokenType::IntegerLiteral: return "IntegerLiteral";
        case TokenType::DoubleLiteral: return "DoubleLiteral";
        case TokenType::StringLiteral: return "StringLiteral";
        case TokenType::CharLiteral: return "CharLiteral";
        case TokenType::OpenParen: return "OpenParen";
        case TokenType::CloseParen: return "CloseParen";
        case TokenType::OpenBrace: return "OpenBrace";
        case TokenType::CloseBrace: return "CloseBrace";
        case TokenType::OpenBracket: return "OpenBracket";
        case TokenType::CloseBracket: return "CloseBracket";
        case TokenType::Semicolon: return "Semicolon";
        case TokenType::Colon: return "Colon";
        case TokenType::Comma: return "Comma";
        case TokenType::Dot: return "Dot";
        case TokenType::QuestionMark: return "QuestionMark";
        case TokenType::Assign: return "Assign";
        case TokenType::Plus: return "Plus";
        case TokenType::Minus: return "Minus";
        case TokenType::Asterisk: return "Asterisk";
        case TokenType::Slash: return "Slash";
        case TokenType::Percent: return "Percent";
        case TokenType::PlusAssign: return "PlusAssign";
        case TokenType::MinusAssign: return "MinusAssign";
        case TokenType::AsteriskAssign: return "AsteriskAssign";
        case TokenType::SlashAssign: return "SlashAssign";
        case TokenType::PercentAssign: return "PercentAssign";
        case TokenType::Increment: return "Increment";
        case TokenType::Decrement: return "Decrement";
        case TokenType::LogicalAnd: return "LogicalAnd";
        case TokenType::LogicalOr: return "LogicalOr";
        case TokenType::LogicalNot: return "LogicalNot";
        case TokenType::EqualsEquals: return "EqualsEquals";
        case TokenType::NotEquals: return "NotEquals";
        case TokenType::LessThan: return "LessThan";
        case TokenType::GreaterThan: return "GreaterThan";
        case TokenType::LessThanOrEqual: return "LessThanOrEqual";
        case TokenType::GreaterThanOrEqual: return "GreaterThanOrEqual";
        case TokenType::At: return "At";
        case TokenType::EndOfFile: return "EndOfFile";
        case TokenType::Unknown: return "Unknown";
        default: return "UnhandledTokenType(" + std::to_string(static_cast<int>(type)) + ")";
    }
}

const std::unordered_map<std::string, TokenType> ScriptTokenizer::s_keywords =
{
    {"namespace", TokenType::Namespace}, {"using", TokenType::Using},
    {"public", TokenType::Public}, {"private", TokenType::Private},
    {"protected", TokenType::Protected}, {"internal", TokenType::Internal},
    {"static", TokenType::Static}, {"readonly", TokenType::Readonly},
    {"class", TokenType::Class}, {"struct", TokenType::Struct},
    {"void", TokenType::Void}, {"int", TokenType::Int}, {"long", TokenType::Long},
    {"double", TokenType::Double}, {"bool", TokenType::Bool},
    {"char", TokenType::Char}, {"string", TokenType::String},
    {"if", TokenType::If}, {"else", TokenType::Else},
    {"while", TokenType::While}, {"for", TokenType::For},
    {"foreach", TokenType::ForEach}, {"in", TokenType::In},
    {"return", TokenType::Return}, {"break", TokenType::Break}, {"continue", TokenType::Continue},
    {"new", TokenType::New}, {"this", TokenType::This},
    {"null", TokenType::Null}, {"true", TokenType::True}, {"false", TokenType::False},
    {"var", TokenType::Var}
};

ScriptTokenizer::ScriptTokenizer(const std::string& source)
    : m_source(source),
      m_currentIndex(0),
      m_currentLineStartOffset(0),
      m_currentLine(1),
      m_startOfLexeme(0),
      m_lexemeStartLine(1),
      m_lexemeStartColumn(1)
{
}

TokenizerError ScriptTokenizer::create_tokenizer_error(const std::string& message) const
{
    // Use m_lexemeStartLine/Column if error is about the current token being formed
    // Or m_currentLine/Column for errors at current peeking position
    return TokenizerError(message, m_lexemeStartLine, m_lexemeStartColumn);
}


std::vector<Token> ScriptTokenizer::tokenize_source()
{
    m_tokens.clear();
    m_currentIndex = 0;
    m_currentLine = 1;
    m_currentLineStartOffset = 0;

    while (!is_at_end())
    {
        skip_whitespace_and_comments();
        if (is_at_end())
        {
            break;
        }

        m_startOfLexeme = m_currentIndex;
        m_lexemeStartLine = m_currentLine;
        m_lexemeStartColumn = static_cast<int>(m_currentIndex - m_currentLineStartOffset) + 1;

        scan_single_token();
    }

    Token eofToken;
    eofToken.type = TokenType::EndOfFile;
    eofToken.lexeme = "";
    eofToken.location.line_start = m_currentLine;
    eofToken.location.column_start = static_cast<int>(m_currentIndex - m_currentLineStartOffset) + 1;
    eofToken.location.line_end = m_currentLine;
    eofToken.location.column_end = static_cast<int>(m_currentIndex - m_currentLineStartOffset);
    if (eofToken.location.column_end < 1 && eofToken.location.column_start ==1) eofToken.location.column_end = 0; // Ensure end <= start for 0-length
    else if (eofToken.location.column_end < 1) eofToken.location.column_end = 1;


    m_tokens.push_back(eofToken);

    return m_tokens;
}

bool ScriptTokenizer::is_at_end() const
{
    return m_currentIndex >= m_source.length();
}

char ScriptTokenizer::advance_char()
{
    char currentChar = m_source[m_currentIndex++];
    if (currentChar == '\n')
    {
        m_currentLine++;
        m_currentLineStartOffset = m_currentIndex;
    }
    return currentChar;
}

char ScriptTokenizer::peek_char() const
{
    if (is_at_end())
    {
        return '\0';
    }
    return m_source[m_currentIndex];
}

char ScriptTokenizer::peek_next_char() const
{
    if (m_currentIndex + 1 >= m_source.length())
    {
        return '\0';
    }
    return m_source[m_currentIndex + 1];
}

void ScriptTokenizer::add_token(TokenType type)
{
    std::string lexemeText = m_source.substr(m_startOfLexeme, m_currentIndex - m_startOfLexeme);
    add_token_internal(type, lexemeText);
}

void ScriptTokenizer::add_token_internal(TokenType type, const std::string& lexemeText)
{
    Token token;
    token.type = type;
    token.lexeme = lexemeText;

    token.location.line_start = m_lexemeStartLine;
    token.location.column_start = m_lexemeStartColumn;

    if (m_currentIndex > m_startOfLexeme)
    {
        size_t lastCharIndexInLexeme = m_currentIndex - 1;
        int endLine = m_lexemeStartLine;
        size_t endLineStartOffset = m_currentLineStartOffset; // Default to current line's start offset

        // Iterate through the lexeme to find internal newlines and update endLine
        for(size_t i = m_startOfLexeme; i < lastCharIndexInLexeme; ++i) {
            if (m_source[i] == '\n') {
                endLine++;
            }
        }
        token.location.line_end = endLine;

        // Find the start of the line where the lexeme ends
        size_t lastNewlineBeforeLexemeEnd = std::string::npos;
        if (endLine > m_lexemeStartLine) { // If lexeme spanned newlines
            lastNewlineBeforeLexemeEnd = m_source.rfind('\n', lastCharIndexInLexeme -1); // search before the last char
        } else { // Lexeme is on a single line
             lastNewlineBeforeLexemeEnd = m_source.rfind('\n', m_startOfLexeme > 0 ? m_startOfLexeme -1 : 0);
        }

        if (lastNewlineBeforeLexemeEnd == std::string::npos && m_startOfLexeme != 0) { // No newline before on the same line, implies first line part
             endLineStartOffset = 0;
        } else if (lastNewlineBeforeLexemeEnd != std::string::npos) {
            endLineStartOffset = lastNewlineBeforeLexemeEnd + 1;
        } else { // First line of file
            endLineStartOffset = 0;
        }
         token.location.column_end = static_cast<int>(lastCharIndexInLexeme - endLineStartOffset) + 1;

    }
    else // Zero-length token (should be rare, e.g. for EOF on empty source)
    {
        token.location.line_end = m_lexemeStartLine;
        token.location.column_end = m_lexemeStartColumn > 1 ? m_lexemeStartColumn -1 : 0;
    }
    m_tokens.push_back(token);
}


void ScriptTokenizer::skip_whitespace_and_comments()
{
    while (true)
    {
        if (is_at_end()) break;
        char c = peek_char();
        switch (c)
        {
            case ' ':
            case '\r':
            case '\t':
                advance_char();
                break;
            case '\n':
                advance_char();
                break;
            case '/':
                if (peek_next_char() == '/') // Line comment
                {
                    advance_char();
                    advance_char();
                    while (peek_char() != '\n' && !is_at_end())
                    {
                        advance_char();
                    }
                }
                else if (peek_next_char() == '*') // Block comment
                {
                    advance_char();
                    advance_char();
                    int commentStartLine = m_currentLine;
                    int commentStartCol = static_cast<int>(m_currentIndex - m_currentLineStartOffset) -1 ; // Approx start before '*'

                    bool foundEnd = false;
                    while (!is_at_end())
                    {
                        if (peek_char() == '*' && peek_next_char() == '/')
                        {
                            advance_char();
                            advance_char();
                            foundEnd = true;
                            break;
                        }
                        else
                        {
                            advance_char();
                        }
                    }
                    if (!foundEnd) {
                        throw TokenizerError("Unterminated block comment.", commentStartLine, commentStartCol);
                    }
                }
                else // Not a comment, just a slash
                {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

void ScriptTokenizer::scan_single_token()
{
    char c = advance_char();

    switch (c)
    {
        case '(': add_token(TokenType::OpenParen); break;
        case ')': add_token(TokenType::CloseParen); break;
        case '{': add_token(TokenType::OpenBrace); break;
        case '}': add_token(TokenType::CloseBrace); break;
        case '[': add_token(TokenType::OpenBracket); break;
        case ']': add_token(TokenType::CloseBracket); break;
        case ',': add_token(TokenType::Comma); break;
        case ';': add_token(TokenType::Semicolon); break;
        case ':': add_token(TokenType::Colon); break;
        case '?': add_token(TokenType::QuestionMark); break;
        case '.': add_token(TokenType::Dot); break;
        case '@': add_token(TokenType::At); break;

        case '+':
            if (peek_char() == '=') { advance_char(); add_token(TokenType::PlusAssign); }
            else if (peek_char() == '+') { advance_char(); add_token(TokenType::Increment); }
            else { add_token(TokenType::Plus); }
            break;
        case '-':
            if (peek_char() == '=') { advance_char(); add_token(TokenType::MinusAssign); }
            else if (peek_char() == '-') { advance_char(); add_token(TokenType::Decrement); }
            else { add_token(TokenType::Minus); }
            break;
        case '*':
            if (peek_char() == '=') { advance_char(); add_token(TokenType::AsteriskAssign); }
            else { add_token(TokenType::Asterisk); }
            break;
        case '/':
            if (peek_char() == '=') { advance_char(); add_token(TokenType::SlashAssign); }
            else { add_token(TokenType::Slash); }
            break;
        case '%':
            if (peek_char() == '=') { advance_char(); add_token(TokenType::PercentAssign); }
            else { add_token(TokenType::Percent); }
            break;
        case '=':
            if (peek_char() == '=') { advance_char(); add_token(TokenType::EqualsEquals); }
            else { add_token(TokenType::Assign); }
            break;
        case '!':
            if (peek_char() == '=') { advance_char(); add_token(TokenType::NotEquals); }
            else { add_token(TokenType::LogicalNot); }
            break;
        case '<':
            if (peek_char() == '=') { advance_char(); add_token(TokenType::LessThanOrEqual); }
            else { add_token(TokenType::LessThan); }
            break;
        case '>':
            if (peek_char() == '=') { advance_char(); add_token(TokenType::GreaterThanOrEqual); }
            else { add_token(TokenType::GreaterThan); }
            break;
        case '&':
            if (peek_char() == '&') { advance_char(); add_token(TokenType::LogicalAnd); }
            else { throw create_tokenizer_error("Unexpected character '&'. Expected '&&' for logical AND."); }
            break;
        case '|':
            if (peek_char() == '|') { advance_char(); add_token(TokenType::LogicalOr); }
            else { throw create_tokenizer_error("Unexpected character '|'. Expected '||' for logical OR."); }
            break;

        case '"': scan_string_literal(); break;
        case '\'': scan_char_literal(); break;

        default:
            if (is_digit(c))
            {
                m_currentIndex--; // Put 'c' back
                scan_number();
            }
            else if (is_alpha(c) || c == '_')
            {
                m_currentIndex--; // Put 'c' back
                scan_identifier_or_keyword();
            }
            else
            {
                throw create_tokenizer_error("Unexpected character '" + std::string(1, c) + "'.");
            }
            break;
    }
}

void ScriptTokenizer::scan_identifier_or_keyword()
{
    while (is_alpha_numeric(peek_char())) // is_alpha_numeric includes '_'
    {
        advance_char();
    }

    std::string text = m_source.substr(m_startOfLexeme, m_currentIndex - m_startOfLexeme);
    auto it = s_keywords.find(text);
    if (it != s_keywords.end())
    {
        add_token(it->second);
    }
    else
    {
        add_token(TokenType::Identifier);
    }
}

void ScriptTokenizer::scan_number()
{
    bool isDouble = false;
    while (is_digit(peek_char()))
    {
        advance_char();
    }

    if (peek_char() == '.' && is_digit(peek_next_char()))
    {
        isDouble = true;
        advance_char(); // Consume the '.'
        while (is_digit(peek_char()))
        {
            advance_char();
        }
    }

    if (std::tolower(static_cast<unsigned char>(peek_char())) == 'e')
    {
        if (m_currentIndex > m_startOfLexeme && (is_digit(m_source[m_currentIndex-1]) || m_source[m_currentIndex-1] == '.')) {
            isDouble = true;
            advance_char();
            if (peek_char() == '+' || peek_char() == '-')
            {
                advance_char();
            }
            if (!is_digit(peek_char()))
            {
                 throw create_tokenizer_error("Exponent lacks digits after 'e'.");
            }
            while (is_digit(peek_char()))
            {
                advance_char();
            }
        }
    }

    char suffix = static_cast<char>(std::toupper(static_cast<unsigned char>(peek_char())));
    if (suffix == 'L')
    {
        if (isDouble) { throw create_tokenizer_error("Suffix 'L' cannot be applied to a double literal."); }
        advance_char();
        add_token(TokenType::IntegerLiteral); // Or LongLiteral if distinct
    }
    else if (suffix == 'D' || suffix == 'F' || suffix == 'M')
    {
        isDouble = true;
        advance_char();
        add_token(TokenType::DoubleLiteral);
    }
    else if (isDouble)
    {
        add_token(TokenType::DoubleLiteral);
    }
    else
    {
        add_token(TokenType::IntegerLiteral);
    }
}

void ScriptTokenizer::scan_string_literal()
{
    while (peek_char() != '"' && !is_at_end())
    {
        if (peek_char() == '\\')
        {
            advance_char();
            if (!is_at_end()) advance_char(); // Consume escaped char
            else { throw create_tokenizer_error("Unterminated string literal due to escape at EOF."); }
        }
        else
        {
            advance_char();
        }
    }

    if (is_at_end())
    {
        throw create_tokenizer_error("Unterminated string literal.");
    }

    advance_char(); // Consume the closing quote '"'
    add_token(TokenType::StringLiteral);
}

void ScriptTokenizer::scan_char_literal()
{
    bool isValidCharContent = false;
    if (peek_char() == '\\')
    {
        advance_char(); // Consume '\'
        if (!is_at_end())
        {
            advance_char(); // Consume escaped char
            isValidCharContent = true;
        }
        else { throw create_tokenizer_error("Unterminated character literal with escape at EOF."); }
    }
    else if (peek_char() != '\'' && !is_at_end())
    {
        advance_char(); // Consume the char
        isValidCharContent = true;
    }

    if (isValidCharContent && peek_char() == '\'')
    {
        advance_char(); // Consume closing '\''
        add_token(TokenType::CharLiteral);
    }
    else
    {
        if (is_at_end()) { throw create_tokenizer_error("Unterminated character literal."); }
        if (peek_char() == '\'') { throw create_tokenizer_error("Empty character literal ''."); }
        throw create_tokenizer_error("Malformed character literal. Expected single character or valid escape sequence followed by '.'.");
    }
}

bool ScriptTokenizer::is_digit(char c) const
{
    return c >= '0' && c <= '9';
}

bool ScriptTokenizer::is_alpha(char c) const
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z');
}

bool ScriptTokenizer::is_alpha_numeric(char c) const
{
    return is_alpha(c) || is_digit(c) || c == '_';
}

} // namespace Mycelium::Scripting::Lang