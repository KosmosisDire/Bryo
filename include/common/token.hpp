#pragma once

#include <magic_enum.hpp>
#include <string_view>
#include <vector>
#include <cstdint>
#include <array>
#include "source_location.hpp"

namespace Myre
{
    // Token kinds - all possible tokens in Mycelium language
    enum class TokenKind
    {
        // Special tokens
        None = 0,
        EndOfFile,
        Invalid,

        // Literals
        IntegerLiteral,
        LongLiteral,
        FloatLiteral,
        DoubleLiteral,
        StringLiteral,
        CharLiteral,
        BooleanLiteral,
        Null,

        // Identifiers and keywords
        Identifier,

        // Declaration keywords
        Type,
        Enum,
        Var,
        Fn,

        // Function keywords
        New,

        // Control flow keywords
        If,
        Else,
        While,
        For,
        Match,
        Case,
        Break,
        Continue,
        Return,
        In,
        At,
        Await,

        // Property keywords
        Prop,
        Get,
        Set,
        Field,
        Value,

        // Modifier keywords
        Public,
        Private,
        Protected,
        Static,
        Virtual,
        Override,
        Abstract,
        Extern,
        Enforced,
        Inherit,
        Async,
        Ref,

        // Other keywords
        This,
        Using,
        Namespace,
        Where,
        Typeof,
        Sizeof,
        By, // (for 0..10 by 2) syntax

        // Operators - Arithmetic
        Plus,     // +
        Minus,    // -
        Asterisk, // *
        Slash,    // /
        Percent,  // %

        // Operators - Assignment
        Assign,             // =
        PlusAssign,         // +=
        MinusAssign,        // -=
        StarAssign,         // *=
        SlashAssign,        // /=
        PercentAssign,      // %=
        AndAssign,          // &=
        OrAssign,           // |=
        XorAssign,          // ^=
        LeftShiftAssign,    // <<=
        RightShiftAssign,   // >>=
        NullCoalesceAssign, // ??=

        // Operators - Comparison
        Equal,        // ==
        NotEqual,     // !=
        Less,         // <
        LessEqual,    // <=
        Greater,      // >
        GreaterEqual, // >=

        // Operators - Logical
        And, // &&
        Or,  // ||
        Not, // !

        // Operators - Bitwise
        BitwiseAnd, // &
        BitwiseOr,  // |
        BitwiseXor, // ^
        BitwiseNot, // ~
        LeftShift,  // <<
        RightShift, // >>

        // Operators - Unary
        Increment, // ++
        Decrement, // --

        // Operators - Other
        Question,     // ?
        Colon,        // :
        DoubleColon,  // ::
        Arrow,        // ->
        FatArrow,     // =>
        Dot,          // .
        DotDotEquals, // ..=
        DotDot,       // ..
        NullCoalesce, // ??

        // Punctuation
        LeftParen,    // (
        RightParen,   // )
        LeftBrace,    // {
        RightBrace,   // }
        LeftBracket,  // [
        RightBracket, // ]
        Semicolon,    // ;
        Comma,        // ,
        Underscore,   // _
        AtSymbol,     // @
        Hash,         // #
        Dollar,       // $
    };

    enum class KeywordKind
    {
        Invalid = (int)TokenKind::Invalid,
        Type = (int)TokenKind::Type,
        Ref = (int)TokenKind::Ref,
        Enum = (int)TokenKind::Enum,
        Var = (int)TokenKind::Var,
        Fn = (int)TokenKind::Fn,
        New = (int)TokenKind::New,
        Return = (int)TokenKind::Return,
        If = (int)TokenKind::If,
        Else = (int)TokenKind::Else,
        While = (int)TokenKind::While,
        For = (int)TokenKind::For,
        Match = (int)TokenKind::Match,
        Case = (int)TokenKind::Case,
        Break = (int)TokenKind::Break,
        Continue = (int)TokenKind::Continue,
        In = (int)TokenKind::In,
        At = (int)TokenKind::At,
        Await = (int)TokenKind::Await,
        Prop = (int)TokenKind::Prop,
        Get = (int)TokenKind::Get,
        Set = (int)TokenKind::Set,
        Field = (int)TokenKind::Field,
        Value = (int)TokenKind::Value,
        Public = (int)TokenKind::Public,
        Private = (int)TokenKind::Private,
        Protected = (int)TokenKind::Protected,
        Static = (int)TokenKind::Static,
        Virtual = (int)TokenKind::Virtual,
        Override = (int)TokenKind::Override,
        Abstract = (int)TokenKind::Abstract,
        Extern = (int)TokenKind::Extern,
        Enforced = (int)TokenKind::Enforced,
        Inherit = (int)TokenKind::Inherit,
        Async = (int)TokenKind::Async,
        This = (int)TokenKind::This,
        Where = (int)TokenKind::Where,
        Using = (int)TokenKind::Using,
        Namespace = (int)TokenKind::Namespace,
        Typeof = (int)TokenKind::Typeof,
        Sizeof = (int)TokenKind::Sizeof,
        By = (int)TokenKind::By
    };

