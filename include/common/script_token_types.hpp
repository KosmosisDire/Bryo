#pragma once
#include <unordered_map>
#include <string> // Required for std::to_string and std::string

namespace Mycelium::Scripting::Lang
{
    enum class TokenType
    {
        Error,
        EndOfFile,

        Identifier,
        BooleanLiteral,
        IntegerLiteral,
        LongLiteral,     
        FloatLiteral, // For numeric literals like 3.14f
        DoubleLiteral, // For numeric literals like 3.14 or 3.14d
        CharLiteral,
        StringLiteral,
        NullLiteral,

        // Punctuation
        OpenParen, CloseParen,         // ( )
        OpenBrace, CloseBrace,         // { }
        OpenBracket, CloseBracket,     // [ and ]
        Semicolon,                     // ;
        Comma,                         // ,
        Dot,                           // .
        Tilde,                         // ~
        Colon,                         // :
        // QuestionMark, etc.

        // Keywords (as before)
        Var, If, Else, While, For, ForEach, Return, New, This,
        Class, Struct, Namespace, Using, Extern,
        Public, Private, Static, Readonly, Virtual,
        
        // Primitive Type Name Keywords
        Bool, Int, String, Long, Double, Char, Void, Float, // Added Float here

        Break, Continue,

        // Operators
        Assign, Plus, Minus, Asterisk, Slash, Percent, // Added Percent
        Increment, Decrement,                         // ++ -- Added

        EqualsEquals, NotEquals,                      // == !=
        LessThan, GreaterThan,                        // < >
        LessThanOrEqual, GreaterThanOrEqual,          // <= >=

        LogicalAnd, LogicalOr, LogicalNot,            // && || !

        PlusAssign, MinusAssign,                      // += -= Added
        AsteriskAssign, SlashAssign, PercentAssign,   // *= /= %= Added
    };


    static const std::unordered_map<std::string_view, TokenType> keywords =
    {
        // Control Flow & Misc Keywords
        {"var", TokenType::Var},
        {"if", TokenType::If},
        {"else", TokenType::Else},
        {"while", TokenType::While},
        {"for", TokenType::For},
        {"foreach", TokenType::ForEach},
        {"return", TokenType::Return},
        {"new", TokenType::New},
        {"this", TokenType::This},
        {"break", TokenType::Break},
        {"continue", TokenType::Continue},

        // Declaration Keywords
        {"class", TokenType::Class},
        {"struct", TokenType::Struct},
        {"namespace", TokenType::Namespace},
        {"using", TokenType::Using},
        {"extern", TokenType::Extern},

        // Modifier Keywords
        {"public", TokenType::Public},
        {"private", TokenType::Private},
        {"static", TokenType::Static},
        {"readonly", TokenType::Readonly},
        {"virtual", TokenType::Virtual},
        // Add other modifiers like protected, internal, readonly if they become keywords

        // Literal Keywords
        {"true", TokenType::BooleanLiteral},
        {"false", TokenType::BooleanLiteral},
        {"null", TokenType::NullLiteral},

        // Primitive Type Name Keywords
        {"bool", TokenType::Bool},
        {"int", TokenType::Int},
        {"string", TokenType::String},
        {"long", TokenType::Long},
        {"double", TokenType::Double},
        {"char", TokenType::Char},
        {"void", TokenType::Void},
        {"float", TokenType::Float} // Added float keyword mapping
    };

    // Helper to convert TokenType to string (for error messages)
    inline std::string token_type_to_string(TokenType type) 
    {
        switch (type)
        {
            // Meta Tokens
            case TokenType::Error: return "Error";
            case TokenType::EndOfFile: return "EndOfFile";

            // Literals
            case TokenType::Identifier: return "Identifier";
            case TokenType::BooleanLiteral: return "BooleanLiteral";
            case TokenType::IntegerLiteral: return "IntegerLiteral";
            case TokenType::LongLiteral: return "LongLiteral";
            case TokenType::FloatLiteral: return "FloatLiteral";
            case TokenType::DoubleLiteral: return "DoubleLiteral";
            case TokenType::CharLiteral: return "CharLiteral";
            case TokenType::StringLiteral: return "StringLiteral";
            case TokenType::NullLiteral: return "NullLiteral";

            // Punctuation
            case TokenType::OpenParen: return "OpenParen";
            case TokenType::CloseParen: return "CloseParen";
            case TokenType::OpenBrace: return "OpenBrace";
            case TokenType::CloseBrace: return "CloseBrace";
            case TokenType::OpenBracket: return "OpenBracket";
            case TokenType::CloseBracket: return "CloseBracket";
            case TokenType::Semicolon: return "Semicolon";
            case TokenType::Comma: return "Comma";
            case TokenType::Dot: return "Dot";
            case TokenType::Tilde: return "Tilde";
            case TokenType::Colon: return "Colon";

            // Keywords (General Purpose & Control Flow)
            case TokenType::Var: return "Var";
            case TokenType::If: return "If";
            case TokenType::Else: return "Else";
            case TokenType::While: return "While";
            case TokenType::For: return "For";
            case TokenType::ForEach: return "ForEach";
            case TokenType::Return: return "Return";
            case TokenType::New: return "New";
            case TokenType::This: return "This";
            case TokenType::Break: return "Break";
            case TokenType::Continue: return "Continue";

            // Keywords (Declarations & Modifiers)
            case TokenType::Class: return "Class";
            case TokenType::Struct: return "Struct";
            case TokenType::Namespace: return "Namespace";
            case TokenType::Using: return "Using";
            case TokenType::Extern: return "Extern";
            case TokenType::Public: return "Public";
            case TokenType::Private: return "Private";
            case TokenType::Static: return "Static";
            case TokenType::Readonly: return "Readonly";
            case TokenType::Virtual: return "Virtual";

            // Keywords (Primitive Type Aliases)
            case TokenType::Bool: return "Bool";
            case TokenType::Int: return "Int";
            case TokenType::String: return "String";
            case TokenType::Long: return "Long";
            case TokenType::Double: return "Double";
            case TokenType::Char: return "Char";   
            case TokenType::Void: return "Void";
            case TokenType::Float: return "Float"; // Added Float case

            // Operators
            case TokenType::Assign: return "Assign";
            case TokenType::Plus: return "Plus";
            case TokenType::Minus: return "Minus";
            case TokenType::Asterisk: return "Asterisk";
            case TokenType::Slash: return "Slash";
            case TokenType::Percent: return "Percent";
            case TokenType::Increment: return "Increment";
            case TokenType::Decrement: return "Decrement";

            case TokenType::EqualsEquals: return "EqualsEquals";
            case TokenType::NotEquals: return "NotEquals";
            case TokenType::LessThan: return "LessThan";
            case TokenType::GreaterThan: return "GreaterThan";
            case TokenType::LessThanOrEqual: return "LessThanOrEqual";
            case TokenType::GreaterThanOrEqual: return "GreaterThanOrEqual";

            case TokenType::LogicalAnd: return "LogicalAnd";
            case TokenType::LogicalOr: return "LogicalOr";
            case TokenType::LogicalNot: return "LogicalNot";

            case TokenType::PlusAssign: return "PlusAssign";
            case TokenType::MinusAssign: return "MinusAssign";
            case TokenType::AsteriskAssign: return "AsteriskAssign";
            case TokenType::SlashAssign: return "SlashAssign";
            case TokenType::PercentAssign: return "PercentAssign";

            default:
                return "UnknownTokenType(" + std::to_string(static_cast<int>(type)) + ")";
        }
    }
}
