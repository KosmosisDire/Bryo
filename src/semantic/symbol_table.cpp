#include "semantic/symbol_table.hpp"
#include <algorithm>
#include <sstream>
#include <functional>
#include <stdexcept>

namespace Myre
{

    SymbolTable::SymbolTable()
    {
    }

    void SymbolTable::add_child(const std::string &key, std::unique_ptr<ScopeNode> child)
    {
        // Get raw pointer before moving the unique_ptr
        ScopeNode *child_ptr = child.get();
        symbols.push_back(std::move(child));
        symbol_map[child_ptr->handle] = child_ptr;

        if (!current && !globalNamespace && child_ptr->is<NamespaceSymbol>())
        {
            globalNamespace = child_ptr->as<NamespaceSymbol>();
            current = globalNamespace;
            return;
        }

        // Current scope must be able to contain symbols (must be a Scope)
        auto *current_scope = current->as<Scope>();
        if (!current_scope)
        {
            throw std::runtime_error("Cannot add child to non-scope node");
        }
        current_scope->add_symbol(key, child_ptr);
    }

    NamespaceSymbol *SymbolTable::enter_namespace(const std::string &name)
    {
        auto sym = std::make_unique<NamespaceSymbol>();
        sym->set_name(name);
        sym->set_access(AccessLevel::Public);
        NamespaceSymbol *ptr = sym.get();
        add_child(name, std::move(sym));
        current = ptr;
        return ptr;
    }

    TypeSymbol *SymbolTable::enter_type(const std::string &name)
    {
        auto sym = std::make_unique<TypeSymbol>();
        sym->set_name(name);
        sym->set_access(AccessLevel::Public);
        TypeSymbol *ptr = sym.get();

        // Register type in type system
        std::string full_name = ptr->get_qualified_name();
        type_system.register_type_symbol(full_name, ptr);

        add_child(name, std::move(sym));
        current = ptr;
        return ptr;
    }

    EnumSymbol *SymbolTable::enter_enum(const std::string &name)
    {
        auto sym = std::make_unique<EnumSymbol>();
        sym->set_name(name);
        sym->set_access(AccessLevel::Public);
        EnumSymbol *ptr = sym.get();

        // Register enum in type system
        std::string full_name = ptr->get_qualified_name();
        type_system.register_type_symbol(full_name, ptr);

        add_child(name, std::move(sym));
        current = ptr;
        return ptr;
    }

    FunctionSymbol *SymbolTable::enter_function(const std::string &name, TypePtr return_type,
                                                std::vector<TypePtr> params)
    {
        auto sym = std::make_unique<FunctionSymbol>();
        sym->set_name(name);
        sym->set_return_type(return_type);
        sym->set_parameter_types(std::move(params));
        sym->set_access(AccessLevel::Private);
        FunctionSymbol *ptr = sym.get();

        // Check if this function has an unresolved return type and needs inference
        if (return_type && std::holds_alternative<UnresolvedType>(return_type->value))
        {
            unresolved_symbols.push_back(ptr);
        }

        add_child(name, std::move(sym));
        current = ptr;
        return ptr;
    }

    PropertySymbol *SymbolTable::enter_property(const std::string &name, TypePtr type)
    {
        auto sym = std::make_unique<PropertySymbol>();
        sym->set_name(name);
        sym->set_type(type);
        sym->set_access(AccessLevel::Private);
        PropertySymbol *ptr = sym.get();

        // Check if this property has an unresolved type and needs inference
        if (type && std::holds_alternative<UnresolvedType>(type->value))
        {
            unresolved_symbols.push_back(ptr);
        }

        add_child(name, std::move(sym));
        current = ptr;
        return ptr;
    }

    BlockScope *SymbolTable::enter_block(const std::string &debug_name)
    {
        auto block = std::make_unique<BlockScope>(debug_name);
        BlockScope *ptr = block.get();
        std::string key = "$block_" + std::to_string(block->handle.id);
        add_child(key, std::move(block));
        current = ptr;
        return ptr;
    }

    void SymbolTable::exit_scope()
    {
        if (current->parent)
        {
            current = current->parent;
        }
    }

    VariableSymbol *SymbolTable::define_variable(const std::string &name, TypePtr type)
    {
        auto sym = std::make_unique<VariableSymbol>();
        sym->set_name(name);
        sym->set_type(type);
        sym->set_access(AccessLevel::Private);
        VariableSymbol *ptr = sym.get();

        // Check if this symbol has an unresolved type and needs inference
        if (type && std::holds_alternative<UnresolvedType>(type->value))
        {
            unresolved_symbols.push_back(ptr);
        }

        add_child(name, std::move(sym));
        return ptr;
    }

    ParameterSymbol *SymbolTable::define_parameter(const std::string &name, TypePtr type)
    {
        auto sym = std::make_unique<ParameterSymbol>();
        sym->set_name(name);
        sym->set_type(type);
        sym->set_access(AccessLevel::Private);
        ParameterSymbol *ptr = sym.get();

        // Check if this symbol has an unresolved type and needs inference
        if (type && std::holds_alternative<UnresolvedType>(type->value))
        {
            unresolved_symbols.push_back(ptr);
        }

        add_child(name, std::move(sym));
        return ptr;
    }

