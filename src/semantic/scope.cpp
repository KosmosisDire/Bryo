#include "semantic/scope.hpp"
#include "semantic/symbol.hpp"
#include <functional>
#include <vector>
#include <algorithm>

namespace Myre {

Symbol* ScopeNode::lookup(const std::string& name) {
    // Look for the name in our children
    auto it = children.find(name);
    if (it != children.end()) {
        return it->second->as_symbol();
    }
    
    // Not found locally, continue up the tree
    return parent ? parent->lookup(name) : nullptr;
}

Symbol* ScopeNode::lookup_local(const std::string& name) {
    // Only look in direct children
    auto it = children.find(name);
    if (it != children.end()) {
        return it->second->as_symbol();
    }
    
    return nullptr;
}

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