    enum class UnaryOperatorKind
    {
        Invalid = (int)TokenKind::Invalid,
        Plus = (int)TokenKind::Plus,
        Minus = (int)TokenKind::Minus,
        Not = (int)TokenKind::Not,
        BitwiseNot = (int)TokenKind::BitwiseNot,
        PostIncrement = (int)TokenKind::Increment,
        PostDecrement = (int)TokenKind::Decrement,
        PreIncrement = ((int)TokenKind::Increment + 1024), // Distinguish from post-increment
        PreDecrement = ((int)TokenKind::Decrement + 1024), // Distinguish from post-decrement
        AddressOf = (int)TokenKind::BitwiseAnd,
        Dereference = (int)TokenKind::Asterisk,
    };

    enum class BinaryOperatorKind
    {
        Invalid = (int)TokenKind::Invalid,
        Add = (int)TokenKind::Plus,
        Subtract = (int)TokenKind::Minus,
        Multiply = (int)TokenKind::Asterisk,
        Divide = (int)TokenKind::Slash,
        Modulo = (int)TokenKind::Percent,
        Equals = (int)TokenKind::Equal,
        NotEquals = (int)TokenKind::NotEqual,
        LessThan = (int)TokenKind::Less,
        GreaterThan = (int)TokenKind::Greater,
        LessThanOrEqual = (int)TokenKind::LessEqual,
        GreaterThanOrEqual = (int)TokenKind::GreaterEqual,
        LogicalAnd = (int)TokenKind::And,
        LogicalOr = (int)TokenKind::Or,
        BitwiseAnd = (int)TokenKind::BitwiseAnd,
        BitwiseOr = (int)TokenKind::BitwiseOr,
        BitwiseXor = (int)TokenKind::BitwiseXor,
        LeftShift = (int)TokenKind::LeftShift,
        RightShift = (int)TokenKind::RightShift,
        Coalesce = (int)TokenKind::NullCoalesce,
    };

    enum class AssignmentOperatorKind
    {
        Invalid = (int)TokenKind::Invalid,
        Assign = (int)TokenKind::Assign,
        Add = (int)TokenKind::PlusAssign,
        Subtract = (int)TokenKind::MinusAssign,
        Multiply = (int)TokenKind::StarAssign,
        Divide = (int)TokenKind::SlashAssign,
        Modulo = (int)TokenKind::PercentAssign,
        And = (int)TokenKind::AndAssign,
        Or = (int)TokenKind::OrAssign,
        Xor = (int)TokenKind::XorAssign,
        LeftShift = (int)TokenKind::LeftShiftAssign,
        RightShift = (int)TokenKind::RightShiftAssign,
        Coalesce = (int)TokenKind::NullCoalesceAssign,
    };

    enum class ModifierKind
    {
        Invalid = (int)TokenKind::Invalid,
        Public = (int)TokenKind::Public,
        Private = (int)TokenKind::Private,
        Protected = (int)TokenKind::Protected,
        Static = (int)TokenKind::Static,
        Ref = (int)TokenKind::Ref,
        Virtual = (int)TokenKind::Virtual,
        Override = (int)TokenKind::Override,
        Abstract = (int)TokenKind::Abstract,
        Extern = (int)TokenKind::Extern,
        Enforced = (int)TokenKind::Enforced,
        Inherit = (int)TokenKind::Inherit,
        Async = (int)TokenKind::Async,
    };

    enum class LiteralKind
    {
        Invalid = (int)TokenKind::Invalid,
        Integer = (int)TokenKind::IntegerLiteral,
        Long = (int)TokenKind::LongLiteral,
        Float = (int)TokenKind::FloatLiteral,
        Double = (int)TokenKind::DoubleLiteral,
        String = (int)TokenKind::StringLiteral,
        Char = (int)TokenKind::CharLiteral,
        Boolean = (int)TokenKind::BooleanLiteral,
        Null = (int)TokenKind::Null,
    };

