#include "sharpie/parser/script_parser.hpp"
#include "sharpie/script_ast.hpp" // For AST nodes like TokenNode, SourceLocation etc.
#include <cctype>                 // For std::isalpha, std::isdigit, std::isalnum
#include <iostream>               // For std::cout in advance_and_lex debug

namespace Mycelium::Scripting::Lang
{

    // Lexing methods of ScriptParser

    CurrentTokenInfo ScriptParser::lex_number_literal()
    {
        CurrentTokenInfo token_info;
        token_info.location.fileName = std::string(fileName);
        token_info.location.lineStart = currentLine;
        token_info.location.columnStart = currentColumn;

        size_t token_start_offset = currentCharOffset;
        bool is_floating_point = false;
        bool has_exponent = false;

        while (!is_at_end_of_source() && std::isdigit(static_cast<unsigned char>(peek_char())))
        {
            consume_char();
        }

        if (!is_at_end_of_source() && peek_char() == '.')
        {
            if (std::isdigit(static_cast<unsigned char>(peek_char(1))))
            {
                is_floating_point = true;
                consume_char();
                while (!is_at_end_of_source() && std::isdigit(static_cast<unsigned char>(peek_char())))
                {
                    consume_char();
                }
            }
        }

        if (!is_at_end_of_source() && (peek_char() == 'e' || peek_char() == 'E'))
        {
            is_floating_point = true;
            has_exponent = true;
            consume_char();

            if (!is_at_end_of_source() && (peek_char() == '+' || peek_char() == '-'))
            {
                consume_char();
            }

            if (is_at_end_of_source() || !std::isdigit(static_cast<unsigned char>(peek_char())))
            {
                SourceLocation error_loc = {currentLine, currentLine, currentColumn, currentColumn};
                error_loc.fileName = std::string(fileName);
                record_error("Exponent in number literal lacks digits.", error_loc);
            }
            else
            {
                while (!is_at_end_of_source() && std::isdigit(static_cast<unsigned char>(peek_char())))
                {
                    consume_char();
                }
            }
        }

        token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
        token_info.type = TokenType::IntegerLiteral;

        char suffix = '\0';
        if (!is_at_end_of_source())
        {
            suffix = peek_char();
            if (suffix == 'L' || suffix == 'l')
            {
                if (is_floating_point)
                {
                    record_error("Suffix 'L'/'l' cannot be applied to a floating-point literal.", token_info.location);
                    token_info.type = TokenType::Error;
                }
                else
                {
                    token_info.type = TokenType::LongLiteral;
                    consume_char();
                    token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
                }
            }
            else if (suffix == 'F' || suffix == 'f')
            {
                token_info.type = TokenType::FloatLiteral;
                is_floating_point = true;
                consume_char();
                token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
            }
            else if (suffix == 'D' || suffix == 'd')
            {
                token_info.type = TokenType::DoubleLiteral;
                is_floating_point = true;
                consume_char();
                token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
            }
        }

        token_info.location.lineEnd = currentLine;
        token_info.location.columnEnd = currentColumn - 1;

        if (token_info.type != TokenType::Error)
        {
            if (is_floating_point || token_info.type == TokenType::FloatLiteral || token_info.type == TokenType::DoubleLiteral)
            {
                if (token_info.type == TokenType::IntegerLiteral)
                {
                    token_info.type = TokenType::DoubleLiteral;
                }
                try
                {
                    std::string value_str = std::string(token_info.lexeme);
                    if (token_info.type == TokenType::FloatLiteral)
                    {
                        token_info.literalValue = std::stof(value_str);
                    }
                    else
                    {
                        token_info.literalValue = std::stod(value_str);
                    }
                }
                catch (const std::out_of_range &)
                {
                    record_error("Floating point literal out of range: " + std::string(token_info.lexeme), token_info.location);
                    token_info.type = TokenType::Error;
                    token_info.literalValue = 0.0;
                }
                catch (const std::invalid_argument &)
                {
                    record_error("Invalid floating point literal format: " + std::string(token_info.lexeme), token_info.location);
                    token_info.type = TokenType::Error;
                    token_info.literalValue = 0.0;
                }
            }
            else
            {
                try
                {
                    if (token_info.type == TokenType::LongLiteral)
                    {
                        token_info.literalValue = std::stoll(std::string(token_info.lexeme));
                    }
                    else
                    {
                        token_info.literalValue = std::stoll(std::string(token_info.lexeme));
                    }
                }
                catch (const std::out_of_range &)
                {
                    record_error("Integer/Long literal out of range: " + std::string(token_info.lexeme), token_info.location);
                    token_info.type = TokenType::Error;
                    token_info.literalValue = static_cast<int64_t>(0);
                }
                catch (const std::invalid_argument &)
                {
                    record_error("Invalid integer/long literal format: " + std::string(token_info.lexeme), token_info.location);
                    token_info.type = TokenType::Error;
                    token_info.literalValue = static_cast<int64_t>(0);
                }
            }
        }
        return token_info;
    }

