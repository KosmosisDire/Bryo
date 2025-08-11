#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>

namespace Myre {

// Forward declarations
struct Type;
class TypeLikeSymbol;
class ScopeNode;
struct ExpressionNode;
struct TypeNameNode;
struct BlockStatementNode;
using TypePtr = std::shared_ptr<Type>;

struct PrimitiveType {
    enum Kind { 
        I32, I64, F32, F64, Bool, String, Char,
        U32, U64, I8, U8, I16, U16,
        Void, Range
    };
    Kind kind;
};

struct ArrayType {
    TypePtr elementType;
    int rank = 1; // Number of dimensions
    std::vector<int> fixedSizes; // length must match rank, 0 means any size (aka size is not encoded in the type)
};

// Reference to a defined type (Player, Enemy, etc.)
struct TypeReference {
    TypeLikeSymbol* definition;  // Points to TypeSymbol or EnumSymbol
};

// Generic type instantiation (List<Player>, Map<String, Item>)
struct GenericInstance {
    TypeLikeSymbol* generic_definition;  // List<T>
    std::vector<TypePtr> type_arguments;  // [Player]
};

struct FunctionType {
    TypePtr return_type;
    std::vector<TypePtr> parameter_types;
};

// Represents unresolved type references
struct UnresolvedType {
    int id = 0; // Unique ID for this unresolved type
    ExpressionNode* initializer = nullptr;   // For variable initializers, property backing field init, and arrow properties
    TypeNameNode* type_name = nullptr;
    ScopeNode* defining_scope = nullptr;     // Can be Symbol or BlockScope
    BlockStatementNode* body = nullptr;      // For function return type inference and property getter blocks

    inline bool can_infer() const { 
        return (initializer != nullptr || type_name != nullptr || body != nullptr) && defining_scope != nullptr; 
    }
};

class Type {
    // Make TypeSystem a friend so it can create Types
    friend class TypeSystem;
    
    // Private constructor, only TypeSystem can create Types
    Type(std::variant<PrimitiveType, ArrayType, TypeReference, GenericInstance, FunctionType, UnresolvedType> v)
        : value(std::move(v)) {}
    
    // Static factory for TypeSystem to use with std::make_shared
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
        TypeReference,
        GenericInstance,
        FunctionType,
        UnresolvedType
    > value;
    
    // Type properties
    bool is_value_type() const;
    bool is_reference_type() const { return !is_value_type(); }
    bool is_void() const;
    std::string get_name() const; // Implementation in type.cpp
    
    // Since we have canonicalization, pointer comparison is sufficient
    bool equals(const TypePtr& other) const { return this == other.get(); }
    
    // Helper to get the underlying type symbol (if any)
    TypeLikeSymbol* get_type_symbol() const;
};

// ============= Type Implementation =============

inline bool Type::is_void() const {
    auto* prim = std::get_if<PrimitiveType>(&value);
    return prim && prim->kind == PrimitiveType::Void;
}

} // namespace Myre