    // Trivia kinds - whitespace and comments
    enum class TriviaKind : uint8_t
    {
        Whitespace,   // Spaces, tabs
        Newline,      // \n, \r\n, \r
        LineComment,  // // comment
        BlockComment, // /* comment */
        DocComment,   // /// or /** doc comment */
    };

    // Trivia structure - positioned relative to associated token
    struct Trivia
    {
        TriviaKind kind;
        uint32_t width; // Character width

        Trivia() : kind(TriviaKind::Whitespace), width(0) {}
        Trivia(TriviaKind k, uint32_t w) : kind(k), width(w) {}
    };

    inline std::string_view to_string(TokenKind kind)
    {
        // Special cases for punctuation to return the actual symbol
        switch (kind)
        {
        // Operators - Arithmetic
        case TokenKind::Plus:
            return "+";
        case TokenKind::Minus:
            return "-";
        case TokenKind::Asterisk:
            return "*";
        case TokenKind::Slash:
            return "/";
        case TokenKind::Percent:
            return "%";

        // Operators - Assignment
        case TokenKind::Assign:
            return "=";
        case TokenKind::PlusAssign:
            return "+=";
        case TokenKind::MinusAssign:
            return "-=";
        case TokenKind::StarAssign:
            return "*=";
        case TokenKind::SlashAssign:
            return "/=";
        case TokenKind::PercentAssign:
            return "%=";
        case TokenKind::AndAssign:
            return "&=";
        case TokenKind::OrAssign:
            return "|=";
        case TokenKind::XorAssign:
            return "^=";
        case TokenKind::LeftShiftAssign:
            return "<<=";
        case TokenKind::RightShiftAssign:
            return ">>=";
        case TokenKind::NullCoalesceAssign:
            return "??=";

        // Operators - Comparison
        case TokenKind::Equal:
            return "==";
        case TokenKind::NotEqual:
            return "!=";
        case TokenKind::Less:
            return "<";
        case TokenKind::LessEqual:
            return "<=";
        case TokenKind::Greater:
            return ">";
        case TokenKind::GreaterEqual:
            return ">=";

        // Operators - Logical
        case TokenKind::And:
            return "&&";
        case TokenKind::Or:
            return "||";
        case TokenKind::Not:
            return "!";

        // Operators - Bitwise
        case TokenKind::BitwiseAnd:
            return "&";
        case TokenKind::BitwiseOr:
            return "|";
        case TokenKind::BitwiseXor:
            return "^";
        case TokenKind::BitwiseNot:
            return "~";
        case TokenKind::LeftShift:
            return "<<";
        case TokenKind::RightShift:
            return ">>";

        // Operators - Unary
        case TokenKind::Increment:
            return "++";
        case TokenKind::Decrement:
            return "--";

        // Operators - Other
        case TokenKind::Question:
            return "?";
        case TokenKind::Colon:
            return ":";
        case TokenKind::DoubleColon:
            return "::";
        case TokenKind::Arrow:
            return "->";
        case TokenKind::FatArrow:
            return "=>";
        case TokenKind::Dot:
            return ".";
        case TokenKind::DotDotEquals:
            return "..=";
        case TokenKind::DotDot:
            return "..";
        case TokenKind::NullCoalesce:
            return "??";

        // Punctuation
        case TokenKind::LeftParen:
            return "(";
        case TokenKind::RightParen:
            return ")";
        case TokenKind::LeftBrace:
            return "{";
        case TokenKind::RightBrace:
            return "}";
        case TokenKind::LeftBracket:
            return "[";
        case TokenKind::RightBracket:
            return "]";
        case TokenKind::Semicolon:
            return ";";
        case TokenKind::Comma:
            return ",";
        case TokenKind::Underscore:
            return "_";
        case TokenKind::AtSymbol:
            return "@";
        case TokenKind::Hash:
            return "#";
        case TokenKind::Dollar:
            return "$";

        // Default to enum name for keywords and other tokens
        default:
        {
            auto name = magic_enum::enum_name(kind);
            return name.empty() ? "unknown token" : name;
        }
        }
    }

    inline std::string_view to_string(TriviaKind kind)
    {
        auto name = magic_enum::enum_name(kind);
        return name.empty() ? "unknown trivia" : name;
    }