    CurrentTokenInfo ScriptParser::lex_string_literal()
    {
        CurrentTokenInfo token_info;
        token_info.location.fileName = std::string(fileName);
        token_info.location.lineStart = currentLine;
        token_info.location.columnStart = currentColumn;
        token_info.type = TokenType::StringLiteral;

        size_t token_start_offset = currentCharOffset;
        consume_char();

        std::string string_value;
        bool properly_terminated = false;

        while (!is_at_end_of_source())
        {
            char ch = peek_char();
            if (ch == '"')
            {
                consume_char();
                properly_terminated = true;
                break;
            }
            if (ch == '\\')
            {
                consume_char();
                if (is_at_end_of_source())
                {
                    record_error("String literal has unterminated escape sequence at end of file", token_info.location);
                    break;
                }
                char escaped_char = consume_char();
                switch (escaped_char)
                {
                case 'n':
                    string_value += '\n';
                    break;
                case 't':
                    string_value += '\t';
                    break;
                case 'r':
                    string_value += '\r';
                    break;
                case '\\':
                    string_value += '\\';
                    break;
                case '"':
                    string_value += '"';
                    break;
                default:
                    string_value += '\\';
                    string_value += escaped_char;
                    SourceLocation err_loc = {currentLine, currentLine, currentColumn - 2, currentColumn - 1};
                    err_loc.fileName = std::string(fileName);
                    record_error("Unknown escape sequence '\\" + std::string(1, escaped_char) + "' in string literal", err_loc);
                    break;
                }
            }
            else if (ch == '\n' || ch == '\r')
            {
                record_error("Newline in string literal. Use verbatim strings (@\"...\") or escape sequences.", token_info.location);
                properly_terminated = false;
                break;
            }
            else
            {
                string_value += consume_char();
            }
        }

        if (!properly_terminated)
        {
            record_error("Unterminated string literal", token_info.location);
            token_info.type = TokenType::Error;
        }

        token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
        token_info.location.lineEnd = currentLine;
        token_info.location.columnEnd = currentColumn - 1;
        token_info.literalValue = string_value;

        return token_info;
    }

    CurrentTokenInfo ScriptParser::lex_char_literal()
    {
        CurrentTokenInfo token_info;
        token_info.location.fileName = std::string(fileName);
        token_info.location.lineStart = currentLine;
        token_info.location.columnStart = currentColumn;
        token_info.type = TokenType::CharLiteral;

        size_t token_start_offset = currentCharOffset;
        char char_value = '\0';
        int char_count = 0;

        consume_char();

        if (is_at_end_of_source() || peek_char() == '\'')
        {
            record_error("Empty character literal", token_info.location);
            token_info.type = TokenType::Error;
            if (!is_at_end_of_source() && peek_char() == '\'')
                consume_char();
        }
        else
        {
            char ch = peek_char();
            if (ch == '\\')
            {
                consume_char();
                if (is_at_end_of_source())
                {
                    record_error("Character literal has unterminated escape sequence at end of file", token_info.location);
                    token_info.type = TokenType::Error;
                }
                else
                {
                    char escaped_char = consume_char();
                    switch (escaped_char)
                    {
                    case 'n':
                        char_value = '\n';
                        break;
                    case 't':
                        char_value = '\t';
                        break;
                    case 'r':
                        char_value = '\r';
                        break;
                    case '\\':
                        char_value = '\\';
                        break;
                    case '\'':
                        char_value = '\'';
                        break;
                    default:
                        char_value = escaped_char;
                        SourceLocation err_loc = {currentLine, currentLine, currentColumn - 2, currentColumn - 1};
                        err_loc.fileName = std::string(fileName);
                        record_error("Unknown escape sequence '\\" + std::string(1, escaped_char) + "' in char literal", err_loc);
                        break;
                    }
                    char_count = 1;
                }
            }
            else if (ch == '\n' || ch == '\r')
            {
                record_error("Newline in character literal", token_info.location);
                token_info.type = TokenType::Error;
            }
            else
            {
                char_value = consume_char();
                char_count = 1;
            }

            if (token_info.type != TokenType::Error)
            {
                if (is_at_end_of_source() || peek_char() != '\'')
                {
                    record_error("Unterminated character literal or too many characters", token_info.location);
                    token_info.type = TokenType::Error;
                    while (!is_at_end_of_source() && peek_char() != '\'' && peek_char() != '\n' && peek_char() != '\r')
                    {
                        consume_char();
                    }
                    if (!is_at_end_of_source() && peek_char() == '\'')
                        consume_char();
                }
                else
                {
                    consume_char();
                    if (char_count > 1)
                    {
                        record_error("Too many characters in character literal", token_info.location);
                        token_info.type = TokenType::Error;
                    }
                }
            }
        }

        token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
        token_info.location.lineEnd = currentLine;
        token_info.location.columnEnd = currentColumn - 1;
        if (token_info.type != TokenType::Error)
        {
            token_info.literalValue = char_value;
        }
        else
        {
            token_info.literalValue = '\0';
        }
        return token_info;
    }

