#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>

namespace Bryo
{
    struct Type;
    struct TypeSymbol;
    using TypePtr = std::shared_ptr<Type>;
    
    enum class PrimitiveKind : uint32_t {
        Void,
        Bool,
        Char,
        I8, I16, I32, I64,
        U8, U16, U32, U64,
        F32, F64,
    };

    #pragma region Type Variants

    struct PrimitiveType {
        PrimitiveKind kind;
    };
    
    struct PointerType {
        TypePtr pointee;
    };
    
    struct ArrayType {
        TypePtr element;
        int32_t size = -1;  // -1 = dynamic array
    };
    
    struct FunctionType {
        TypePtr returnType;
        std::vector<TypePtr> paramTypes;
    };
    
    struct NamedType {
        TypeSymbol* symbol;  // Points to the type's symbol
    };
    
    struct GenericType {
        TypeSymbol* genericSymbol;
        std::vector<TypePtr> typeArgs;
    };
    
    struct TypeParameter {
        std::string name;
        uint32_t index;
    };
    
    struct UnresolvedType {
        uint32_t id;  // For type inference
    };
    
    #pragma region Type
    
    struct Type {
        std::variant<
            PrimitiveType,
            PointerType,
            ArrayType,
            FunctionType,
            NamedType,
            GenericType,
            TypeParameter,
            UnresolvedType
        > kind;
        
        // Helper methods
        template<typename T>
        bool is() const { return std::holds_alternative<T>(kind); }
        
        template<typename T>
        const T* as() const { 
            if (auto ptr = std::get_if<T>(&kind)) {
            return ptr;
            }
            return nullptr;
        }
        
        bool is_void() const;
        bool is_value_type() const;
        bool is_reference_type() const;
        std::string get_name() const;
        int get_size() const;
        int get_alignment() const;
    };

} // namespace Bryo
