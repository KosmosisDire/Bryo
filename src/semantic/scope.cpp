#include "semantic/scope.hpp"
#include "semantic/type_definition.hpp"

namespace Myre {

SymbolPtr Scope::lookup(const std::string& name) {
    auto it = symbols.find(name);
    if (it != symbols.end()) {
        return it->second;
    }
    if (auto p = parent.lock()) {
        return p->lookup(name);
    }
    return nullptr;
}

SymbolPtr Scope::lookup_local(const std::string& name) {
    auto it = symbols.find(name);
    return (it != symbols.end()) ? it->second : nullptr;
}

bool Scope::define(SymbolPtr sym) {
    if (symbols.find(sym->name) != symbols.end()) {
        return false; // Already defined
    }
    symbols[sym->name] = sym;
    return true;
}

std::string Scope::get_full_name() const {
    if (kind == Global) return "";
    
    std::string fullName = name;
    auto p = parent.lock();
    while (p && p->kind == Namespace) {
        fullName = p->name + "." + fullName;
        p = p->parent.lock();
    }
    return fullName;
}

std::string Scope::to_string(int indent) const {
    std::string indentStr(indent * 2, ' ');
    std::string result;
    
    // Scope header
    std::string kindStr;
    switch (kind) {
        case Global: kindStr = "Global"; break;
        case Namespace: kindStr = "Namespace"; break;
        case Type: kindStr = "Type"; break;
        case Function: kindStr = "Function"; break;
        case Block: kindStr = "Block"; break;
    }
    
    result += indentStr + kindStr + " Scope";
    if (!name.empty()) {
        result += " '" + name + "'";
    }
    result += " {\n";
    
    // List symbols
    for (const auto& [symbolName, symbol] : symbols) {
        result += indentStr + "  " + symbolName + ": ";
        
        std::string kindStr;
        switch (symbol->kind) {
            case SymbolKind::Type: kindStr = "Type"; break;
            case SymbolKind::Field: kindStr = "Field"; break;
            case SymbolKind::Function: kindStr = "Function"; break;
            case SymbolKind::Variable: kindStr = "Variable"; break;
            case SymbolKind::Parameter: kindStr = "Parameter"; break;
            case SymbolKind::Property: kindStr = "Property"; break;
            case SymbolKind::Namespace: kindStr = "Namespace"; break;
        }
        result += kindStr;
        
        if (symbol->type) {
            result += " : " + symbol->type->get_name();
        }
        result += "\n";
    }
    
    result += indentStr + "}\n";
    return result;
}

std::string Scope::to_string_recursive(int indent) const {
    std::string result = to_string(indent);
    
    // Find and print child scopes by looking through all symbols for nested scopes
    for (const auto& [name, symbol] : symbols) {
        if (symbol->kind == SymbolKind::Type) {
            // Get the type definition and print its member scope
            if (auto typeDef = symbol->type->get_type_definition()) {
                if (typeDef->member_scope && !typeDef->member_scope->symbols.empty()) {
                    result += typeDef->member_scope->to_string_recursive(indent + 1);
                }
            }
        }
    }
    
    return result;
}

} // namespace Myre