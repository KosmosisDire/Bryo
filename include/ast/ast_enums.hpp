#pragma once

#include <string>

namespace Mycelium::Scripting::Lang
{
    // Corresponds to BfToken
    enum class TokenKind : uint16_t
    {
        None,

        // Keywords
        Abstract, As, Asm, Base, Bool, Break, Byte, Case, Catch, Char,
        Class, Const, Continue, Default, Delegate, Delete, Do, Double,
        Else, Enum, Extern, False, Field, Finally, Float, Fn, For, Foreach,
        Get, If, Implicit, In, Inline, Int, Interface, Internal, Is, Long,
        Match, Mut, Namespace, New, Null, Open, Operator, Out, Override,
        Params, Passdown, Private, Prop, Protected, Public, Readonly, Ref,
        Return, Sealed, Set, Short, SizeOf, Static, String, Struct, Switch,
        This, Throw, True, Try, Type, TypeOf, Uint, Ulong, Underscore,
        Ushort, Using, Value, Var, Virtual, Void, Volatile, While,

        // Operators and Punctuation
        Assign,
        Equals,
        NotEquals,
        LessThan,
        GreaterThan,
        LessThanOrEqual,
        GreaterThanOrEqual,
        Plus,
        Minus,
        Asterisk,
        Slash,
        Percent,
        Ampersand,
        Bar,
        Caret,
        Bang,
        Tilde,
        Question,
        Colon,
        Semicolon,
        Comma,
        Dot,
        OpenParen,
        CloseParen,
        OpenBrace,
        CloseBrace,
        OpenBracket,
        CloseBracket,
        OpenAngle,
        CloseAngle,

        // Compound Operators
        PlusAssign,
        MinusAssign,
        MultiplyAssign,
        DivideAssign,
        ModuloAssign,
        AndAssign,
        OrAssign,
        XorAssign,
        LeftShift,
        LeftShiftAssign,
        RightShift,
        RightShiftAssign,
        Coalesce,
        CoalesceAssign,
        LogicalAnd,
        LogicalOr,
        Increment,
        Decrement,
        Arrow,
        FatArrow,
        DotDot,
        DotDotEquals,
        ColonColon,

        // Literals and Identifiers
        Identifier,
        IntegerLiteral,
        FloatLiteral,
        StringLiteral,
        CharLiteral,

        // Misc
        EndOfFile,
        Invalid,
    };

    enum class UnaryOperatorKind
    {
        None,
        Plus,
        Minus,
        Not,
        BitwiseNot,
        PreIncrement,
        PreDecrement,
        PostIncrement,
        PostDecrement,
        AddressOf,
        Dereference,
    };

    enum class BinaryOperatorKind
    {
        None,
        Add,
        Subtract,
        Multiply,
        Divide,
        Modulo,
        Equals,
        NotEquals,
        LessThan,
        GreaterThan,
        LessThanOrEqual,
        GreaterThanOrEqual,
        LogicalAnd,
        LogicalOr,
        BitwiseAnd,
        BitwiseOr,
        BitwiseXor,
        LeftShift,
        RightShift,
        Coalesce,
    };

    enum class AssignmentOperatorKind
    {
        None,
        Assign,
        Add,
        Subtract,
        Multiply,
        Divide,
        Modulo,
        And,
        Or,
        Xor,
        LeftShift,
        RightShift,
        Coalesce,
    };

    enum class ModifierKind
    {
        Public,
        Private,
        Protected,
        Static,
        Mut,
        Ref,
        Virtual,
        Override,
        Abstract,
        Extern,
        Open,
        Passdown,
        Inline,
    };

    enum class LiteralKind
    {
        Integer,
        Long,
        Float,
        Double,
        String,
        Char,
        Boolean,
        Null,
    };

    // Helper functions
    const char* to_string(TokenKind kind);
    const char* to_string(UnaryOperatorKind kind);
    const char* to_string(BinaryOperatorKind kind);
    const char* to_string(AssignmentOperatorKind kind);

} // namespace Mycelium::Scripting::Lang