#pragma once

#include <string>
#include <unordered_map>
#include "type.hpp"

namespace Bryo
{

    // ============= Type System =============
    class TypeSystem
    {
        // Primitive types (now can point to any type)
        std::unordered_map<std::string, TypePtr> primitives;

        // All type symbols (by fully qualified name)
        std::unordered_map<std::string, TypeLikeSymbol *> type_symbols;

        // Canonical types for arrays, functions, etc.
        std::unordered_map<std::string, TypePtr> canonical_types;

        int next_unresolved_id = 1; // For generating unique IDs for unresolved types

    public:
        TypeSystem();

        // Primitive type access
        TypePtr get_primitive_type(const std::string &name)
        {
            auto it = primitives.find(name);
            return (it != primitives.end()) ? it->second : nullptr;
        }

        TypePtr get_primitive_type(PrimitiveType::Kind kind)
        {
            for (const auto &[name, type] : primitives)
            {
                auto prim = std::get_if<PrimitiveType>(&type->value);
                if (prim && prim->kind == kind)
                    return type;
            }
            return nullptr;
        }

        // Type symbol management
        void register_type_symbol(const std::string &full_name, TypeLikeSymbol *type_symbol)
        {
            type_symbols[full_name] = type_symbol;
        }

        TypeLikeSymbol *lookup_type_symbol(const std::string &full_name)
        {
            auto it = type_symbols.find(full_name);
            return (it != type_symbols.end()) ? it->second : nullptr;
        }

        // Type creation/lookup methods
        TypePtr get_array_type(TypePtr element, int fixedSize);
        TypePtr get_function_type(std::vector<TypePtr> params, TypePtr ret);
        TypePtr get_type_reference(TypeLikeSymbol *type_symbol);
        TypePtr get_unresolved_type();
        
        // Generic type methods
        TypePtr get_type_parameter(const std::string& name, int parameterId);
        TypePtr get_generic_type(TypeLikeSymbol *genericDefinition, const std::vector<TypePtr>& typeArguments);

        TypePtr get_pointer_type(TypePtr pointeeType);

        // Helper method to register any type as a primitive alias
        void register_primitive_alias(const std::string &alias, TypePtr type)
        {
            primitives[alias] = type;
        }

        // Debug/display functions
        std::string to_string(bool include_builtins = true) const;

    private:
        // Helper to create actual primitive types
        TypePtr create_primitive_type(PrimitiveType::Kind kind)
        {
            return Type::create(PrimitiveType{kind});
        }
    };

    // TypeSystem constructor
    inline TypeSystem::TypeSystem()
    {
        // Helper function to register any type under an alias
        auto register_alias = [&](const std::string &alias, TypePtr type)
        {
            primitives[alias] = type;
        };

        // Create basic primitive types first
        auto char_type = create_primitive_type(PrimitiveType::Char);
        auto i8_type = create_primitive_type(PrimitiveType::I8);
        auto i16_type = create_primitive_type(PrimitiveType::I16);
        auto i32_type = create_primitive_type(PrimitiveType::I32);
        auto i64_type = create_primitive_type(PrimitiveType::I64);
        auto u8_type = create_primitive_type(PrimitiveType::U8);
        auto u16_type = create_primitive_type(PrimitiveType::U16);
        auto u32_type = create_primitive_type(PrimitiveType::U32);
        auto u64_type = create_primitive_type(PrimitiveType::U64);
        auto f32_type = create_primitive_type(PrimitiveType::F32);
        auto f64_type = create_primitive_type(PrimitiveType::F64);
        auto bool_type = create_primitive_type(PrimitiveType::Bool);
        auto void_type = create_primitive_type(PrimitiveType::Void);

        // Register basic primitive types
        register_alias("char", char_type);
        register_alias("i8", i8_type);
        register_alias("i16", i16_type);
        register_alias("i32", i32_type);
        register_alias("i64", i64_type);
        register_alias("u8", u8_type);
        register_alias("u16", u16_type);
        register_alias("u32", u32_type);
        register_alias("u64", u64_type);
        register_alias("f32", f32_type);
        register_alias("f64", f64_type);
        register_alias("bool", bool_type);
        register_alias("void", void_type);

        // String as i8* (pointer to char)
        auto string_type = get_pointer_type(char_type);
        register_alias("string", string_type);
    }

