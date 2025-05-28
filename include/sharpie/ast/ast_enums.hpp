#pragma once

#include <string>
#include <vector>
#include <memory> // For std::shared_ptr
#include <sstream> // For std::stringstream

namespace Mycelium::Scripting::Lang
{
    // Forward declaration
    struct TokenNode;

    enum class ModifierKind
    {
        Public,
        Private,
        Protected,
        Internal,
        Static,
        Readonly
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
    enum class UnaryOperatorKind
    {
        LogicalNot,
        UnaryPlus,
        UnaryMinus,
        PreIncrement,
        PostIncrement,
        PreDecrement,
        PostDecrement
    };
    enum class BinaryOperatorKind
    {
        Add,
        Subtract,
        Multiply,
        Divide,
        Modulo,
        LogicalAnd,
        LogicalOr,
        Equals,
        NotEquals,
        LessThan,
        GreaterThan,
        LessThanOrEqual,
        GreaterThanOrEqual
    };
    enum class AssignmentOperatorKind
    {
        Assign,
        AddAssign,
        SubtractAssign,
        MultiplyAssign,
        DivideAssign,
        ModuloAssign
    };

    inline std::string to_string(ModifierKind kind)
    {
        switch (kind)
        {
        case ModifierKind::Public: return "public";
        case ModifierKind::Private: return "private";
        case ModifierKind::Protected: return "protected";
        case ModifierKind::Internal: return "internal";
        case ModifierKind::Static: return "static";
        case ModifierKind::Readonly: return "readonly";
        default: return "unknown";
        }
    }

    // Note: TokenNode will need to be defined or forward-declared if this function remains here
    // and ast_enums.hpp is included before ast_base.hpp where TokenNode might be defined.
    // For now, assuming TokenNode will be available.
    inline std::string modifiers_to_string(const std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> &modifiers)
    {
        if (modifiers.empty())
            return "";
        std::stringstream ss;
        for (size_t i = 0; i < modifiers.size(); ++i)
        {
            ss << to_string(modifiers[i].first) << (i == modifiers.size() - 1 ? "" : " ");
        }
        return ss.str();
    }
    inline std::string to_string(LiteralKind kind)
    {
        switch (kind)
        {
        case LiteralKind::Integer: return "integer";
        case LiteralKind::Long: return "long";
        case LiteralKind::Float: return "float";
        case LiteralKind::Double: return "double";
        case LiteralKind::String: return "string";
        case LiteralKind::Char: return "char";
        case LiteralKind::Boolean: return "boolean";
        case LiteralKind::Null: return "null";
        default: return "unknown";
        }
    }
    inline std::string to_string(UnaryOperatorKind op)
    {
        switch (op)
        {
        case UnaryOperatorKind::LogicalNot: return "!";
        case UnaryOperatorKind::UnaryPlus: return "+";
        case UnaryOperatorKind::UnaryMinus: return "-";
        case UnaryOperatorKind::PreIncrement: return "++";
        case UnaryOperatorKind::PostIncrement: return "++";
        case UnaryOperatorKind::PreDecrement: return "--";
        case UnaryOperatorKind::PostDecrement: return "--";
        default: return "unknown";
        }
    }
    inline std::string to_string(BinaryOperatorKind op)
    {
        switch (op)
        {
        case BinaryOperatorKind::Add: return "+";
        case BinaryOperatorKind::Subtract: return "-";
        case BinaryOperatorKind::Multiply: return "*";
        case BinaryOperatorKind::Divide: return "/";
        case BinaryOperatorKind::Modulo: return "%";
        case BinaryOperatorKind::LogicalAnd: return "&&";
        case BinaryOperatorKind::LogicalOr: return "||";
        case BinaryOperatorKind::Equals: return "==";
        case BinaryOperatorKind::NotEquals: return "!=";
        case BinaryOperatorKind::LessThan: return "<";
        case BinaryOperatorKind::GreaterThan: return ">";
        case BinaryOperatorKind::LessThanOrEqual: return "<=";
        case BinaryOperatorKind::GreaterThanOrEqual: return ">=";
        default: return "unknown";
        }
    }
    inline std::string to_string(AssignmentOperatorKind op)
    {
        switch (op)
        {
        case AssignmentOperatorKind::Assign: return "=";
        case AssignmentOperatorKind::AddAssign: return "+=";
        case AssignmentOperatorKind::SubtractAssign: return "-=";
        case AssignmentOperatorKind::MultiplyAssign: return "*=";
        case AssignmentOperatorKind::DivideAssign: return "/=";
        case AssignmentOperatorKind::ModuloAssign: return "%=";
        default: return "unknown";
        }
    }
}
