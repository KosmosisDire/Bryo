#pragma once

#include <string>
#include <unordered_map>
#include "type.hpp"

namespace Myre {

// ============= Type System =============
class TypeSystem {
    // Primitive types
    std::unordered_map<std::string, TypePtr> primitives;
    
    // All type symbols (by fully qualified name)
    std::unordered_map<std::string, TypeLikeSymbol*> type_symbols;
    
    // Canonical types for arrays, functions, etc.
    std::unordered_map<std::string, TypePtr> canonical_types;

    int next_unresolved_id = 1; // For generating unique IDs for unresolved types

public:
    TypeSystem();
    
    // Primitive type access
    TypePtr get_primitive(const std::string& name) {
        auto it = primitives.find(name);
        return (it != primitives.end()) ? it->second : nullptr;
    }
    
    // Type symbol management
    void register_type_symbol(const std::string& full_name, TypeLikeSymbol* type_symbol) {
        type_symbols[full_name] = type_symbol;
    }
    
    TypeLikeSymbol* lookup_type_symbol(const std::string& full_name) {
        auto it = type_symbols.find(full_name);
        return (it != type_symbols.end()) ? it->second : nullptr;
    }
    
    // Type creation/lookup methods
    TypePtr get_array_type(TypePtr element, int rank = 1);
    TypePtr get_generic_instance(TypeLikeSymbol* generic_def, std::vector<TypePtr> args);
    TypePtr get_function_type(TypePtr ret, std::vector<TypePtr> params);
    TypePtr get_type_reference(TypeLikeSymbol* type_symbol);
    TypePtr get_unresolved_type();
    
    
    // Debug/display functions
    std::string to_string(bool include_builtins = true) const;
};

// TypeSystem constructor
inline TypeSystem::TypeSystem() {
    // Create primitive types without Symbol references
    auto create_primitive = [&](const std::string& alias, PrimitiveType::Kind kind) {
        auto primitive_type = Type::create(PrimitiveType{kind});
        primitives[alias] = primitive_type;
    };
    
    // Register all primitive types
    create_primitive("i32", PrimitiveType::I32);
    create_primitive("i64", PrimitiveType::I64);
    create_primitive("f32", PrimitiveType::F32);
    create_primitive("f64", PrimitiveType::F64);
    create_primitive("bool", PrimitiveType::Bool);
    create_primitive("string", PrimitiveType::String);
    create_primitive("char", PrimitiveType::Char);
    create_primitive("u32", PrimitiveType::U32);
    create_primitive("u64", PrimitiveType::U64);
    create_primitive("i8", PrimitiveType::I8);
    create_primitive("u8", PrimitiveType::U8);
    create_primitive("i16", PrimitiveType::I16);
    create_primitive("u16", PrimitiveType::U16);
    create_primitive("void", PrimitiveType::Void);
}


// Type creation with canonicalization
inline TypePtr TypeSystem::get_array_type(TypePtr element, int rank) {
    if (!element) return nullptr;
    
    // Create cache key
    std::string key = "array:" + element->get_name() + ":" + std::to_string(rank);
    
    // Check cache
    auto it = canonical_types.find(key);
    if (it != canonical_types.end()) {
        return it->second;
    }
    
    // Create new type
    auto type = Type::create(ArrayType{element, rank});
    canonical_types[key] = type;
    return type;
}

inline TypePtr TypeSystem::get_generic_instance(TypeLikeSymbol* generic_def, std::vector<TypePtr> args) {
    if (!generic_def) return nullptr;
    
    // Create cache key: "generic:List<i32>"
    std::string key = "generic:" + generic_def->name() + "<";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) key += ",";
        key += args[i]->get_name();
    }
    key += ">";
    
    // Check cache
    auto it = canonical_types.find(key);
    if (it != canonical_types.end()) {
        return it->second;
    }
    
    // Create new type
    auto type = Type::create(GenericInstance{generic_def, args});
    canonical_types[key] = type;
    return type;
}

inline TypePtr TypeSystem::get_function_type(TypePtr ret, std::vector<TypePtr> params) {
    // Create cache key: "func:(i32,bool)->string"
    std::string key = "func:(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) key += ",";
        key += params[i]->get_name();
    }
    key += ")->" + ret->get_name();
    
    // Check cache
    auto it = canonical_types.find(key);
    if (it != canonical_types.end()) {
        return it->second;
    }
    
    // Create new type
    auto type = Type::create(FunctionType{ret, params});
    canonical_types[key] = type;
    return type;
}

inline TypePtr TypeSystem::get_type_reference(TypeLikeSymbol* type_symbol) {
    if (!type_symbol) return nullptr;
    
    // Check cache first
    std::string cache_key = "ref:" + type_symbol->get_qualified_name();
    auto it = canonical_types.find(cache_key);
    if (it != canonical_types.end()) {
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

inline std::string TypeSystem::to_string(bool include_builtins) const {
    std::string result = "=== TYPE SYSTEM ===\n\n";
    
    if (include_builtins) {
        result += "Primitive Types:\n";
        for (const auto& [name, type] : primitives) {
            result += "  " + name + " -> " + type->get_name() + "\n";
        }
        result += "\n";
    }
    
    result += "Type Symbols:\n";
    bool found_user_types = false;
    for (const auto& [fullName, type_symbol] : type_symbols) {
        // Skip built-in types if include_builtins is false
        if (!include_builtins && fullName.find('.') != std::string::npos) {
            continue;
        }
        
        found_user_types = true;
        
        result += "  type " + fullName + " {\n";
        result += "    is_ref: " + std::string(type_symbol->has_modifier(SymbolModifiers::Ref) ? "true" : "false") + "\n";
        result += "    is_abstract: " + std::string(type_symbol->has_modifier(SymbolModifiers::Abstract) ? "true" : "false") + "\n";
        // Count members by checking if the type symbol is a scope
        size_t member_count = 0;
        if (auto* scope = type_symbol->as_scope()) {
            member_count = scope->symbols.size();
        }
        result += "    members: " + std::to_string(member_count) + "\n";
        result += "  }\n";
    }
    
    if (!found_user_types && !include_builtins) {
        result += "  (no user-defined types)\n";
    }
    
    result += "\nCanonical Types: " + std::to_string(canonical_types.size()) + " cached\n";
    
    return result;
}

} // namespace Myre