#include "script_tokenizer.hpp"
#include "script_ast.hpp"
#include <stdexcept>
#include <cctype>

namespace Mycelium::Scripting::Lang
{

// --- TokenType to String Helper (Implementation) ---
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
        case TokenType::EndOfFile: return "EndOfFile";
        case TokenType::Unknown: return "Unknown";
        default: return "UnhandledTokenType(" + std::to_string(static_cast<int>(type)) + ")";
    }
}


// --- ScriptTokenizer Implementation ---

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
    eofToken.lexeme = ""; // Empty lexeme for EOF
    eofToken.location.line_start = m_currentLine;
    eofToken.location.column_start = static_cast<int>(m_currentIndex - m_currentLineStartOffset) + 1;
    eofToken.location.line_end = m_currentLine;
    eofToken.location.column_end = static_cast<int>(m_currentIndex - m_currentLineStartOffset); // End before start for 0-length
    if (eofToken.location.column_end < 1) eofToken.location.column_end = 1; // Ensure positive column
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

    if (m_currentIndex > m_startOfLexeme) // If token has length
    {
        size_t lastCharIndex = m_currentIndex - 1;
        if (m_source[lastCharIndex] == '\n')
        {
            // Token ended with a newline. m_currentLine is already the *next* line.
            token.location.line_end = m_currentLine - 1;
            // Column of the newline char itself. Find start of the line it was on.
            size_t startOfThisNewlineLine = m_source.rfind('\n', lastCharIndex - 1);
            if (startOfThisNewlineLine == std::string::npos) // Newline was on the first line of file
            {
                startOfThisNewlineLine = 0;
            }
            else
            {
                startOfThisNewlineLine += 1; // Move past the previous newline
            }
            token.location.column_end = static_cast<int>(lastCharIndex - startOfThisNewlineLine) + 1;
        }
        else
        {
            // Token did not end with a newline. Last char is on m_currentLine.
            // m_currentLineStartOffset is the start of m_currentLine.
            token.location.line_end = m_currentLine;
            token.location.column_end = static_cast<int>(lastCharIndex - m_currentLineStartOffset) + 1;
        }
    }
    else // Zero-length token (e.g., an error placeholder, or EOF if source is empty)
    {
        token.location.line_end = m_lexemeStartLine;
        token.location.column_end = m_lexemeStartColumn > 1 ? m_lexemeStartColumn -1 : 1; // Ends "before" it starts
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
                advance_char(); // Handles line counting and offset update
                break;
            case '/':
                if (peek_next_char() == '/') // Line comment
                {
                    advance_char(); // Consume '/'
                    advance_char(); // Consume '/'
                    while (peek_char() != '\n' && !is_at_end())
                    {
                        advance_char();
                    }
                    // The newline (or EOF) will be handled by the next iteration or by is_at_end()
                }
                else if (peek_next_char() == '*') // Block comment
                {
                    advance_char(); // Consume '/'
                    advance_char(); // Consume '*'
                    int commentStartLine = m_currentLine; // For error reporting
                    int commentStartCol = static_cast<int>(m_currentIndex - m_currentLineStartOffset) + 1 - 2; // Approx start

                    while (!is_at_end())
                    {
                        if (peek_char() == '*' && peek_next_char() == '/')
                        {
                            advance_char(); // Consume '*'
                            advance_char(); // Consume '/'
                            goto next_iteration_skip_whitespace; // Break out of inner while and continue outer while
                        }
                        else
                        {
                            advance_char(); // Consumes char inside comment, advance_char handles newlines
                        }
                    }
                    // If loop finishes, it's an unterminated block comment
                    throw std::runtime_error("Unterminated block comment starting near line " +
                                            std::to_string(commentStartLine) + ", col " + std::to_string(commentStartCol));
                }
                else // Not a comment, just a slash (division operator)
                {
                    return; // Done skipping
                }
                break;
            default:
                return; // Not whitespace or comment start, done skipping
        }
        next_iteration_skip_whitespace:;
    }
}

void ScriptTokenizer::scan_single_token()
{
    char c = advance_char(); // Consume the character to decide what token it is

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
        case '/': // Comments handled by skip_whitespace_and_comments
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
            else { add_token(TokenType::Unknown); /* Or bitwise & if supported */ }
            break;
        case '|':
            if (peek_char() == '|') { advance_char(); add_token(TokenType::LogicalOr); }
            else { add_token(TokenType::Unknown); /* Or bitwise | if supported */ }
            break;

        case '"': scan_string_literal(); break;
        case '\'': scan_char_literal(); break;

        default:
            if (is_digit(c))
            {
                m_currentIndex--; // Put 'c' back to be read by scan_number
                if (c == '\n') { /* This case should not happen if is_digit(c) is true */ }
                scan_number();
            }
            else if (is_alpha(c) || c == '_') // Identifiers (or keywords)
            {
                m_currentIndex--; // Put 'c' back
                if (c == '\n') { /* Should not happen if is_alpha(c) is true */ }
                scan_identifier_or_keyword();
            }
            else
            {
                add_token(TokenType::Unknown); // Unknown character
                // Optionally throw:
                // throw std::runtime_error("Unexpected character '" + std::string(1, c) +
                //                          "' at line " + std::to_string(m_lexemeStartLine) +
                //                          ", col " + std::to_string(m_lexemeStartColumn));
            }
            break;
    }
}

