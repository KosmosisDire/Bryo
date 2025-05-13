#pragma once

#include "script_ast.hpp" // For SourceLocation
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
    IntegerLiteral,
    DoubleLiteral,
    StringLiteral,
    CharLiteral,

    // Operators & Punctuation
    OpenParen, CloseParen,
    OpenBrace, CloseBrace,
    OpenBracket, CloseBracket,
    Semicolon, Colon, Comma, Dot,
    QuestionMark,

    // Assignment & Arithmetic
    Assign,
    Plus, Minus, Asterisk, Slash, Percent,
    PlusAssign, MinusAssign, AsteriskAssign, SlashAssign, PercentAssign,
    Increment, Decrement,

    // Logical & Relational
    LogicalAnd, LogicalOr, LogicalNot,
    EqualsEquals, NotEquals,
    LessThan, GreaterThan, LessThanOrEqual, GreaterThanOrEqual,

    // Special
    At,
    EndOfFile,
    Unknown // For errors or unrecognized characters (primarily for tokenizer internal use before throwing)
};

struct Token
{
    TokenType type;
    std::string lexeme;
    Mycelium::Scripting::Lang::SourceLocation location;
};

// Helper function to convert TokenType to a string (useful for debugging)
std::string token_type_to_string(TokenType type);

} // namespace Mycelium::Scripting::Lang