#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include "common/symbol_handle.hpp"

namespace Bryo
{

    // Forward declarations
    struct Type;
    class TypeLikeSymbol;
    class ScopeNode;
    struct BaseExprSyntax;
    struct BlockSyntax;
    using TypePtr = std::shared_ptr<Type>;

    
    struct PrimitiveType
    {
        enum Kind
        {
            I8,
            U8,
            I16,
            U16,
            I32,
            U32,
            I64,
            U64,
            F32,
            F64,
            Bool,
            Char,
            Void,
        };

        Kind kind;
    };

    struct ArrayType
    {
        TypePtr elementType;
        int fixedSize = -1; // length must match rank, < 0 means any size (aka size is not encoded in the type)
    };

    // Reference to a defined type (Player, Enemy, etc.)
    struct TypeReference
    {
        TypeLikeSymbol *definition; // Points to TypeSymbol or EnumSymbol
    };

    struct FunctionType
    {
        TypePtr returnType;
        std::vector<TypePtr> parameterTypes;
    };

    struct UnresolvedType
    {
        int id = 0;
    };

    // Represents a type parameter like T in Array<T>
    struct TypeParameter
    {
        std::string name;
        int parameterId;  // Unique ID within the generic context
    };

    // Represents a generic type instantiation like Array<i32>
    struct GenericType
    {
        TypeLikeSymbol *genericDefinition;  // Points to the generic type definition
        std::vector<TypePtr> typeArguments; // The actual type arguments
    };

    struct PointerType
    {
        TypePtr pointeeType;
    };

    class Type
    {
        // Make TypeSystem a friend so it can create Types
        friend class TypeSystem;

        // Private constructor, only TypeSystem can create Types
        Type(std::variant<PrimitiveType, ArrayType, TypeReference, FunctionType, UnresolvedType, TypeParameter, GenericType, PointerType> v)
            : value(std::move(v)) {}

        // Static factory for TypeSystem to use with std::make_shared
        template <typename T>
        static TypePtr create(T &&variant_value)
        {
            struct EnableMakeShared : Type
            {
                EnableMakeShared(T &&v) : Type(std::forward<T>(v)) {}
            };
            return std::make_shared<EnableMakeShared>(std::forward<T>(variant_value));
        }

    public:
        enum class StorageKind {
            Direct,      // Value stored directly (primitives, structs)
            Indirect,    // Value accessed via implicit pointer (classes, arrays)
            Explicit     // Explicit pointer type
        };

        std::variant<
            PrimitiveType,
            ArrayType,
            TypeReference,
            FunctionType,
            UnresolvedType,
            TypeParameter,
            GenericType,
            PointerType
            >
            value;

        // Type properties
        bool is_value_type() const;
        bool is_reference_type() const { return !is_value_type(); }
        bool is_void() const;

        StorageKind get_storage_kind() const
        {
            if (this->is<PointerType>())
                return StorageKind::Explicit;
            if (this->is_reference_type())
                return StorageKind::Indirect;
            return StorageKind::Direct;
        }

        template <typename T>
        bool is() const
        {
            return std::holds_alternative<T>(value);
        }
        template<typename T>
        T &as()
        {
            return std::get<T>(value);
        }
        std::string get_name() const; // Implementation in type.cpp

        // Since we have canonicalization, pointer comparison is sufficient
        bool equals(const TypePtr &other) const { return this == other.get(); }

        // Helper to get the underlying type symbol (if any)
        TypeLikeSymbol *get_type_symbol() const;
    };

    // ============= Type Implementation =============

    inline bool Type::is_void() const
    {
        auto prim = std::get_if<PrimitiveType>(&value);
        return prim && prim->kind == PrimitiveType::Void;
    }

} // namespace Bryo