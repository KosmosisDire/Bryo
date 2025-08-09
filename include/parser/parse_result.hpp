#pragma once

#include "ast/ast.hpp"

namespace Myre
{
    // Simplified ParseResult - just success or failure
    // ErrorNodes are created in the AST for failures
    template<typename T>
    struct ParseResult
    {        
        T* node = nullptr;
        
        // Factory methods
        static ParseResult ok(T* n) { return ParseResult{n}; }
        static ParseResult fail() { return ParseResult{nullptr}; }
        
        // Query methods
        bool success() const { return node != nullptr; }
        bool failed() const { return node == nullptr; }
        
        // Convenient operators
        operator bool() const { return success(); }
        T* operator->() const { return node; }
        T* get() const { return node; }

        template <typename AsType>
        ParseResult<AsType> as() const {
            if (node) {
                return ParseResult<AsType>::ok(static_cast<AsType*>(node));
            }
            return ParseResult<AsType>::fail();
        }
        
        // Get node with specific type or create error node
        template<typename Parser>
        T* get_or_error(const char* msg, Parser* parser) {
            if (node) return node;
            return reinterpret_cast<T*>(parser->create_error_node(msg));
        }
    };
    
    // Helper struct for variable patterns
    struct TypedIdentifier {
        TypeNameNode* type;
        IdentifierNode* name;
        
        static ParseResult<TypedIdentifier> ok(TypeNameNode* t, IdentifierNode* n) {
            auto* result = new TypedIdentifier{t, n};
            return ParseResult<TypedIdentifier>{reinterpret_cast<TypedIdentifier*>(result)};
        }
    };
    
    // Variable pattern types
    enum class VarPattern {
        TYPED,      // Type identifier
        VAR,        // var identifier  
        IDENTIFIER  // just identifier (existing variable)
    };
    
    struct VariablePattern {
        VarPattern pattern;
        TypeNameNode* type = nullptr;
        IdentifierNode* name = nullptr;
        TokenNode* varKeyword = nullptr;
    };
}