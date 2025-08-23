#include "semantic/type.hpp"
#include "semantic/symbol.hpp"

namespace Myre
{

    std::string Type::get_name() const
    {
        return std::visit([](const auto &v) -> std::string
                          {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, PrimitiveType>) {
            switch (v.kind) {
                case PrimitiveType::I32: return "i32";
                case PrimitiveType::I64: return "i64";
                case PrimitiveType::F32: return "f32";
                case PrimitiveType::F64: return "f64";
                case PrimitiveType::Bool: return "bool";
                case PrimitiveType::String: return "string";
                case PrimitiveType::Char: return "char";
                case PrimitiveType::U32: return "u32";
                case PrimitiveType::U64: return "u64";
                case PrimitiveType::I8: return "i8";
                case PrimitiveType::U8: return "u8";
                case PrimitiveType::I16: return "i16";
                case PrimitiveType::U16: return "u16";
                case PrimitiveType::Void: return "void";
                case PrimitiveType::Range: return "Range";
            }
            return "unknown";
        } else if constexpr (std::is_same_v<T, TypeReference>) {
            return v.definition ? v.definition->name() : "unknown";
        } else if constexpr (std::is_same_v<T, ArrayType>) {
            std::string result = v.elementType ? v.elementType->get_name() : "unknown";

            if (v.fixedSize >= 0)
                result += "[" + std::to_string(v.fixedSize) + "]";
            else
                result += "[]";
            return result;
        } else if constexpr (std::is_same_v<T, FunctionType>) {
            return "function";
        } else if constexpr (std::is_same_v<T, UnresolvedType>) {
            return "var:" + std::to_string(v.id);
        }
        return "unknown"; }, value);
    }

    bool Type::is_value_type() const
    {
        return std::visit([](const auto &v) -> bool
                          {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, PrimitiveType>) {
            // All primitives are value types
            return true;
        } else if constexpr (std::is_same_v<T, TypeReference>) {
            if (v.definition) {
                return !v.definition->has_modifier(SymbolModifiers::Ref);
            }
            return true; // Default to value type if definition is missing
        } else {
            return false; // Arrays, functions are reference types
        } }, value);
    }

    TypeLikeSymbol *Type::get_type_symbol() const
    {
        return std::visit([](const auto &v) -> TypeLikeSymbol *
                          {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, TypeReference>) {
            return v.definition;
        } else {
            return nullptr; // Primitives, arrays, functions don't have Symbol definitions
        } }, value);
    }

} // namespace Myre