void ScriptTokenizer::scan_identifier_or_keyword()
{
    // m_startOfLexeme, m_lexemeStartLine, m_lexemeStartColumn are already set.
    // The first char of identifier is at m_currentIndex (it was put back).
    while (is_alpha_numeric(peek_char()) || peek_char() == '_')
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
    // m_startOfLexeme, m_lexemeStartLine, m_lexemeStartColumn are set.
    // The first digit is at m_currentIndex (it was put back).
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
    
    if (std::tolower(peek_char()) == 'e')
    {
        // Check if previous char was a digit or '.', to avoid 'e' in identifiers being numbers
        if (m_currentIndex > m_startOfLexeme && (is_digit(m_source[m_currentIndex-1]) || m_source[m_currentIndex-1] == '.')) {
            isDouble = true;
            advance_char(); // Consume 'e' or 'E'
            if (peek_char() == '+' || peek_char() == '-')
            {
                advance_char(); // Consume sign
            }
            if (!is_digit(peek_char()))
            {
                 // Error: exponent lacks digits. Tokenize what we have, parser might complain.
                 // Or, if we want to be strict, this is a malformed number.
                 // For now, we let it pass; the lexeme will include 'e[sign]'
            }
            while (is_digit(peek_char()))
            {
                advance_char();
            }
        }
    }

    char suffix = static_cast<char>(std::toupper(static_cast<unsigned char>(peek_char())));
    if (suffix == 'L') // Long integer
    {
        if (isDouble) { /* Error: 'L' suffix on a double. For now, ignore. Parser can validate. */ }
        else { advance_char(); } // Consume 'L'
    }
    else if (suffix == 'D' || suffix == 'F' || suffix == 'M') // Double, Float, Decimal
    {
        isDouble = true;
        advance_char(); // Consume suffix
    }

    if (isDouble)
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
    // Opening quote was consumed by scan_single_token, m_startOfLexeme points to it.
    // m_currentIndex is after the opening quote.
    while (peek_char() != '"' && !is_at_end())
    {
        if (peek_char() == '\\') // Escape sequence
        {
            advance_char(); // Consume '\'
            if (!is_at_end())
            {
                advance_char(); // Consume the escaped character (e.g., 'n', 't', '"', '\')
            }
            else // Unterminated escape at EOF
            {
                // Error: unterminated string due to escape at EOF
                // Let the outer is_at_end() check handle unterminated string.
                break;
            }
        }
        else
        {
            advance_char(); // Consumes char inside string
        }
    }

    if (is_at_end()) // Unterminated string
    {
        // Add an error token or throw
        add_token(TokenType::Unknown); // Lexeme will be from opening quote to EOF
        // throw std::runtime_error("Unterminated string literal starting at line " +
        //                          std::to_string(m_lexemeStartLine) + ", col " + std::to_string(m_lexemeStartColumn));
        return;
    }

    advance_char(); // Consume the closing quote '"'
    add_token(TokenType::StringLiteral); // Lexeme includes quotes
}

void ScriptTokenizer::scan_char_literal()
{
    // Opening quote was consumed by scan_single_token, m_startOfLexeme points to it.
    // m_currentIndex is after the opening quote.
    bool isValidChar = false;
    if (peek_char() == '\\') // Escape sequence
    {
        advance_char(); // Consume '\'
        if (!is_at_end())
        {
            advance_char(); // Consume escaped char
            isValidChar = true;
        }
        // else: unterminated escape, will be caught by peek_char() != '\'' check
    }
    else if (peek_char() != '\'' && !is_at_end()) // Regular character
    {
        advance_char(); // Consume the char
        isValidChar = true;
    }
    // else: empty char '' or unterminated like ' at EOF. isValidChar remains false.

    if (isValidChar && peek_char() == '\'')
    {
        advance_char(); // Consume closing '\''
        add_token(TokenType::CharLiteral);
    }
    else
    {
        // Malformed char literal (e.g. 'ab', '', 'a at EOF, unterminated escape)
        // Try to consume until the next quote or sensible delimiter to allow parsing to continue.
        while (!is_at_end() && peek_char() != '\'' && peek_char() != '\n')
        {
            advance_char();
        }
        if (peek_char() == '\'') advance_char(); // Consume if found

        add_token(TokenType::Unknown); // The lexeme will contain the malformed attempt
        // throw std::runtime_error("Malformed character literal starting at line " +
        //                          std::to_string(m_lexemeStartLine) + ", col " + std::to_string(m_lexemeStartColumn));
    }
}

// Character check helpers (using cctype for locale-awareness if needed, but basic checks are fine)
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
    return is_alpha(c) || is_digit(c) || c == '_'; // Allowing underscore in alphanumeric for identifiers
}


}
