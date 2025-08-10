#include "semantic/scope.hpp"
#include "semantic/symbol.hpp"
#include <functional>
#include <vector>
#include <algorithm>

namespace Myre {

// ============= ScopeNode Safe Casting Helpers =============

Scope* ScopeNode::as_scope() {
    return dynamic_cast<Scope*>(this);
}

const Scope* ScopeNode::as_scope() const {
    return dynamic_cast<const Scope*>(this);
}

bool ScopeNode::is_scope() const {
    return dynamic_cast<const Scope*>(this) != nullptr;
}

// ============= Scope Lookup Methods =============

Symbol* Scope::lookup(const std::string& name) {
    // Search locally first
    auto* result = lookup_local(name);
    if (result) return result;
    
    // Get the ScopeNode that this Scope belongs to and walk up parent chain
    ScopeNode* node = as_scope_node();
    node = node->parent;  // Start with parent
    
    while (node) {
        // Check if this parent is also a Scope and do full recursive lookup
        if (auto* parent_scope = node->as_scope()) {
            result = parent_scope->lookup(name); 
            if (result) return result;
        }
        node = node->parent;
    }
    return nullptr;
}

Symbol* Scope::lookup_local(const std::string& name) {
    // Only look in direct symbols
    auto it = symbols.find(name);
    if (it != symbols.end()) {
        return it->second->as_symbol();
    }
    return nullptr;
}

void Scope::add_symbol(const std::string& name, std::unique_ptr<ScopeNode> symbol) {
    // Set parent relationship
    ScopeNode* node = as_scope_node();
    symbol->parent = node;
    symbols[name] = std::move(symbol);
}

// ============= ScopeNode Navigation Methods =============

NamespaceSymbol* ScopeNode::get_enclosing_namespace() const {
    const ScopeNode* node = this;
    while (node) {
        if (auto* sym = const_cast<ScopeNode*>(node)->as_symbol()) {
            if (auto* ns = sym->as<NamespaceSymbol>()) {
                return ns;
            }
        }
        node = node->parent;
    }
    return nullptr;
}

TypeSymbol* ScopeNode::get_enclosing_type() const {
    const ScopeNode* node = this;
    while (node) {
        if (auto* sym = const_cast<ScopeNode*>(node)->as_symbol()) {
            if (auto* type = sym->as<TypeSymbol>()) {
                return type;
            }
        }
        node = node->parent;
    }
    return nullptr;
}

EnumSymbol* ScopeNode::get_enclosing_enum() const {
    const ScopeNode* node = this;
    while (node) {
        if (auto* sym = const_cast<ScopeNode*>(node)->as_symbol()) {
            if (auto* enum_sym = sym->as<EnumSymbol>()) {
                return enum_sym;
            }
        }
        node = node->parent;
    }
    return nullptr;
}

FunctionSymbol* ScopeNode::get_enclosing_function() const {
    const ScopeNode* node = this;
    while (node) {
        if (auto* sym = const_cast<ScopeNode*>(node)->as_symbol()) {
            if (auto* func = sym->as<FunctionSymbol>()) {
                return func;
            }
        }
        node = node->parent;
    }
    return nullptr;
}

TypeLikeSymbol* ScopeNode::get_enclosing_type_like() const {
    const ScopeNode* node = this;
    while (node) {
        if (auto* sym = const_cast<ScopeNode*>(node)->as_symbol()) {
            if (auto* type_like = sym->as<TypeLikeSymbol>()) {
                return type_like;
            }
        }
        node = node->parent;
    }
    return nullptr;
}

std::string ScopeNode::build_qualified_name(const std::string& name) const {
    std::vector<std::string> parts;
    
    // Walk up to collect namespace/type names
    const ScopeNode* node = this;
    while (node) {
        if (auto* sym = const_cast<ScopeNode*>(node)->as_symbol()) {
            // Include namespaces and type-like symbols in qualified name
            if (sym->is<NamespaceSymbol>() || sym->is<TypeLikeSymbol>()) {
                if (sym->name() != "global") {  // Skip the global namespace
                    parts.push_back(sym->name());
                }
            }
        }
        node = node->parent;
    }
    
    // Build the qualified name
    std::string result;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        if (!result.empty()) {
            result += ".";
        }
        result += *it;
    }
    
    if (!result.empty()) {
        result += ".";
    }
    result += name;
    
    return result;
}


} // namespace Myre