    FieldSymbol *SymbolTable::define_field(const std::string &name, TypePtr type)
    {
        auto sym = std::make_unique<FieldSymbol>();
        sym->set_name(name);
        sym->set_type(type);
        sym->set_access(AccessLevel::Private);
        FieldSymbol *ptr = sym.get();

        // Check if this symbol has an unresolved type and needs inference
        if (type && std::holds_alternative<UnresolvedType>(type->value))
        {
            unresolved_symbols.push_back(ptr);
        }

        add_child(name, std::move(sym));
        return ptr;
    }

    EnumCaseSymbol *SymbolTable::define_enum_case(const std::string &name, std::vector<TypePtr> params)
    {
        auto sym = std::make_unique<EnumCaseSymbol>();
        sym->set_name(name);
        sym->set_params(std::move(params));
        sym->set_access(AccessLevel::Public);
        EnumCaseSymbol *ptr = sym.get();
        add_child(name, std::move(sym));
        return ptr;
    }

    NamespaceSymbol *SymbolTable::get_current_namespace() const
    {
        return current->get_enclosing_namespace();
    }

    TypeSymbol *SymbolTable::get_current_type() const
    {
        return current->get_enclosing_type();
    }

    EnumSymbol *SymbolTable::get_current_enum() const
    {
        return current->get_enclosing_enum();
    }

    FunctionSymbol *SymbolTable::get_current_function() const
    {
        return current->get_enclosing_function();
    }

    PropertySymbol *SymbolTable::get_current_property() const
    {
        return current->get_enclosing_property();
    }

    Symbol *SymbolTable::lookup(const std::string &name)
    {
        // Current scope must be able to lookup (must be a Scope)
        auto *current_scope = current->as<Scope>();
        if (!current_scope)
        {
            return nullptr; // Can't lookup from non-scope nodes
        }
        return current_scope->lookup(name);
    }

    TypePtr SymbolTable::resolve_type_name(const std::string &type_name)
    {
        return resolve_type_name(type_name, current);
    }

    TypePtr SymbolTable::resolve_type_name(const std::string &type_name, ScopeNode *scope)
    {
        // First check if it is a built-in type
        auto prim = type_system.get_primitive(type_name);
        if (prim)
        {
            return prim;
        }

        // Look up the symbol starting from the given scope
        auto *scope_container = scope->as<Scope>();
        if (!scope_container)
        {
            return type_system.get_unresolved_type();
        }

        auto *symbol = scope_container->lookup(type_name);
        if (!symbol)
        {
            return type_system.get_unresolved_type();
        }

        // Check if it's a type-like symbol (Type or Enum)
        if (auto *type_like = symbol->as<TypeLikeSymbol>())
        {
            return type_system.get_type_reference(type_like);
        }

        return type_system.get_unresolved_type();
    }

    ScopeNode *SymbolTable::lookup_handle(const SymbolHandle &handle) const
    {
        auto it = symbol_map.find(handle);
        if (it != symbol_map.end())
        {
            return it->second;
        }
        return nullptr;
    }

    void SymbolTable::mark_symbol_resolved(Symbol *symbol)
    {
        auto it = std::find(unresolved_symbols.begin(), unresolved_symbols.end(), symbol);
        if (it != unresolved_symbols.end())
        {
            unresolved_symbols.erase(it);
        }
    }

    std::string SymbolTable::build_qualified_name(const std::string &name) const
    {
        return current->build_qualified_name(name);
    }

    // === Multi-file support implementation ===
    std::vector<std::string> SymbolTable::merge(SymbolTable &other)
    {
        std::vector<std::string> conflicts;
        std::unordered_map<SymbolHandle, SymbolHandle> handle_remapping;

        // Step 1: Transfer ownership of all symbols
        for (auto &symbol : other.symbols)
        {
            symbols.push_back(std::move(symbol));
        }

        // Step 2: Update symbol_map with all the transferred symbols
        for (auto &[handle, ptr] : other.symbol_map)
        {
            symbol_map[handle] = ptr;
        }

        // Step 3: Handle global namespace merging
        if (globalNamespace && other.globalNamespace)
        {
            // Both have global namespaces - merge them
            merge_namespace(globalNamespace, other.globalNamespace, handle_remapping, conflicts);
        }
        else if (!globalNamespace && other.globalNamespace)
        {
            // We don't have one, adopt theirs
            globalNamespace = other.globalNamespace;
        }

        // Step 4: Update symbol_map for remapped handles
        // Instead of erasing, point old handles to the merged symbols
        for (const auto &[old_handle, new_handle] : handle_remapping)
        {
            symbol_map[old_handle] = symbol_map[new_handle];
        }

        // Step 5: Update all parent pointers based on remapping
        for (auto &symbol : symbols)
        {
            if (symbol && symbol->parent && handle_remapping.count(symbol->parent->handle))
            {
                symbol->parent = symbol_map[handle_remapping[symbol->parent->handle]];
            }
        }

        // Step 6: Merge unresolved symbols
        unresolved_symbols.insert(unresolved_symbols.end(),
                                  other.unresolved_symbols.begin(),
                                  other.unresolved_symbols.end());

        // Clear other's state
        other.symbols.clear();
        other.symbol_map.clear();
        other.unresolved_symbols.clear();
        other.current = nullptr;
        other.globalNamespace = nullptr;

        return conflicts;
    }