    // Type creation with canonicalization
    inline TypePtr TypeSystem::get_array_type(TypePtr element, int fixedSize)
    {
        if (!element)
            return nullptr;

        // Create cache key
        std::string key = element->get_name() + "[" + std::to_string(fixedSize) + "]";

        // Check cache
        auto it = canonical_types.find(key);
        if (it != canonical_types.end())
        {
            return it->second;
        }

        // Create new type
        auto type = Type::create(ArrayType{element, fixedSize});
        canonical_types[key] = type;
        return type;
    }

    inline TypePtr TypeSystem::get_function_type(std::vector<TypePtr> params, TypePtr ret)
    {
        // Create cache key: "fn (i32, bool): string"
        std::string key = "fn (";
        for (size_t i = 0; i < params.size(); ++i)
        {
            if (i > 0)
                key += ",";
            key += params[i]->get_name();
        }
        key += "):" + ret->get_name();

        // Check cache
        auto it = canonical_types.find(key);
        if (it != canonical_types.end())
        {
            return it->second;
        }

        // Create new type
        auto type = Type::create(FunctionType{ret, params});
        canonical_types[key] = type;
        return type;
    }

    inline TypePtr TypeSystem::get_type_reference(TypeLikeSymbol *type_symbol)
    {
        if (!type_symbol)
            return nullptr;

        // Check cache first
        std::string cache_key = "ref:" + type_symbol->get_qualified_name();
        auto it = canonical_types.find(cache_key);
        if (it != canonical_types.end())
        {
            return it->second;
        }

        // Create new type
        auto type = Type::create(TypeReference{type_symbol});
        canonical_types[cache_key] = type;
        return type;
    }

    inline TypePtr TypeSystem::get_unresolved_type()
    {
        return Type::create(UnresolvedType{next_unresolved_id++});
    }

    inline TypePtr TypeSystem::get_type_parameter(const std::string& name, int parameterId)
    {
        return Type::create(TypeParameter{name, parameterId});
    }

    inline TypePtr TypeSystem::get_generic_type(TypeLikeSymbol *genericDefinition, const std::vector<TypePtr>& typeArguments)
    {
        if (!genericDefinition)
            return nullptr;

        // Create cache key: "Array<i32,bool>" 
        std::string key = genericDefinition->get_qualified_name() + "<";
        for (size_t i = 0; i < typeArguments.size(); ++i)
        {
            if (i > 0)
                key += ",";
            key += typeArguments[i]->get_name();
        }
        key += ">";

        // Check cache
        auto it = canonical_types.find(key);
        if (it != canonical_types.end())
        {
            return it->second;
        }

        // Create new generic type
        auto type = Type::create(GenericType{genericDefinition, typeArguments});
        canonical_types[key] = type;
        return type;
    }

    inline TypePtr TypeSystem::get_pointer_type(TypePtr pointeeType)
    {
        if (!pointeeType)
            return nullptr;

        // Create cache key
        std::string key = pointeeType->get_name() + "*";

        // Check cache
        auto it = canonical_types.find(key);
        if (it != canonical_types.end())
        {
            return it->second;
        }

        // Create new pointer type
        auto type = Type::create(PointerType{pointeeType});
        canonical_types[key] = type;
        return type;
    }

    inline std::string TypeSystem::to_string(bool include_builtins) const
    {
        std::string result = "=== TYPE SYSTEM ===\n\n";

        if (include_builtins)
        {
            result += "Primitive Types:\n";
            for (const auto &[name, type] : primitives)
            {
                result += "  " + name + " -> " + type->get_name() + "\n";
            }
            result += "\n";
        }

        result += "Type Symbols:\n";
        bool found_user_types = false;
        for (const auto &[fullName, type_symbol] : type_symbols)
        {
            // Skip built-in types if include_builtins is false
            if (!include_builtins && fullName.find('.') != std::string::npos)
            {
                continue;
            }

            found_user_types = true;

            result += "  type " + fullName + " {\n";
            result += "    is_ref: " + std::string(type_symbol->has_modifier(SymbolModifiers::Ref) ? "true" : "false") + "\n";
            result += "    is_abstract: " + std::string(type_symbol->has_modifier(SymbolModifiers::Abstract) ? "true" : "false") + "\n";
            // Count members by checking if the type symbol is a scope
            size_t member_count = 0;
            if (auto scope = type_symbol->as<Scope>())
            {
                member_count = scope->symbols.size();
            }
            result += "    members: " + std::to_string(member_count) + "\n";
            result += "  }\n";
        }

        if (!found_user_types && !include_builtins)
        {
            result += "  (no user-defined types)\n";
        }

        result += "\nCanonical Types: " + std::to_string(canonical_types.size()) + " cached\n";

        return result;
    }

} // namespace Bryo