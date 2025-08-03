#pragma once

#include "semantic/type.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace Myre
{

    // === SYMBOL DEFINITION ===

    enum class SymbolKind
    {
        Variable,
        Function,
        Type,
        Parameter,
        Constant
    };

    struct Symbol
    {
        std::string name;
        SymbolKind kind;
        Type *type;     // Non-owning pointer
        void *ast_node; // Optional pointer to AST node

        Symbol(const std::string &n, SymbolKind k, Type *t, void *node = nullptr)
            : name(n), kind(k), type(t), ast_node(node) {}
    };

    // === SIMPLE MUTABLE SYMBOL TABLE ===

    class SymbolTable
    {
    private:
        // Stack of scopes - each scope is a hash map
        std::vector<std::unordered_map<std::string, Symbol>> scope_stack_;

        // Type storage - owns all type objects
        std::vector<std::unique_ptr<Type>> owned_types_;

        // Primitive type cache
        std::unordered_map<std::string, PrimitiveType *> primitive_types_;

        // Struct type registry
        std::unordered_map<std::string, StructType *> struct_types_;

    public:
        SymbolTable();

        // === SCOPE MANAGEMENT ===

        void enter_scope();
        void exit_scope();
        size_t current_scope_depth() const { return scope_stack_.size(); }

        // === SYMBOL OPERATIONS ===

        // Add symbol to current scope
        bool add_symbol(const std::string &name, SymbolKind kind, Type *type, void *ast_node = nullptr);

        // Lookup symbol (searches all scopes from innermost to outermost)
        Symbol *lookup(const std::string &name);
        const Symbol *lookup(const std::string &name) const;

        // Lookup in current scope only
        Symbol *lookup_current_scope(const std::string &name);

        // Member function lookup helper
        Symbol *lookup_member_function(const std::string &type_name, const std::string &method_name);

        // === TYPE MANAGEMENT ===

        // Get or create primitive types
        PrimitiveType *get_primitive_type(const std::string &name);

        // Create new types (symbol table owns them)
        StructType *create_struct_type(const std::string &name);
        FunctionType *create_function_type(Type *return_type, const std::vector<Type *> &params, bool varargs = false);
        PointerType *create_pointer_type(Type *pointee);
        ArrayType *create_array_type(Type *element, size_t size);

        // Type lookup
        Type *lookup_type(const std::string &name);

        // Register struct type with its methods
        void register_struct_type(StructType *struct_type);

        // === UTILITY ===

        // Get all symbols in current scope
        std::vector<Symbol *> get_current_scope_symbols();

        // Debug print
        void dump_symbols() const;

    private:
        // Initialize built-in primitive types
        void initialize_primitive_types();

        // Helper to create type and register ownership
        template <typename T, typename... Args>
        T *create_type(Args &&...args)
        {
            auto type = std::make_unique<T>(std::forward<Args>(args)...);
            T *ptr = type.get();
            owned_types_.push_back(std::move(type));
            return ptr;
        }
    };

    // === COMMON PRIMITIVE TYPE SIZES ===

    struct PrimitiveTypeInfo
    {
        const char *name;
        size_t size;
    };

    inline const PrimitiveTypeInfo PRIMITIVE_TYPES[] = {
        {"void", 0},
        {"bool", 1},
        {"i8", 1},
        {"u8", 1},
        {"i16", 2},
        {"u16", 2},
        {"i32", 4},
        {"u32", 4},
        {"i64", 8},
        {"u64", 8},
        {"f32", 4},
        {"f64", 8},
        {"char", 1},
    };

} // namespace Myre