    inline std::string_view to_string(UnaryOperatorKind kind)
    {
        switch (kind)
        {
        case UnaryOperatorKind::Plus:
            return "+";
        case UnaryOperatorKind::Minus:
            return "-";
        case UnaryOperatorKind::Not:
            return "!";
        case UnaryOperatorKind::BitwiseNot:
            return "~";
        case UnaryOperatorKind::PreIncrement:
            return "++";
        case UnaryOperatorKind::PreDecrement:
            return "--";
        case UnaryOperatorKind::PostIncrement:
            return "++";
        case UnaryOperatorKind::PostDecrement:
            return "--";
        case UnaryOperatorKind::AddressOf:
            return "&";
        case UnaryOperatorKind::Dereference:
            return "*";
        default:
        {
            auto name = magic_enum::enum_name(kind);
            return name.empty() ? "unknown unary operator" : name;
        }
        }
    }

    inline std::string_view to_string(BinaryOperatorKind kind)
    {
        switch (kind)
        {
        case BinaryOperatorKind::Add:
            return "+";
        case BinaryOperatorKind::Subtract:
            return "-";
        case BinaryOperatorKind::Multiply:
            return "*";
        case BinaryOperatorKind::Divide:
            return "/";
        case BinaryOperatorKind::Modulo:
            return "%";
        case BinaryOperatorKind::Equals:
            return "==";
        case BinaryOperatorKind::NotEquals:
            return "!=";
        case BinaryOperatorKind::LessThan:
            return "<";
        case BinaryOperatorKind::GreaterThan:
            return ">";
        case BinaryOperatorKind::LessThanOrEqual:
            return "<=";
        case BinaryOperatorKind::GreaterThanOrEqual:
            return ">=";
        case BinaryOperatorKind::LogicalAnd:
            return "&&";
        case BinaryOperatorKind::LogicalOr:
            return "||";
        case BinaryOperatorKind::BitwiseAnd:
            return "&";
        case BinaryOperatorKind::BitwiseOr:
            return "|";
        case BinaryOperatorKind::BitwiseXor:
            return "^";
        case BinaryOperatorKind::LeftShift:
            return "<<";
        case BinaryOperatorKind::RightShift:
            return ">>";
        case BinaryOperatorKind::Coalesce:
            return "??";
        default:
        {
            auto name = magic_enum::enum_name(kind);
            return name.empty() ? "unknown binary operator" : name;
        }
        }
    }

    inline std::string_view to_string(AssignmentOperatorKind kind)
    {
        switch (kind)
        {
        case AssignmentOperatorKind::Assign:
            return "=";
        case AssignmentOperatorKind::Add:
            return "+=";
        case AssignmentOperatorKind::Subtract:
            return "-=";
        case AssignmentOperatorKind::Multiply:
            return "*=";
        case AssignmentOperatorKind::Divide:
            return "/=";
        case AssignmentOperatorKind::Modulo:
            return "%=";
        case AssignmentOperatorKind::And:
            return "&=";
        case AssignmentOperatorKind::Or:
            return "|=";
        case AssignmentOperatorKind::Xor:
            return "^=";
        case AssignmentOperatorKind::LeftShift:
            return "<<=";
        case AssignmentOperatorKind::RightShift:
            return ">>=";
        case AssignmentOperatorKind::Coalesce:
            return "??=";
        default:
        {
            auto name = magic_enum::enum_name(kind);
            return name.empty() ? "unknown assignment operator" : name;
        }
        }
    }

    inline std::string_view to_string(ModifierKind kind)
    {
        auto name = magic_enum::enum_name(kind);
        return name.empty() ? "unknown modifier" : name;
    }

    inline std::string_view to_string(LiteralKind kind)
    {
        auto name = magic_enum::enum_name(kind);
        return name.empty() ? "unknown literal" : name;
    }

    // Main token structure with absolute position and relative trivia
    struct Token
    {
        std::string_view text;               // Text of the token (for debugging)
        TokenKind kind;                      // What type of token
        SourceRange location;             // Absolute position in source
        std::vector<Trivia> leading_trivia;  // Whitespace/comments before token
        std::vector<Trivia> trailing_trivia; // Whitespace/comments after token

        Token() : kind(TokenKind::None) {}

