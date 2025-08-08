#include "semantic/type_definition.hpp"
#include "semantic/scope.hpp"
#include "semantic/symbol.hpp"

namespace Myre {

TypeDefinition::TypeDefinition(const std::string& name, const std::string& full_name)
    : name(name), full_name(full_name), modifiers(SymbolModifiers::None) {
    body_scope = std::make_shared<Scope>();
    body_scope->kind = Scope::Type;
    body_scope->name = name;
}

bool TypeDefinition::is_ref_type() const { 
    return (modifiers & SymbolModifiers::Ref) != SymbolModifiers::None; 
}

bool TypeDefinition::is_abstract() const { 
    return (modifiers & SymbolModifiers::Abstract) != SymbolModifiers::None; 
}

bool TypeDefinition::is_generic() const {
    return !type_parameters.empty();
}

SymbolPtr TypeDefinition::lookup_member(const std::string& name) {
    return body_scope->lookup_local(name);
}

bool TypeDefinition::add_member(SymbolPtr member) {
    return body_scope->define(member);
}

} // namespace Myre