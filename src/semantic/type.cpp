#include "semantic/type.hpp"
#include "semantic/symbol.hpp"

namespace Myre
{

    std::string Type::get_name() const
    {
        return std::visit([](const auto &v) -> std::string
                          {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, PrimitiveType>)
        {
            switch (v.kind) {
                case PrimitiveType::I32: return "i32";
                case PrimitiveType::I64: return "i64";
                case PrimitiveType::F32: return "f32";
                case PrimitiveType::F64: return "f64";
                case PrimitiveType::Bool: return "bool";
                case PrimitiveType::Char: return "char";
                case PrimitiveType::U32: return "u32";
                case PrimitiveType::U64: return "u64";
                case PrimitiveType::I8: return "i8";
                case PrimitiveType::U8: return "u8";
                case PrimitiveType::I16: return "i16";
                case PrimitiveType::U16: return "u16";
                case PrimitiveType::Void: return "void";
            }
            return "unknown";
        }
        else if constexpr (std::is_same_v<T, TypeReference>)
        {
            return v.definition ? v.definition->name() : "unknown";
        }
        else if constexpr (std::is_same_v<T, ArrayType>)
        {
            std::string result = v.elementType ? v.elementType->get_name() : "unknown";

            if (v.fixedSize >= 0)
                result += "[" + std::to_string(v.fixedSize) + "]";
            else
                result += "[]";
            return result;
        }
        else if constexpr (std::is_same_v<T, FunctionType>)
        {
            return "function";
        }
        else if constexpr (std::is_same_v<T, UnresolvedType>)
        {
            return "var:" + std::to_string(v.id);
        }
        else if constexpr (std::is_same_v<T, TypeParameter>)
        {
            return v.name + ":" + std::to_string(v.parameterId);
        }
        else if constexpr (std::is_same_v<T, GenericType>)
        {
            std::string result = v.genericDefinition ? v.genericDefinition->name() : "unknown";
            result += "<";
            for (size_t i = 0; i < v.typeArguments.size(); ++i) {
                if (i > 0) result += ",";
                result += v.typeArguments[i] ? v.typeArguments[i]->get_name() : "unknown";
            }
            result += ">";
            return result;
        }
        else if constexpr (std::is_same_v<T, PointerType>)
        {
            return (v.pointeeType ? v.pointeeType->get_name() : "unknown") + "*";
        }
        else
        {
            return "unknown";
        } }, value);
    }

    bool Type::is_value_type() const
    {
        // Primitives are value types
        if (std::holds_alternative<PrimitiveType>(value))
        {
            return true;
        }

        // TODO: Arrays are value types (change to reference later)
        if (std::holds_alternative<ArrayType>(value))
        {
            return true;
        }

        // User-defined types check their symbol
        if (auto typeRef = std::get_if<TypeReference>(&value))
        {
            if (auto typeSymbol = typeRef->definition)
            {
                // Structs are value types, classes are reference types
                return !typeSymbol->has_modifier(SymbolModifiers::Ref);
            }
        }

        // Pointers are value types (the pointer itself is a value)
        if (std::holds_alternative<PointerType>(value))
        {
            return true;
        }

        // Everything else defaults to value type
        return true;
    }
    
    TypeLikeSymbol *Type::get_type_symbol() const
    {
        return std::visit([](const auto &v) -> TypeLikeSymbol *
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, TypeReference>)
            {
                return v.definition;
            }
            else if constexpr (std::is_same_v<T, GenericType>)
            {
                return v.genericDefinition;
            }
            else
            {
                return nullptr; // Primitives, arrays, functions, type parameters don't have Symbol definitions
            }
        }, 
        value);
    }

} // namespace Myre