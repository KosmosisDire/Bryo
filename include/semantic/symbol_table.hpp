#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include "scope.hpp"
#include "symbol.hpp"
#include "type_system.hpp"

namespace Myre
{

    class SymbolTable
    {
        std::vector<std::unique_ptr<ScopeNode>> symbols;
        std::unordered_map<SymbolHandle, ScopeNode *> symbol_map;

        NamespaceSymbol *globalNamespace = nullptr; // Root namespace
        ScopeNode *current = nullptr;               // Current scope
        TypeSystem type_system;
        std::vector<Symbol *> unresolved_symbols; // Changed from UnscopedSymbol*

    public:
        SymbolTable();

        // Enter named scopes
        NamespaceSymbol *enter_namespace(const std::string &name);
        TypeSymbol *enter_type(const std::string &name);
        EnumSymbol *enter_enum(const std::string &name);
        FunctionSymbol *enter_function(const std::string &name, TypePtr return_type);
        PropertySymbol *enter_property(const std::string &name, TypePtr type);

        // Enter anonymous scope
        BlockScope *enter_block(const std::string &debug_name = "block");

        void set_current_scope(ScopeNode *scope)
        {
            current = scope;
        }

        // Exit current scope
        void exit_scope();

        // Define symbols in current scope
        VariableSymbol *define_variable(const std::string &name, TypePtr type);
        ParameterSymbol *define_parameter(const std::string &name, TypePtr type);
        EnumCaseSymbol *define_enum_case(const std::string &name, std::vector<TypePtr> params = {});

        // Context queries
        NamespaceSymbol *get_current_namespace() const;
        TypeSymbol *get_current_type() const;
        EnumSymbol *get_current_enum() const;
        FunctionSymbol *get_current_function() const;
        PropertySymbol *get_current_property() const;
        ScopeNode *get_current_scope() const { return current; }
        NamespaceSymbol *get_global_namespace() { return globalNamespace; }

        // Lookup
        Symbol *lookup(const std::string &name);
        TypePtr resolve_type_name(const std::string &name);
        TypePtr resolve_type_name(const std::string &name, ScopeNode *scope);
        ScopeNode *lookup_handle(const SymbolHandle &handle) const;

        // Type system access
        TypeSystem &get_type_system() { return type_system; }

        // Unresolved symbols tracking
        const std::vector<Symbol *> &get_unresolved_symbols() const { return unresolved_symbols; }
        void get_all_symbols(std::vector<ScopeNode *> &all_symbols) const
        {
            for (const auto &symbol : symbols)
            {
                all_symbols.push_back(symbol.get());
            }
        }

        const bool is_symbol_unresolved(Symbol *symbol) const { return std::find(unresolved_symbols.begin(), unresolved_symbols.end(), symbol) != unresolved_symbols.end(); }
        const bool has_unresolved_symbols() const { return !unresolved_symbols.empty(); }
        const size_t unresolved_symbols_count() const { return unresolved_symbols.size(); }
        void mark_symbol_resolved(Symbol *symbol);

        // Helper to build fully qualified name in current scope
        std::string build_qualified_name(const std::string &name) const;

        // === Multi-file support ===

        // Merge another symbol table into this one
        // This moves all symbols from 'other' into this table, preserving pointers
        // After merge, 'other' is empty but still valid
        // Returns list of any name conflicts found
        std::vector<std::string> merge(SymbolTable &other);
        void merge_namespace(NamespaceSymbol *target_ns, NamespaceSymbol *source_ns, std::unordered_map<SymbolHandle, SymbolHandle> &handle_remapping, std::vector<std::string> &conflicts);

        // Debug/display functions
        std::string to_string() const;
        
        // Helper to add a child to current scope
        void add_child(const std::string &key, std::unique_ptr<ScopeNode> child);
    private:
    };

} // namespace Myre