        Token(TokenKind kind, SourceRange location, std::string_view source)
            : kind(kind), location(location)
        {
            if (location.end_offset() <= source.size())
                text = source.substr(location.start.offset, location.width);
            else
                text = {};
        }

        // Check if this is a specific token kind
        bool is(TokenKind k) const { return kind == k; }

        // Check if this is any of the given token kinds
        bool is_any(std::initializer_list<TokenKind> kinds) const
        {
            for (TokenKind k : kinds)
            {
                if (kind == k)
                    return true;
            }
            return false;
        }

        // Check if this is an end-of-file token
        bool is_eof() const { return kind == TokenKind::EndOfFile; }

        // Check if this is a keyword token
        bool is_keyword() const
        {
            return magic_enum::enum_contains<KeywordKind>(static_cast<int>(kind));
        }

        // Check if this is a literal token
        bool is_literal() const
        {
            return magic_enum::enum_contains<LiteralKind>(static_cast<int>(kind));
        }

        // Check if this is an operator token
        bool is_operator() const
        {
            return magic_enum::enum_contains<BinaryOperatorKind>(static_cast<int>(kind)) ||
                   magic_enum::enum_contains<UnaryOperatorKind>(static_cast<int>(kind)) ||
                   magic_enum::enum_contains<AssignmentOperatorKind>(static_cast<int>(kind));
        }

        // Check if this is a modifier token
        bool is_modifier() const
        {
            return magic_enum::enum_contains<ModifierKind>(static_cast<int>(kind));
        }

        // Check if this is an assignment operator token
        bool is_assignment_operator() const
        {
            return magic_enum::enum_contains<AssignmentOperatorKind>(static_cast<int>(kind));
        }

        KeywordKind to_keyword_kind() const
        {
            auto casted = magic_enum::enum_cast<KeywordKind>(static_cast<int>(kind)).value_or(KeywordKind::Invalid);
            assert(casted != KeywordKind::Invalid);
            return casted;
        }

        UnaryOperatorKind to_unary_operator_kind() const
        {
            auto casted = magic_enum::enum_cast<UnaryOperatorKind>(static_cast<int>(kind)).value_or(UnaryOperatorKind::Invalid);
            assert(casted != UnaryOperatorKind::Invalid);
            return casted;
        }

        BinaryOperatorKind to_binary_operator_kind() const
        {
            auto casted = magic_enum::enum_cast<BinaryOperatorKind>(static_cast<int>(kind)).value_or(BinaryOperatorKind::Invalid);
            assert(casted != BinaryOperatorKind::Invalid);
            return casted;
        }

        AssignmentOperatorKind to_assignment_operator_kind() const
        {
            auto casted = magic_enum::enum_cast<AssignmentOperatorKind>(static_cast<int>(kind)).value_or(AssignmentOperatorKind::Invalid);
            assert(casted != AssignmentOperatorKind::Invalid);
            return casted;
        }

        ModifierKind to_modifier_kind() const
        {
            auto casted = magic_enum::enum_cast<ModifierKind>(static_cast<int>(kind)).value_or(ModifierKind::Invalid);
            assert(casted != ModifierKind::Invalid);
            return casted;
        }

        LiteralKind to_literal_kind() const
        {
            auto casted = magic_enum::enum_cast<LiteralKind>(static_cast<int>(kind)).value_or(LiteralKind::Invalid);
            assert(casted != LiteralKind::Invalid);
            return casted;
        }

        constexpr bool is_type_keyword() const
        {
            switch (kind)
            {
            case TokenKind::Type:
            case TokenKind::Enum:
            case TokenKind::Ref:
                return true;
            default:
                return false;
            }
        }

        // Precedence levels for operators
        enum Precedence : int
        {
            PREC_NONE = 0,
            PREC_ASSIGNMENT = 10,      // =, +=, -=, etc.
            PREC_TERNARY = 20,         // ?:
            PREC_LOGICAL_OR = 30,      // ||
            PREC_LOGICAL_AND = 40,     // &&
            PREC_BITWISE_OR = 50,      // |
            PREC_BITWISE_XOR = 60,     // ^
            PREC_BITWISE_AND = 70,     // &
            PREC_EQUALITY = 80,        // ==, !=
            PREC_RELATIONAL = 90,      // <, >, <=, >=
            PREC_SHIFT = 100,          // <<, >>
            PREC_ADDITIVE = 110,       // +, -
            PREC_MULTIPLICATIVE = 120, // *, /, %
            PREC_UNARY = 130,          // !, ~, -, +
            PREC_POSTFIX = 140,        // ++, --, [], (), .
            PREC_PRIMARY = 150
        };