    void SymbolTable::merge_namespace(NamespaceSymbol *target_ns,
                                      NamespaceSymbol *source_ns,
                                      std::unordered_map<SymbolHandle, SymbolHandle> &handle_remapping,
                                      std::vector<std::string> &conflicts)
    {
        if (!target_ns || !source_ns)
            return;

        // Record that source namespace is being merged into target
        handle_remapping[source_ns->handle] = target_ns->handle;

        auto *target_scope = target_ns->as<Scope>();
        auto *source_scope = source_ns->as<Scope>();
        if (!target_scope || !source_scope)
            return;

        // Move all symbols from source to target
        for (auto &[name, source_symbol] : source_scope->symbols)
        {
            auto target_it = target_scope->symbols.find(name);

            if (target_it != target_scope->symbols.end())
            {
                // Both namespaces have this symbol
                auto *target_symbol = target_it->second;

                // If both are namespaces, merge them recursively
                if (auto *target_child_ns = target_symbol->as<NamespaceSymbol>())
                {
                    if (auto *source_child_ns = source_symbol->as<NamespaceSymbol>())
                    {
                        merge_namespace(target_child_ns, source_child_ns,
                                        handle_remapping, conflicts);
                        continue;
                    }
                }

                // For now, just report other conflicts
                conflicts.push_back("Symbol conflict: " + name + " already exists");
            }
            else
            {
                // No conflict - add symbol to target namespace
                source_symbol->parent = target_ns;
                target_scope->symbols[name] = source_symbol;
            }
        }
    }

    std::string SymbolTable::to_string() const
    {
        std::stringstream ss;
        ss << "=== SYMBOL TABLE ===\n";

        // Print current context
        if (auto *ns = get_current_namespace())
        {
            if (ns->name() != "global")
            {
                ss << "Current Namespace: " << ns->name() << "\n";
            }
        }

        if (auto *type = get_current_type())
        {
            ss << "Current Type: " << type->name() << "\n";
        }

        if (auto *func = get_current_function())
        {
            ss << "Current Function: " << func->name() << "\n";
        }

        ss << "\nScope Hierarchy:\n";

        // Recursive function to print the tree
        std::function<void(ScopeNode *, int)> print_tree = [&](ScopeNode *node, int indent)
        {
            std::string indent_str(indent * 2, ' ');

            if (auto *sym = node->as<Symbol>())
            {
                // Print symbol info
                ss << indent_str;
                ss << sym->kind_name() << " " << sym->name();

                // Add type-specific info
                if (auto *typed_symbol = sym->as<TypedSymbol>())
                {
                    if (typed_symbol->type())
                    {
                        if (auto *func = sym->as<FunctionSymbol>())
                        {
                            ss << " -> " << typed_symbol->type()->get_name();
                        }
                        else
                        {
                            ss << ": " << typed_symbol->type()->get_name();
                        }
                    }
                }
                else if (auto *type = sym->as<TypeSymbol>())
                {
                    if (type->is_ref_type())
                        ss << " (ref)";
                    if (type->is_abstract())
                        ss << " (abstract)";
                }
                else if (auto *enum_sym = sym->as<EnumSymbol>())
                {
                    ss << " (enum)";
                }
                else if (auto *case_sym = sym->as<EnumCaseSymbol>())
                {
                    if (case_sym->is_tagged())
                    {
                        ss << "(";
                        bool first = true;
                        for (const auto &type : case_sym->params())
                        {
                            if (!first)
                                ss << ", ";
                            ss << type->get_name();
                            first = false;
                        }
                        ss << ")";
                    }
                }

                // Add brackets if this node has symbols (is a scope)
                if (auto *scope = node->as<Scope>())
                {
                    if (!scope->symbols.empty())
                    {
                        ss << " {\n";
                        // Print symbols
                        for (const auto &[key, child] : scope->symbols)
                        {
                            print_tree(child, indent + 1);
                        }
                        ss << indent_str << "}";
                    }
                }
                ss << "\n";
            }
            else if (auto *block = node->as<BlockScope>())
            {
                ss << indent_str << "block (" << block->debug_name << ")";

                // Add brackets if this block has symbols
                if (!block->symbols.empty())
                {
                    ss << " {\n";
                    // Print symbols
                    for (const auto &[key, child] : block->symbols)
                    {
                        print_tree(child, indent + 1);
                    }
                    ss << indent_str << "}";
                }
                ss << "\n";
            }
        };

        print_tree(globalNamespace, 0);

        // Display unresolved symbols
        if (!unresolved_symbols.empty())
        {
            ss << "\nUnresolved Symbols:\n";
            for (const auto *sym : unresolved_symbols)
            {
                ss << " - " << sym->name() << "\n";
            }
        }

        return ss.str();
    }

} // namespace Myre