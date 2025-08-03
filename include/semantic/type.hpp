#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>

namespace Myre {

// Forward declarations
struct Type;
struct TypeDefinition;
using TypePtr = std::shared_ptr<Type>;

// ============= Types =============
struct PrimitiveType {
    enum Kind { 
        I32, I64, F32, F64, Bool, String, Char,
        U32, U64, I8, U8, I16, U16,
        Void 
    };
    Kind kind;
    std::weak_ptr<struct TypeDefinition> definition; // Points to System.Int32, System.Boolean, etc.
    const bool isRefType = false; // All primitives are value types
};

struct ArrayType {
    TypePtr elementType;
    int rank = 1; // Number of dimensions
    std::vector<int> fixedSizes; // length must match rank, 0 means any size (aka size is not encoded in the type)
};

// Reference to a defined type (Player, Enemy, etc.)
struct DefinedType {
    std::string name;  // For diagnostics
    std::weak_ptr<struct TypeDefinition> definition;
};

// Generic type instantiation (List<Player>, Map<String, Item>)
struct InstantiatedType {
    std::weak_ptr<struct TypeDefinition> generic_definition;  // List<T>
    std::vector<TypePtr> type_arguments;                      // [Player]
};

struct FunctionType {
    TypePtr returnType;
    std::vector<TypePtr> parameterTypes;
};

// Represents unresolved type references
struct UnresolvedType {
    std::string name;
};

// The Type variant
class Type {
    // Make TypeRegistry a friend so it can create Types
    friend class TypeRegistry;
    
    // Private constructor - only TypeRegistry can create Types
    Type(std::variant<PrimitiveType, ArrayType, DefinedType, InstantiatedType, FunctionType, UnresolvedType> v)
        : value(std::move(v)) {}
    
    // Static factory for TypeRegistry to use with std::make_shared
    template<typename T>
    static TypePtr create(T&& variant_value) {
        struct EnableMakeShared : Type {
            EnableMakeShared(T&& v) : Type(std::forward<T>(v)) {}
        };
        return std::make_shared<EnableMakeShared>(std::forward<T>(variant_value));
    }
    
public:
    std::variant<
        PrimitiveType,
        ArrayType,
        DefinedType,
        InstantiatedType,
        FunctionType,
        UnresolvedType
    > value;
    
    // Type properties
    bool is_value_type() const;
    bool is_reference_type() const { return !is_value_type(); }
    bool is_void() const;
    std::string get_name() const;
    
    // Since we have canonicalization, pointer comparison is sufficient
    bool equals(const TypePtr& other) const { return this == other.get(); }
    
    // Helper to get the underlying type definition (if any)
    std::shared_ptr<TypeDefinition> get_type_definition() const;
};

// ============= Type Implementation =============

// Implementation in type.cpp - needs complete TypeDefinition type

inline bool Type::is_void() const {
    auto* prim = std::get_if<PrimitiveType>(&value);
    return prim && prim->kind == PrimitiveType::Void;
}

inline std::string Type::get_name() const {
    return std::visit([](const auto& v) -> std::string {
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
            }
            return "unknown";
        } else if constexpr (std::is_same_v<T, DefinedType>) {
            return v.name;
        } else if constexpr (std::is_same_v<T, InstantiatedType>) {
            // TODO: Build full generic name like "List<Player>"
            return "generic"; // Placeholder
        } else if constexpr (std::is_same_v<T, ArrayType>) {
            return "array";
        } else if constexpr (std::is_same_v<T, FunctionType>) {
            return "function";
        } else if constexpr (std::is_same_v<T, UnresolvedType>) {
            return v.name;
        }
        return "unknown";
    }, value);
}

// Implementation in type.cpp

} // namespace Myre