    CurrentTokenInfo ScriptParser::lex_identifier_or_keyword()
    {
        CurrentTokenInfo token_info;
        token_info.location.fileName = std::string(fileName);
        token_info.location.lineStart = currentLine;
        token_info.location.columnStart = currentColumn;

        size_t token_start_offset = currentCharOffset;
        consume_char();

        while (!is_at_end_of_source())
        {
            char ch = peek_char();
            if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')
            {
                consume_char();
            }
            else
            {
                break;
            }
        }

        token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
        token_info.location.lineEnd = currentLine;
        token_info.location.columnEnd = currentColumn - 1;

        auto keyword_it = keywords.find(token_info.lexeme);
        if (keyword_it != keywords.end())
        {
            token_info.type = keyword_it->second;
            if (token_info.type == TokenType::BooleanLiteral) // Changed from TokenType::Bool
            {
                token_info.literalValue = (token_info.lexeme == "true");
            }
            else if (token_info.type == TokenType::NullLiteral)
            {
                token_info.literalValue = std::monostate{};
            }
        }
        else
        {
            token_info.type = TokenType::Identifier;
        }
        return token_info;
    }

    CurrentTokenInfo ScriptParser::lex_operator_or_punctuation()
    {
        CurrentTokenInfo token_info;
        token_info.location.fileName = std::string(fileName);
        token_info.location.lineStart = currentLine;
        token_info.location.columnStart = currentColumn;

        size_t token_start_offset = currentCharOffset;
        char c1 = consume_char();
        token_info.type = TokenType::Error;
        char c2 = peek_char(0);

        switch (c1)
        {
        case '+':
            if (c2 == '=')
            {
                consume_char();
                token_info.type = TokenType::PlusAssign;
            }
            else if (c2 == '+')
            {
                consume_char();
                token_info.type = TokenType::Increment;
            }
            else
            {
                token_info.type = TokenType::Plus;
            }
            break;
        case '-':
            if (c2 == '=')
            {
                consume_char();
                token_info.type = TokenType::MinusAssign;
            }
            else if (c2 == '-')
            {
                consume_char();
                token_info.type = TokenType::Decrement;
            }
            else
            {
                token_info.type = TokenType::Minus;
            }
            break;
        case '*':
            if (c2 == '=')
            {
                consume_char();
                token_info.type = TokenType::AsteriskAssign;
            }
            else
            {
                token_info.type = TokenType::Asterisk;
            }
            break;
        case '/':
            if (c2 == '=')
            {
                consume_char();
                token_info.type = TokenType::SlashAssign;
            }
            else
            {
                token_info.type = TokenType::Slash;
            }
            break;
        case '%':
            if (c2 == '=')
            {
                consume_char();
                token_info.type = TokenType::PercentAssign;
            }
            else
            {
                token_info.type = TokenType::Percent;
            }
            break;
        case '=':
            if (c2 == '=')
            {
                consume_char();
                token_info.type = TokenType::EqualsEquals;
            }
            else
            {
                token_info.type = TokenType::Assign;
            }
            break;
        case '!':
            if (c2 == '=')
            {
                consume_char();
                token_info.type = TokenType::NotEquals;
            }
            else
            {
                token_info.type = TokenType::LogicalNot;
            }
            break;
        case '<':
            if (c2 == '=')
            {
                consume_char();
                token_info.type = TokenType::LessThanOrEqual;
            }
            else
            {
                token_info.type = TokenType::LessThan;
            }
            break;
        case '>':
            if (c2 == '=')
            {
                consume_char();
                token_info.type = TokenType::GreaterThanOrEqual;
            }
            else
            {
                token_info.type = TokenType::GreaterThan;
            }
            break;
        case '&':
            if (c2 == '&')
            {
                consume_char();
                token_info.type = TokenType::LogicalAnd;
            }
            else
            {
                record_error("Unexpected character '&'. Did you mean '&&'?", token_info.location);
            }
            break;
        case '|':
            if (c2 == '|')
            {
                consume_char();
                token_info.type = TokenType::LogicalOr;
            }
            else
            {
                record_error("Unexpected character '|'. Did you mean '||'?", token_info.location);
            }
            break;
        case '(':
            token_info.type = TokenType::OpenParen;
            break;
        case ')':
            token_info.type = TokenType::CloseParen;
            break;
        case '{':
            token_info.type = TokenType::OpenBrace;
            break;
        case '}':
            token_info.type = TokenType::CloseBrace;
            break;
        case '[':
            token_info.type = TokenType::OpenBracket;
            break;
        case ']':
            token_info.type = TokenType::CloseBracket;
            break;
        case ';':
            token_info.type = TokenType::Semicolon;
            break;
        case ',':
            token_info.type = TokenType::Comma;
            break;
        case '.':
            token_info.type = TokenType::Dot;
            break;
        case '~':
            token_info.type = TokenType::Tilde;
            break;
        case ':':
            token_info.type = TokenType::Colon;
            break;
        default:
            SourceLocation error_loc = token_info.location;
            error_loc.columnEnd = error_loc.columnStart;
            error_loc.lineEnd = error_loc.lineStart;
            record_error("Unknown operator or punctuation character '" + std::string(1, c1) + "'", error_loc);
            break;
        }

        token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
        token_info.location.lineEnd = currentLine;
        token_info.location.columnEnd = currentColumn - 1;
        token_info.literalValue = std::monostate{};

        return token_info;
    }

