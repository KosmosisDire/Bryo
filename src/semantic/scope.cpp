#include "semantic/scope.hpp"
#include "semantic/symbol.hpp"
#include <functional>
#include <vector>
#include <algorithm>

namespace Myre
{

    // ============= Scope Lookup Methods =============

    Symbol *Scope::lookup(const std::string &name)
    {
        // Check if name contains dots (qualified name)
        size_t dot_pos = name.find('.');

        if (dot_pos == std::string::npos)
        {
            // Simple name - use existing logic
            // Search locally first
            auto result = lookup_local(name);
            if (result)
                return result;

            // Get the ScopeNode that this Scope belongs to and walk up parent chain
            ScopeNode *node = as_scope_node();
            node = node->parent; // Start with parent

            while (node)
            {
                // Check if this parent is also a Scope and do full recursive lookup
                if (auto parent_scope = node->as<Scope>())
                {
                    result = parent_scope->lookup(name);
                    if (result)
                        return result;
                }
                node = node->parent;
            }
            return nullptr;
        }

        // Qualified name - split and resolve step by step
        std::string first_part = name.substr(0, dot_pos);
        std::string remaining = name.substr(dot_pos + 1);

        // Use the normal lookup logic to find the first part
        Symbol *current = lookup(first_part);
        if (!current)
            return nullptr;

        // Now walk through the remaining parts
        while (!remaining.empty())
        {
            // Check if current symbol has a scope
            Scope *current_scope = current->as<Scope>();
            if (!current_scope)
            {
                // Current symbol doesn't have a scope, can't continue
                return nullptr;
            }

            // Find the next dot or use the whole remaining string
            dot_pos = remaining.find('.');
            std::string next_part;

            if (dot_pos == std::string::npos)
            {
                // Last part
                next_part = remaining;
                remaining.clear();
            }
            else
            {
                // More parts to come
                next_part = remaining.substr(0, dot_pos);
                remaining = remaining.substr(dot_pos + 1);
            }

            // Look up the next part in the current scope (local only)
            current = current_scope->lookup_local(next_part);
            if (!current)
                return nullptr;
        }

        return current;
    }

    Symbol *Scope::lookup_local(const std::string &name)
    {
        // Only look in direct symbols
        auto it = symbols.find(name);
        if (it != symbols.end())
        {
            return it->second->as<Symbol>();
        }
        return nullptr;
    }

    std::vector<FunctionSymbol *> Scope::lookup_functions_local(const std::string &name)
    {
        std::vector<FunctionSymbol *> local_overloads;

        // Only check this scope, don't walk up parents
        auto it = symbols.find(name);
        if (it != symbols.end())
        {
            if (auto group = it->second->as<FunctionGroupSymbol>())
            {
                auto overloads = group->get_overloads();
                local_overloads.insert(local_overloads.end(), overloads.begin(), overloads.end());
            }
        }

        return local_overloads;
    }

    std::vector<FunctionSymbol *> Scope::lookup_functions(const std::string &name)
    {
        std::vector<FunctionSymbol *> all_overloads;

        // Check this scope using local lookup
        auto local_overloads = lookup_functions_local(name);
        all_overloads.insert(all_overloads.end(), local_overloads.begin(), local_overloads.end());

        // Walk up parent scopes, skipping non-scope nodes
        ScopeNode *node = as_scope_node()->parent;
        while (node)
        {
            if (auto parent_scope = node->as<Scope>())
            {
                auto parent_overloads = parent_scope->lookup_functions(name);
                all_overloads.insert(all_overloads.end(), parent_overloads.begin(), parent_overloads.end());
                break; // Found a scope parent, let recursion handle the rest
            }
            node = node->parent;
        }

        return all_overloads;
    }

    void Scope::add_symbol(const std::string &name, ScopeNode *symbol)
    {
        // Set parent relationship
        ScopeNode *node = as_scope_node();
        symbol->parent = node;
        symbols[name] = symbol;
    }

    // ============= ScopeNode Navigation Methods =============

    NamespaceSymbol *ScopeNode::get_enclosing_namespace() const
    {
        const ScopeNode *node = this;
        while (node)
        {
            if (auto sym = node->as<Symbol>())
            {
                if (auto ns = sym->as<NamespaceSymbol>())
                {
                    return const_cast<NamespaceSymbol *>(ns);
                }
            }
            node = node->parent;
        }
        return nullptr;
    }

    TypeSymbol *ScopeNode::get_enclosing_type() const
    {
        const ScopeNode *node = this;
        while (node)
        {
            if (auto sym = node->as<Symbol>())
            {
                if (auto type = sym->as<TypeSymbol>())
                {
                    return const_cast<TypeSymbol *>(type);
                }
            }
            node = node->parent;
        }
        return nullptr;
    }

    EnumSymbol *ScopeNode::get_enclosing_enum() const
    {
        const ScopeNode *node = this;
        while (node)
        {
            if (auto sym = node->as<Symbol>())
            {
                if (auto enum_sym = sym->as<EnumSymbol>())
                {
                    return const_cast<EnumSymbol *>(enum_sym);
                }
            }
            node = node->parent;
        }
        return nullptr;
    }

    FunctionSymbol *ScopeNode::get_enclosing_function() const
    {
        const ScopeNode *node = this;
        while (node)
        {
            if (auto sym = node->as<Symbol>())
            {
                if (auto func = sym->as<FunctionSymbol>())
                {
                    return const_cast<FunctionSymbol *>(func);
                }
            }
            node = node->parent;
        }
        return nullptr;
    }

    PropertySymbol *ScopeNode::get_enclosing_property() const
    {
        const ScopeNode *node = this;
        while (node)
        {
            if (auto sym = node->as<Symbol>())
            {
                if (auto prop = sym->as<PropertySymbol>())
                {
                    return const_cast<PropertySymbol *>(prop);
                }
            }
            node = node->parent;
        }
        return nullptr;
    }

    TypeLikeSymbol *ScopeNode::get_enclosing_type_like() const
    {
        const ScopeNode *node = this;
        while (node)
        {
            if (auto sym = node->as<Symbol>())
            {
                if (auto type_like = sym->as<TypeLikeSymbol>())
                {
                    return const_cast<TypeLikeSymbol *>(type_like);
                }
            }
            node = node->parent;
        }
        return nullptr;
    }

    std::string ScopeNode::build_qualified_name(const std::string &name) const
    {
        std::vector<std::string> parts;

        // Walk up to collect namespace/type names
        const ScopeNode *node = this;
        while (node)
        {
            if (auto sym = node->as<Symbol>())
            {
                // Include namespaces and type-like symbols in qualified name
                if (sym->is<NamespaceSymbol>() || sym->is<TypeLikeSymbol>())
                {
                    if (sym->name() != "global")
                    { // Skip the global namespace
                        parts.push_back(sym->name());
                    }
                }
            }
            node = node->parent;
        }

        // Build the qualified name
        std::string result;
        for (auto it = parts.rbegin(); it != parts.rend(); ++it)
        {
            if (!result.empty())
            {
                result += ".";
            }
            result += *it;
        }

        if (!result.empty())
        {
            result += ".";
        }
        result += name;

        return result;
    }

} // namespace Myre