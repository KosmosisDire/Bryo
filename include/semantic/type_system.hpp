#pragma once

#include <string>
#include <unordered_map>
#include "type.hpp"
#include "type_definition.hpp"

namespace Myre {

// ============= Type System =============
class TypeSystem {
    // Primitive types
    std::unordered_map<std::string, TypePtr> primitives;
    
    // All type definitions (by fully qualified name)
    std::unordered_map<std::string, TypeDefinitionPtr> definitions;
    
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
    
    // Type definition management
    void register_type_definition(const std::string& full_name, TypeDefinitionPtr definition) {
        definitions[full_name] = definition;
    }
    
    TypeDefinitionPtr lookup_type_definition(const std::string& full_name) {
        auto it = definitions.find(full_name);

        return (it != definitions.end()) ? it->second : nullptr;
    }
    
    // Type creation/lookup methods
    TypePtr get_or_create_array_type(TypePtr element);
    TypePtr get_or_create_function_type(TypePtr ret, std::vector<TypePtr> params);
    TypePtr get_type_reference(const std::string& full_name);
    TypePtr get_unresolved_type();
    
    
    // Debug/display functions
    std::string to_string(bool include_builtins = true) const;
};

// TypeSystem constructor
inline TypeSystem::TypeSystem() {
    // Create TypeDefinitions for built-in value types (like C#'s System.Int32, etc.)
    auto create_builtin_type = [&](const std::string& alias, const std::string& full_name, PrimitiveType::Kind kind) {
        // Extract the simple name from the full name (System.Int32 -> Int32)
        std::string simple_name = full_name;
        auto dot_pos = full_name.find_last_of('.');
        if (dot_pos != std::string::npos) {
            simple_name = full_name.substr(dot_pos + 1);
        }
        
        // Create TypeDefinition for the built-in type
        auto type_def = std::make_shared<TypeDefinition>(simple_name, full_name);
        type_def->modifiers = SymbolModifiers::None; // Value types by default
        
        // Register the type definition
        definitions[full_name] = type_def;
        
        // Create primitive type reference pointing to this definition
        auto primitive_type = Type::create(PrimitiveType{kind, type_def, false});
        primitives[alias] = primitive_type;
        
        // Also register under alias for lookup (i32 -> System.Int32 TypeDefinition)
        definitions[alias] = type_def;
    };
    
    // Register all primitive types as aliases to System types
    create_builtin_type("i32", "System.Int32", PrimitiveType::I32);
    create_builtin_type("i64", "System.Int64", PrimitiveType::I64);
    create_builtin_type("f32", "System.Single", PrimitiveType::F32);
    create_builtin_type("f64", "System.Double", PrimitiveType::F64);
    create_builtin_type("bool", "System.Boolean", PrimitiveType::Bool);
    create_builtin_type("string", "System.String", PrimitiveType::String);
    create_builtin_type("char", "System.Char", PrimitiveType::Char);
    create_builtin_type("u32", "System.UInt32", PrimitiveType::U32);
    create_builtin_type("u64", "System.UInt64", PrimitiveType::U64);
    create_builtin_type("i8", "System.SByte", PrimitiveType::I8);
    create_builtin_type("u8", "System.Byte", PrimitiveType::U8);
    create_builtin_type("i16", "System.Int16", PrimitiveType::I16);
    create_builtin_type("u16", "System.UInt16", PrimitiveType::U16);
    create_builtin_type("void", "System.Void", PrimitiveType::Void);
}


// Type creation with canonicalization
inline TypePtr TypeSystem::get_or_create_array_type(TypePtr element) {
    if (!element) return nullptr;
    
    // Create cache key
    std::string key = "array:" + element->get_name();
    
    // Check cache
    auto it = canonical_types.find(key);
    if (it != canonical_types.end()) {
        return it->second;
    }
    
    // Create new type
    auto type = Type::create(ArrayType{element, 1});
    canonical_types[key] = type;
    return type;
}

inline TypePtr TypeSystem::get_or_create_function_type(TypePtr ret, std::vector<TypePtr> params) {
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

inline TypePtr TypeSystem::get_type_reference(const std::string& full_name) {
    if (auto def = lookup_type_definition(full_name)) {
        // Check cache first
        auto it = canonical_types.find("defined:" + full_name);
        if (it != canonical_types.end()) {
            return it->second;
        }
        
        // Create new type
        auto type = Type::create(DefinedType{def->name, def});
        canonical_types["defined:" + full_name] = type;
        return type;
    }
    return nullptr;
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
    
    result += "Type Definitions:\n";
    bool found_user_types = false;
    for (const auto& [fullName, typeDef] : definitions) {
        // Skip built-in types if include_builtins is false
        if (!include_builtins) {
            // Check the TypeDefinition's full_name, not the key
            if (typeDef->full_name.find('.') != std::string::npos) {
                continue;
            }
        }
        
        found_user_types = true;
        
        // Use a cleaner format more like the scope display
        std::string type_display_name = typeDef->name;
        if (typeDef->full_name != typeDef->name) {
            type_display_name = typeDef->full_name;
        }
        
        result += "  type " + type_display_name + " {\n";
        result += "    is_ref: " + std::string(typeDef->is_ref_type() ? "true" : "false") + "\n";
        result += "    is_abstract: " + std::string(typeDef->is_abstract() ? "true" : "false") + "\n";
        result += "    is_generic: " + std::string(typeDef->is_generic() ? "true" : "false") + "\n";
        
        if (!typeDef->type_parameters.empty()) {
            result += "    type_parameters: [";
            for (size_t i = 0; i < typeDef->type_parameters.size(); ++i) {
                if (i > 0) result += ", ";
                result += typeDef->type_parameters[i];
            }
            result += "]\n";
        }
        
        result += "    members: " + std::to_string(typeDef->body_scope ? typeDef->body_scope->symbols.size() : 0) + "\n";
        result += "  }\n";
    }
    
    if (!found_user_types && !include_builtins) {
        result += "  (no user-defined types)\n";
    }
    
    result += "\nCanonical Types: " + std::to_string(canonical_types.size()) + " cached\n";
    
    return result;
}

} // namespace Myre