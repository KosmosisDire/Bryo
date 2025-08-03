#pragma once

#include <string>
#include <unordered_map>
#include "type.hpp"
#include "type_definition.hpp"

namespace Myre {

// ============= Type Registry =============
class TypeRegistry {
    std::unordered_map<std::string, TypePtr> primitiveTypes;
    std::unordered_map<std::string, TypeDefinitionPtr> type_definitions; // Full name -> TypeDefinition
    
    // Canonical type caches to ensure unique instances
    std::unordered_map<std::string, TypePtr> canonical_array_types;    // "Player[],1" -> TypePtr
    std::unordered_map<std::string, TypePtr> canonical_function_types; // "i32,bool->string" -> TypePtr
    std::unordered_map<std::string, TypePtr> canonical_generic_types;  // "List<Player>" -> TypePtr
    
public:
    TypeRegistry();
    
    TypePtr get_primitive(const std::string& name) {
        auto it = primitiveTypes.find(name);
        return (it != primitiveTypes.end()) ? it->second : nullptr;
    }
    
    void register_type_definition(const std::string& fullName, TypeDefinitionPtr definition) {
        type_definitions[fullName] = definition;
    }
    
    TypeDefinitionPtr lookup_type_definition(const std::string& fullName) {
        auto it = type_definitions.find(fullName);
        return (it != type_definitions.end()) ? it->second : nullptr;
    }
    
    // Create a Type reference for a type definition
    TypePtr get_type_reference(const std::string& fullName) {
        if (auto def = lookup_type_definition(fullName)) {
            return get_defined_type(def);
        }
        return nullptr;
    }
    
    // Type creation methods with canonicalization
    TypePtr get_defined_type(TypeDefinitionPtr definition);
    TypePtr get_array_type(TypePtr elementType, int rank = 1);
    TypePtr get_instantiated_type(TypeDefinitionPtr generic_def, std::vector<TypePtr> type_args);
    TypePtr get_function_type(TypePtr returnType, std::vector<TypePtr> parameterTypes);
    TypePtr get_unresolved_type(const std::string& name);
    
    // Resolve an unresolved type using current namespace context
    TypePtr resolve_type(const UnresolvedType& unresolved, const std::string& currentNamespace);
    
    // Debug/display functions
    std::string to_string() const;
};

// TypeRegistry constructor
inline TypeRegistry::TypeRegistry() {
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
        type_definitions[full_name] = type_def;
        
        // Create primitive type reference pointing to this definition
        auto primitive_type = Type::create(PrimitiveType{kind, type_def, false});
        primitiveTypes[alias] = primitive_type;
        
        // Also register under full name for lookup
        type_definitions[alias] = type_def;
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

inline TypePtr TypeRegistry::resolve_type(const UnresolvedType& unresolved, const std::string& currentNamespace) {
    // Try as primitive first
    if (auto type = get_primitive(unresolved.name)) {
        return type;
    }
    
    // Try with current namespace
    if (!currentNamespace.empty()) {
        std::string nameWithNamespace = currentNamespace + "." + unresolved.name;
        if (auto type = get_type_reference(nameWithNamespace)) {
            return type;
        }
    }
    
    // Try as global name
    if (auto type = get_type_reference(unresolved.name)) {
        return type;
    }
    
    // Try common namespaces (like System)
    std::string systemName = "System." + unresolved.name;
    if (auto type = get_type_reference(systemName)) {
        return type;
    }
    
    return nullptr;
}

// Type creation with canonicalization
inline TypePtr TypeRegistry::get_defined_type(TypeDefinitionPtr definition) {
    if (!definition) return nullptr;
    
    // Check cache first
    auto it = canonical_generic_types.find(definition->full_name);
    if (it != canonical_generic_types.end()) {
        return it->second;
    }
    
    // Create new type
    auto type = Type::create(DefinedType{definition->name, definition});
    canonical_generic_types[definition->full_name] = type;
    return type;
}

inline TypePtr TypeRegistry::get_array_type(TypePtr elementType, int rank) {
    if (!elementType) return nullptr;
    
    // Create cache key
    std::string key = elementType->get_name() + "[" + std::to_string(rank) + "]";
    
    // Check cache
    auto it = canonical_array_types.find(key);
    if (it != canonical_array_types.end()) {
        return it->second;
    }
    
    // Create new type
    auto type = Type::create(ArrayType{elementType, rank});
    canonical_array_types[key] = type;
    return type;
}

inline TypePtr TypeRegistry::get_instantiated_type(TypeDefinitionPtr generic_def, std::vector<TypePtr> type_args) {
    if (!generic_def) return nullptr;
    
    // Create cache key: "List<Player,Item>"
    std::string key = generic_def->name + "<";
    for (size_t i = 0; i < type_args.size(); ++i) {
        if (i > 0) key += ",";
        key += type_args[i]->get_name();
    }
    key += ">";
    
    // Check cache
    auto it = canonical_generic_types.find(key);
    if (it != canonical_generic_types.end()) {
        return it->second;
    }
    
    // Create new type
    auto type = Type::create(InstantiatedType{generic_def, type_args});
    canonical_generic_types[key] = type;
    return type;
}

inline TypePtr TypeRegistry::get_function_type(TypePtr returnType, std::vector<TypePtr> parameterTypes) {
    // Create cache key: "(i32,bool)->string"
    std::string key = "(";
    for (size_t i = 0; i < parameterTypes.size(); ++i) {
        if (i > 0) key += ",";
        key += parameterTypes[i]->get_name();
    }
    key += ")->" + returnType->get_name();
    
    // Check cache
    auto it = canonical_function_types.find(key);
    if (it != canonical_function_types.end()) {
        return it->second;
    }
    
    // Create new type
    auto type = Type::create(FunctionType{returnType, parameterTypes});
    canonical_function_types[key] = type;
    return type;
}

inline TypePtr TypeRegistry::get_unresolved_type(const std::string& name) {
    // For unresolved types, we don't cache them since they're temporary
    return Type::create(UnresolvedType{name});
}

inline std::string TypeRegistry::to_string() const {
    std::string result = "=== TYPE REGISTRY ===\n\n";
    
    result += "Primitive Types:\n";
    for (const auto& [name, type] : primitiveTypes) {
        result += "  " + name + " -> " + type->get_name() + "\n";
    }
    
    result += "\nType Definitions:\n";
    for (const auto& [fullName, typeDef] : type_definitions) {
        result += "  " + fullName + " {\n";
        result += "    name: " + typeDef->name + "\n";
        result += "    full_name: " + typeDef->full_name + "\n";
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
        result += "    members: " + std::to_string(typeDef->member_scope->symbols.size()) + "\n";
        result += "  }\n";
    }
    
    result += "\nCanonical Array Types: " + std::to_string(canonical_array_types.size()) + " cached\n";
    result += "Canonical Function Types: " + std::to_string(canonical_function_types.size()) + " cached\n";
    result += "Canonical Generic Types: " + std::to_string(canonical_generic_types.size()) + " cached\n";
    
    return result;
}

} // namespace Myre