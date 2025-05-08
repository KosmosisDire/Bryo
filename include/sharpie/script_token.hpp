#pragma once

#include "script_ast.hpp"
#include <string>
#include <vector>
#include <optional>

namespace Mycelium::Scripting::Lang
{

enum class TokenType
{
    // Keywords
    Namespace, Using, Public, Private, Protected, Internal, Static, Readonly,
    Class, Struct, Void,
    Int, Long, Double, Bool, Char, String, // Primitive type keywords
    If, Else, While, For, ForEach, In,
    Return, Break, Continue,
    New, This, Null, True, False, Var,

    // Identifiers
    Identifier,

    // Literals
    IntegerLiteral,     // 123, 123L
    DoubleLiteral,      // 1.23, 1.23d, 1.23f (or actual double)
    StringLiteral,      // "hello"
    CharLiteral,        // 'c'

    // Operators & Punctuation
    OpenParen, CloseParen,       // ( )
    OpenBrace, CloseBrace,       // { }
    OpenBracket, CloseBracket,   // [ ]
    Semicolon, Colon, Comma, Dot, // ; : , .
    QuestionMark,                // ?

    // Assignment & Arithmetic
    Assign,                      // =
    Plus, Minus, Asterisk, Slash, Percent, // + - * / %
    PlusAssign, MinusAssign, AsteriskAssign, SlashAssign, PercentAssign, // += -= *= /= %=
    Increment, Decrement,        // ++ --

    // Logical & Relational
    LogicalAnd, LogicalOr, LogicalNot, // && || !
    EqualsEquals, NotEquals,            // == !=
    LessThan, GreaterThan, LessThanOrEqual, GreaterThanOrEqual, // < > <= >=

    // Comments (usually skipped, but token type can exist for tools)
    // LineComment,
    // BlockComment,

    // Special
    EndOfFile,
    Unknown // For errors or unrecognized characters
};

struct Token
{
    TokenType type;
    std::string lexeme;
    // Optional: std::variant<long long, double, std::string, bool, char> value;
    Mycelium::Scripting::Lang::SourceLocation location;
};

// Helper function to convert TokenType to a string (useful for debugging)
std::string token_type_to_string(TokenType type);
}