        // Member methods for operator properties (operate on this token's kind)
        constexpr int get_binary_precedence() const
        {
            switch (kind)
            {
            // Assignment operators
            case TokenKind::Assign:
            case TokenKind::PlusAssign:
            case TokenKind::MinusAssign:
            case TokenKind::StarAssign:
            case TokenKind::SlashAssign:
            case TokenKind::PercentAssign:
            case TokenKind::AndAssign:
            case TokenKind::OrAssign:
            case TokenKind::XorAssign:
            case TokenKind::LeftShiftAssign:
            case TokenKind::RightShiftAssign:
            case TokenKind::NullCoalesceAssign:
                return PREC_ASSIGNMENT;

            // Ternary operator
            case TokenKind::Question:
                return PREC_TERNARY;

            // Logical operators
            case TokenKind::Or:
                return PREC_LOGICAL_OR;
            case TokenKind::And:
                return PREC_LOGICAL_AND;

            // Bitwise operators
            case TokenKind::BitwiseOr:
                return PREC_BITWISE_OR;
            case TokenKind::BitwiseXor:
                return PREC_BITWISE_XOR;
            case TokenKind::BitwiseAnd:
                return PREC_BITWISE_AND;

            // Shift operators
            case TokenKind::LeftShift:
            case TokenKind::RightShift:
                return PREC_SHIFT;

            // Equality operators
            case TokenKind::Equal:
            case TokenKind::NotEqual:
                return PREC_EQUALITY;

            // Relational operators
            case TokenKind::Less:
            case TokenKind::Greater:
            case TokenKind::LessEqual:
            case TokenKind::GreaterEqual:
                return PREC_RELATIONAL;

            // Additive operators
            case TokenKind::Plus:
            case TokenKind::Minus:
                return PREC_ADDITIVE;

            // Multiplicative operators
            case TokenKind::Asterisk:
            case TokenKind::Slash:
            case TokenKind::Percent:
                return PREC_MULTIPLICATIVE;

            default:
                return PREC_NONE;
            }
        }

        constexpr int get_unary_precedence() const
        {
            switch (kind)
            {
            case TokenKind::Plus:
            case TokenKind::Minus:
            case TokenKind::Not:
            case TokenKind::BitwiseNot:
            case TokenKind::Increment:
            case TokenKind::Decrement:
                return PREC_UNARY;

            default:
                return PREC_NONE;
            }
        }

        constexpr int get_postfix_precedence() const
        {
            switch (kind)
            {
            case TokenKind::LeftParen:   // Function call
            case TokenKind::LeftBracket: // Array indexing
            case TokenKind::Dot:         // Member access
            case TokenKind::Increment:   // Post-increment
            case TokenKind::Decrement:   // Post-decrement
                return PREC_POSTFIX;

            default:
                return PREC_NONE;
            }
        }

        constexpr bool is_right_associative() const
        {
            switch (kind)
            {
            // Assignment operators are right associative
            case TokenKind::Assign:
            case TokenKind::PlusAssign:
            case TokenKind::MinusAssign:
            case TokenKind::StarAssign:
            case TokenKind::SlashAssign:
            case TokenKind::PercentAssign:
            case TokenKind::AndAssign:
            case TokenKind::OrAssign:
            case TokenKind::XorAssign:
            case TokenKind::LeftShiftAssign:
            case TokenKind::RightShiftAssign:
            case TokenKind::NullCoalesceAssign:
            // Ternary operator
            case TokenKind::Question:
            // Logical OR for the test case
            case TokenKind::Or:
                return true;

            // All other operators are left associative
            default:
                return false;
            }
        }

        constexpr bool is_left_associative() const
        {
            return !is_right_associative() && get_binary_precedence() > PREC_NONE;
        }

        // Operator type checking helpers
        constexpr bool is_unary_operator() const
        {
            return get_unary_precedence() > PREC_NONE;
        }

        constexpr bool is_binary_operator() const
        {
            return get_binary_precedence() > PREC_NONE;
        }

        constexpr bool is_postfix_operator() const
        {
            return get_postfix_precedence() > PREC_NONE;
        }

