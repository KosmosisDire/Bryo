#include "symbol.hpp"

namespace Bryo
{
    // Symbol implementation
    std::string Symbol::get_qualified_name() const {
        if (!parent || parent->kind == SymbolKind::Namespace && parent->name.empty()) {
            return name;
        }
        return parent->get_qualified_name() + "." + name;
    }
    
    // ContainerSymbol implementation
    Symbol* ContainerSymbol::add_member(std::unique_ptr<Symbol> symbol) {
        symbol->parent = this;
        Symbol* ptr = symbol.get();
        member_order.push_back(ptr);
        members.emplace(symbol->name, std::move(symbol));
        return ptr;
    }
    
    std::vector<Symbol*> ContainerSymbol::get_member(const std::string& name) {
        std::vector<Symbol*> results;
        auto range = members.equal_range(name);
        for (auto it = range.first; it != range.second; ++it) {
            results.push_back(it->second.get());
        }
        return results;
    }
    
    std::vector<FunctionSymbol*> ContainerSymbol::get_functions(const std::string& name) {
        std::vector<FunctionSymbol*> results;
        auto range = members.equal_range(name);
        for (auto it = range.first; it != range.second; ++it) {
            if (auto func = it->second->as<FunctionSymbol>()) {
                results.push_back(func);
            }
        }
        return results;
    }
    
    // NamespaceSymbol implementation
    NamespaceSymbol::NamespaceSymbol(const std::string& name) {
        kind = SymbolKind::Namespace;
        this->name = name;
        access = Accessibility::Public;
    }

    // BlockSymbol implementation
    BlockSymbol::BlockSymbol(const std::string& debug_name) {
        kind = SymbolKind::Block;
        this->name = debug_name;
        access = Accessibility::Private;  // Blocks are always private
    }

    // TypeSymbol implementation
    TypeSymbol::TypeSymbol(const std::string& name, TypePtr type) {
        kind = SymbolKind::Type;
        this->name = name;
        this->type = type;
    }
    
    bool TypeSymbol::is_value_type() const {
        return type && type->is_value_type();
    }
    
    bool TypeSymbol::is_reference_type() const {
        return type && type->is_reference_type();
    }
    
    // FunctionSymbol implementation
    FunctionSymbol::FunctionSymbol(const std::string& name, TypePtr return_type) {
        kind = SymbolKind::Function;
        this->name = name;
        this->return_type = return_type;
    }
    
    std::string FunctionSymbol::get_mangled_name() const {
        std::string mangled = get_qualified_name();
        for (auto param : parameters) {
            mangled += "_";
            if (param->type) {
                mangled += param->type->get_name();
            }
        }
        return mangled;
    }
    
    bool FunctionSymbol::signature_matches(FunctionSymbol* other) const {
        if (parameters.size() != other->parameters.size()) return false;
        if (return_type != other->return_type) return false;
        
        for (size_t i = 0; i < parameters.size(); i++) {
            if (parameters[i]->type != other->parameters[i]->type) {
                return false;
            }
        }
        return true;
    }

    FunctionSymbol* FunctionSymbol::overridden_method()
    {
        auto* type = parent->as<TypeSymbol>();
        if (!type || !type->base_class) return nullptr;
        
        // If this is a new virtual method (not override)
        if (vtable_index >= type->base_class->vtable.size()) {
            return nullptr;
        }
        
        // Look up what the base has at this index
        return type->base_class->vtable[vtable_index];
    }
    
    // VariableSymbol implementation
    VariableSymbol::VariableSymbol(const std::string& name, TypePtr type) 
        : type(type) {
        this->name = name;
        kind = SymbolKind::Variable;
    }
    
    // FieldSymbol implementation
    FieldSymbol::FieldSymbol(const std::string& name, TypePtr type)
        : VariableSymbol(name, type) {}
    
    // ParameterSymbol implementation
    ParameterSymbol::ParameterSymbol(const std::string& name, TypePtr type, uint32_t idx)
        : VariableSymbol(name, type), index(idx) {}
    
    // LocalSymbol implementation
    LocalSymbol::LocalSymbol(const std::string& name, TypePtr type)
        : VariableSymbol(name, type) {}
    
    // PropertySymbol implementation
    PropertySymbol::PropertySymbol(const std::string& name, TypePtr type) {
        kind = SymbolKind::Property;
        this->name = name;
        this->type = type;
    }
    
    // EnumCaseSymbol implementation
    EnumCaseSymbol::EnumCaseSymbol(const std::string& name) {
        kind = SymbolKind::EnumCase;
        this->name = name;
    }

} // namespace Bryo
