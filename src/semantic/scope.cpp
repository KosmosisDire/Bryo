#include "semantic/scope.hpp"
#include "semantic/type_definition.hpp"

namespace Myre {

SymbolPtr Scope::lookup(const std::string& name)
{
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

    result += "\n";
    result += indentStr + "{\n";
    
    // List symbols with cleaner formatting
    for (const auto& [symbolName, symbol] : symbols) {
        result += indentStr + "  ";
        
        // Format like test3.myre syntax
        switch (symbol->kind)
        {
            case SymbolKind::Type:
                result += "type " + symbolName;
                // Show type members inline if they exist
                if (auto* typeInfo = std::get_if<TypeInfo>(&symbol->data)) {
                    if (typeInfo->body_scope && !typeInfo->body_scope->symbols.empty()) {
                        result += typeInfo->body_scope->to_string(indent + 1);
                    }
                }
                break;
            case SymbolKind::Function:
                result += "fn " + symbolName;
                // Get function info from variant
                if (auto* funcInfo = std::get_if<FunctionInfo>(&symbol->data)) {
                    if (funcInfo->return_type) {
                        result += ": " + funcInfo->return_type->get_name();
                    }
                    
                    // Show function scope inline if it exists
                    if (funcInfo->body_scope && !funcInfo->body_scope->symbols.empty())
                    {
                        result += funcInfo->body_scope->to_string(indent + 1);
                    }
                }
                break;
            case SymbolKind::Field:
            case SymbolKind::Variable:
            case SymbolKind::Parameter:
            case SymbolKind::Property:
                if (auto* varInfo = std::get_if<VariableInfo>(&symbol->data)) {
                    if (varInfo->type) {
                        result += varInfo->type->get_name() + " " + symbolName;
                    } else {
                        result += "var " + symbolName;
                    }
                } else {
                    result += "unknown var " + symbolName;
                }
                break;
            case SymbolKind::Namespace:

                if (auto* nsInfo = std::get_if<NamespaceInfo>(&symbol->data)) {
                    result += "namespace " + symbolName;
                    if (nsInfo->scope && !nsInfo->scope->symbols.empty()) {
                        result += nsInfo->scope->to_string(indent + 1);
                    }
                }
                else
                {
                    result += "namespace " + symbolName;
                }
                break;
        }
        result += "\n";
    }

    // Also show unnamed child scopes (like block scopes)
    for (const auto& childScope : unnamedChildren) {
        result += indentStr + "  " + childScope->name + " ";
        result += childScope->to_string(indent + 1);
        result += "\n";
    }

    result += indentStr + "}";

    return result;
}

} // namespace Myre