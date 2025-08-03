#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include "symbol.hpp"
#include "type.hpp"

namespace Myre {

// ============= Scopes =============
class Scope {
public:
    enum Kind {
        Global,
        Namespace,
        Type,
        Function,
        Block
    };
    
    Kind kind;
    std::string name; // For named scopes (namespace, type, etc.)
    std::weak_ptr<Scope> parent;
    std::unordered_map<std::string, SymbolPtr> symbols;
    SymbolPtr scopeDefinition;
    
    SymbolPtr lookup(const std::string& name);
    SymbolPtr lookup_local(const std::string& name);
    bool define(SymbolPtr sym);
    std::string get_full_name() const;
    std::string to_string(int indent = 0) const;
    std::string to_string_recursive(int indent = 0) const;
};

using ScopePtr = std::shared_ptr<Scope>;

} // namespace Myre