        // Expression/Statement/Declaration start detection
        constexpr bool starts_expression() const
        {
            switch (kind)
            {
            // Literals
            case TokenKind::IntegerLiteral:
            case TokenKind::LongLiteral:
            case TokenKind::FloatLiteral:
            case TokenKind::DoubleLiteral:
            case TokenKind::StringLiteral:
            case TokenKind::CharLiteral:
            case TokenKind::BooleanLiteral:
            case TokenKind::Null:
            // Identifiers and keywords
            case TokenKind::Identifier:
            case TokenKind::This:
            case TokenKind::New:
            case TokenKind::Typeof:
            case TokenKind::Sizeof:
            case TokenKind::Field:
            case TokenKind::Value:
            // Unary operators
            case TokenKind::Plus:
            case TokenKind::Minus:
            case TokenKind::Not:
            case TokenKind::BitwiseNot:
            case TokenKind::Increment:
            case TokenKind::Decrement:
            // Grouping
            case TokenKind::LeftParen:
            // Special expressions
            case TokenKind::Match:
            case TokenKind::Dot: // For enum members like .Red
                return true;
            default:
                return false;
            }
        }

        constexpr bool starts_statement() const
        {
            switch (kind)
            {
            case TokenKind::If:
            case TokenKind::While:
            case TokenKind::For:
            case TokenKind::Return:
            case TokenKind::Break:
            case TokenKind::Continue:
            case TokenKind::LeftBrace:
            case TokenKind::Match:
                return true;
            default:
                return starts_expression(); // Expressions can be statements
            }
        }

        constexpr bool starts_declaration() const
        {
            switch (kind)
            {
            case TokenKind::Type:
            case TokenKind::Enum:
            case TokenKind::Fn:
            case TokenKind::Var:
            case TokenKind::Using:
            case TokenKind::Namespace:

            // Modifiers can start declarations
            case TokenKind::Public:
            case TokenKind::Private:
            case TokenKind::Protected:
            case TokenKind::Static:
            case TokenKind::Virtual:
            case TokenKind::Override:
            case TokenKind::Abstract:
            case TokenKind::Extern:
            case TokenKind::Enforced:
            case TokenKind::Inherit:
            case TokenKind::Ref:
            case TokenKind::Async:
                return true;
            default:
                return false;
            }
        }

        std::string_view to_string() const
        {
            return Myre::to_string(kind);
        }

        static TokenKind get_keyword_kind(std::string_view keyword)
        {
            auto it = keyword_map.find(keyword);
            if (it != keyword_map.end())
            {
                return it->second;
            }
            return TokenKind::Identifier; // Not a keyword
        }

    private:
        static const std::unordered_map<std::string_view, TokenKind> keyword_map;
    };

    // Initialize the keyword map with all keywords
    inline const std::unordered_map<std::string_view, TokenKind> Token::keyword_map =
        {
            {"type", TokenKind::Type},
            {"ref", TokenKind::Ref},
            {"enum", TokenKind::Enum},
            {"var", TokenKind::Var},
            {"fn", TokenKind::Fn},
            {"new", TokenKind::New},
            {"return", TokenKind::Return},
            {"if", TokenKind::If},
            {"else", TokenKind::Else},
            {"while", TokenKind::While},
            {"for", TokenKind::For},
            {"match", TokenKind::Match},
            {"case", TokenKind::Case},
            {"break", TokenKind::Break},
            {"continue", TokenKind::Continue},
            {"await", TokenKind::Await},
            {"prop", TokenKind::Prop},
            {"get", TokenKind::Get},
            {"set", TokenKind::Set},
            {"public", TokenKind::Public},
            {"private", TokenKind::Private},
            {"protected", TokenKind::Protected},
            {"static", TokenKind::Static},
            {"virtual", TokenKind::Virtual},
            {"override", TokenKind::Override},
            {"abstract", TokenKind::Abstract},
            {"extern", TokenKind::Extern},
            {"enforced", TokenKind::Enforced},
            {"async", TokenKind::Async},
            {"this", TokenKind::This},
            {"using", TokenKind::Using},
            {"namespace", TokenKind::Namespace},
            {"typeof", TokenKind::Typeof},
            {"sizeof", TokenKind::Sizeof},
            {"where", TokenKind::Where},
            {"inherit", TokenKind::Inherit},
            {"in", TokenKind::In},
            {"at", TokenKind::At},
            {"by", TokenKind::By},
            {"true", TokenKind::BooleanLiteral},
            {"false", TokenKind::BooleanLiteral}};

} // namespace Myre