    void ScriptParser::skip_whitespace_and_comments()
    {
        while (!is_at_end_of_source())
        {
            char current_char = peek_char();
            if (current_char == ' ' || current_char == '\t' || current_char == '\r' || current_char == '\n')
            {
                consume_char();
            }
            else if (current_char == '/' && peek_char(1) == '/')
            {
                consume_char();
                consume_char();
                while (!is_at_end_of_source() && peek_char() != '\n')
                {
                    consume_char();
                }
            }
            else if (current_char == '/' && peek_char(1) == '*')
            {
                SourceLocation comment_start_loc;
                comment_start_loc.fileName = std::string(fileName);
                comment_start_loc.lineStart = currentLine;
                comment_start_loc.columnStart = currentColumn;

                consume_char();
                consume_char();

                bool comment_closed = false;
                while (!is_at_end_of_source())
                {
                    char c = consume_char();
                    if (c == '*' && peek_char() == '/')
                    {
                        consume_char();
                        comment_closed = true;
                        break;
                    }
                }
                if (!comment_closed)
                {
                    record_error("Unterminated multi-line comment", comment_start_loc);
                }
            }
            else
            {
                break;
            }
        }
    }

    void ScriptParser::advance_and_lex()
    {
        previousTokenInfo = currentTokenInfo;
        skip_whitespace_and_comments();

        if (is_at_end_of_source())
        {
            currentTokenInfo.type = TokenType::EndOfFile;
            currentTokenInfo.lexeme = "";
            currentTokenInfo.location.fileName = std::string(fileName);
            currentTokenInfo.location.lineStart = currentLine;
            currentTokenInfo.location.columnStart = currentColumn;
            currentTokenInfo.location.lineEnd = currentLine;
            currentTokenInfo.location.columnEnd = currentColumn;
            currentTokenInfo.literalValue = std::monostate{};
        }
        else
        {
            char first_char = peek_char();
            if (std::isalpha(static_cast<unsigned char>(first_char)) || first_char == '_')
            {
                currentTokenInfo = lex_identifier_or_keyword();
            }
            else if (std::isdigit(static_cast<unsigned char>(first_char)))
            {
                currentTokenInfo = lex_number_literal();
            }
            else if (first_char == '"')
            {
                currentTokenInfo = lex_string_literal();
            }
            else if (first_char == '\'')
            {
                currentTokenInfo = lex_char_literal();
            }
            else
            {
                currentTokenInfo = lex_operator_or_punctuation();
            }
        }

        // Debug Print (Optional)
        // std::cout << "LEXED TOKEN: Type=" << token_type_to_string(currentTokenInfo.type)
        //           << ", Lexeme='";
        // std::cout.write(currentTokenInfo.lexeme.data(), currentTokenInfo.lexeme.length());
        // std::cout << "', Loc=" << currentTokenInfo.location.to_string();
        // std::cout << std::endl;
    }

    char ScriptParser::peek_char(size_t offset) const
    {
        if (currentCharOffset + offset >= sourceCode.length())
        {
            return '\0';
        }
        return sourceCode[currentCharOffset + offset];
    }

    char ScriptParser::consume_char()
    {
        if (is_at_end_of_source())
        {
            return '\0';
        }
        char current_char = sourceCode[currentCharOffset];
        currentCharOffset++;

        if (current_char == '\n')
        {
            currentLine++;
            currentColumn = 1;
            currentLineStartOffset = currentCharOffset;
        }
        else
        {
            currentColumn++;
        }
        return current_char;
    }

    bool ScriptParser::is_at_end_of_source() const
    {
        return currentCharOffset >= sourceCode.length();
    }

} // namespace Mycelium::Scripting::Lang
