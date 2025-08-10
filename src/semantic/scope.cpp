#include "semantic/scope.hpp"
#include "semantic/symbol.hpp"
#include <functional>

namespace Myre {

// ScopeNode implementation
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

// Context query implementations
Symbol* ScopeNode::get_enclosing_namespace() const {
    const ScopeNode* node = this;
    while (node) {
        if (auto* sym = const_cast<ScopeNode*>(node)->as_symbol()) {
            if (sym->is_namespace()) {
                return sym;
            }
        }
        node = node->parent;
    }
    return nullptr;
}

Symbol* ScopeNode::get_enclosing_type() const {
    const ScopeNode* node = this;
    while (node) {
        if (auto* sym = const_cast<ScopeNode*>(node)->as_symbol()) {
            if (sym->is_type()) {
                return sym;
            }
        }
        node = node->parent;
    }
    return nullptr;
}

Symbol* ScopeNode::get_enclosing_function() const {
    const ScopeNode* node = this;
    while (node) {
        if (auto* sym = const_cast<ScopeNode*>(node)->as_symbol()) {
            if (sym->is_function()) {
                return sym;
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
            if (sym->is_namespace() || sym->is_type()) {
                if (sym->name != "global") {  // Skip the global namespace
                    parts.push_back(sym->name);
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

// Symbol implementation
std::string Symbol::get_qualified_name() const {
    std::vector<std::string> names;
    names.push_back(name);
    
    const ScopeNode* current = parent;
    while (current) {
        if (auto* sym = const_cast<ScopeNode*>(current)->as_symbol()) {
            if (sym->is_namespace() || sym->is_type()) {
                names.push_back(sym->name);
            }
        }
        current = current->parent;
    }
    
    std::string result;
    for (auto it = names.rbegin(); it != names.rend(); ++it) {
        if (!result.empty()) {
            result += ".";
        }
        result += *it;
    }
    
    return result;
}

} // namespace Myre