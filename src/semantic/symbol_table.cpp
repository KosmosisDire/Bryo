#include "semantic/symbol_table.hpp"
#include <algorithm>
#include <sstream>
#include <functional>

namespace Myre {

SymbolTable::SymbolTable() {
    // Create global namespace symbol
    global_symbol = std::make_unique<NamespaceSymbol>();
    global_symbol->set_name("global");
    global_symbol->set_access(AccessLevel::Public);
    current = global_symbol.get();
}

void SymbolTable::add_child(const std::string& key, std::unique_ptr<Symbol> child) {
    child->parent = current;
    current->children[key] = std::move(child);
}

void SymbolTable::add_block_child(const std::string& key, std::unique_ptr<BlockScope> child) {
    child->parent = current;
    current->children[key] = std::move(child);
}

NamespaceSymbol* SymbolTable::enter_namespace(const std::string& name) {
    auto sym = std::make_unique<NamespaceSymbol>();
    sym->set_name(name);
    sym->set_access(AccessLevel::Public);
    NamespaceSymbol* ptr = sym.get();
    add_child(name, std::move(sym));
    current = ptr;
    return ptr;
}

TypeSymbol* SymbolTable::enter_type(const std::string& name) {
    auto sym = std::make_unique<TypeSymbol>();
    sym->set_name(name);
    sym->set_access(AccessLevel::Public);
    TypeSymbol* ptr = sym.get();
    
    // Register type in type system
    std::string full_name = ptr->get_qualified_name();
    type_system.register_type_symbol(full_name, ptr);
    
    add_child(name, std::move(sym));
    current = ptr;
    return ptr;
}

EnumSymbol* SymbolTable::enter_enum(const std::string& name) {
    auto sym = std::make_unique<EnumSymbol>();
    sym->set_name(name);
    sym->set_access(AccessLevel::Public);
    EnumSymbol* ptr = sym.get();
    
    // Register enum in type system
    std::string full_name = ptr->get_qualified_name();
    type_system.register_type_symbol(full_name, ptr);
    
    add_child(name, std::move(sym));
    current = ptr;
    return ptr;
}

FunctionSymbol* SymbolTable::enter_function(const std::string& name, TypePtr return_type, 
                                    std::vector<TypePtr> params) {
    auto sym = std::make_unique<FunctionSymbol>();
    sym->set_name(name);
    sym->set_return_type(return_type);
    sym->set_parameter_types(std::move(params));
    sym->set_access(AccessLevel::Private);
    FunctionSymbol* ptr = sym.get();
    add_child(name, std::move(sym));
    current = ptr;
    return ptr;
}

BlockScope* SymbolTable::enter_block(const std::string& debug_name) {
    auto block = std::make_unique<BlockScope>(debug_name);
    BlockScope* ptr = block.get();
    std::string key = "$block_" + std::to_string(next_block_id++);
    add_block_child(key, std::move(block));
    current = ptr;
    return ptr;
}

void SymbolTable::exit_scope() {
    if (current->parent) {
        current = current->parent;
    }
}

VariableSymbol* SymbolTable::define_variable(const std::string& name, TypePtr type) {
    auto sym = std::make_unique<VariableSymbol>();
    sym->set_name(name);
    sym->set_type(type);
    sym->set_access(AccessLevel::Private);
    VariableSymbol* ptr = sym.get();
    
    // Check if this symbol has an unresolved type and needs inference
    if (type && std::holds_alternative<UnresolvedType>(type->value)) {
        unresolved_symbols.push_back(ptr);
    }
    
    add_child(name, std::move(sym));
    return ptr;
}

ParameterSymbol* SymbolTable::define_parameter(const std::string& name, TypePtr type) {
    auto sym = std::make_unique<ParameterSymbol>();
    sym->set_name(name);
    sym->set_type(type);
    sym->set_access(AccessLevel::Private);
    ParameterSymbol* ptr = sym.get();
    
    // Check if this symbol has an unresolved type and needs inference
    if (type && std::holds_alternative<UnresolvedType>(type->value)) {
        unresolved_symbols.push_back(ptr);
    }
    
    add_child(name, std::move(sym));
    return ptr;
}

FieldSymbol* SymbolTable::define_field(const std::string& name, TypePtr type) {
    auto sym = std::make_unique<FieldSymbol>();
    sym->set_name(name);
    sym->set_type(type);
    sym->set_access(AccessLevel::Private);
    FieldSymbol* ptr = sym.get();
    
    // Check if this symbol has an unresolved type and needs inference
    if (type && std::holds_alternative<UnresolvedType>(type->value)) {
        unresolved_symbols.push_back(ptr);
    }
    
    add_child(name, std::move(sym));
    return ptr;
}

PropertySymbol* SymbolTable::define_property(const std::string& name, TypePtr type) {
    auto sym = std::make_unique<PropertySymbol>();
    sym->set_name(name);
    sym->set_type(type);
    sym->set_access(AccessLevel::Private);
    PropertySymbol* ptr = sym.get();
    
    // Check if this symbol has an unresolved type and needs inference
    if (type && std::holds_alternative<UnresolvedType>(type->value)) {
        unresolved_symbols.push_back(ptr);
    }
    
    add_child(name, std::move(sym));
    return ptr;
}

EnumCaseSymbol* SymbolTable::define_enum_case(const std::string& name, std::vector<TypePtr> associated_types) {
    auto sym = std::make_unique<EnumCaseSymbol>();
    sym->set_name(name);
    sym->set_associated_types(std::move(associated_types));
    sym->set_access(AccessLevel::Public);
    EnumCaseSymbol* ptr = sym.get();
    add_child(name, std::move(sym));
    return ptr;
}

NamespaceSymbol* SymbolTable::get_current_namespace() const {
    return current->get_enclosing_namespace();
}

TypeSymbol* SymbolTable::get_current_type() const {
    return current->get_enclosing_type();
}

EnumSymbol* SymbolTable::get_current_enum() const {
    return current->get_enclosing_enum();
}

FunctionSymbol* SymbolTable::get_current_function() const {
    return current->get_enclosing_function();
}

Symbol* SymbolTable::lookup(const std::string& name) {
    return current->lookup(name);
}

TypePtr SymbolTable::resolve_type_name(const std::string& type_name) {
    return resolve_type_name(type_name, current);
}

TypePtr SymbolTable::resolve_type_name(const std::string& type_name, ScopeNode* scope) {
    // First check if it is a built-in type
    auto prim = type_system.get_primitive(type_name);
    if (prim) {
        return prim;
    }
    
    // Look up the symbol starting from the given scope
    auto* symbol = scope->lookup(type_name);
    if (!symbol) {
        return type_system.get_unresolved_type();
    }
    
    // Check if it's a type-like symbol (Type or Enum)
    if (auto* type_like = symbol->as<TypeLikeSymbol>()) {
        return type_system.get_type_reference(type_like);
    }
    
    return type_system.get_unresolved_type();
}

void SymbolTable::mark_symbol_resolved(Symbol* symbol) {
    auto it = std::find(unresolved_symbols.begin(), unresolved_symbols.end(), symbol);
    if (it != unresolved_symbols.end()) {
        unresolved_symbols.erase(it);
    }
}

std::string SymbolTable::build_qualified_name(const std::string& name) const {
    return current->build_qualified_name(name);
}

std::string SymbolTable::to_string() const {
    std::stringstream ss;
    ss << "=== SYMBOL TABLE ===\n";
    
    // Print current context
    if (auto* ns = get_current_namespace()) {
        if (ns->name() != "global") {
            ss << "Current Namespace: " << ns->name() << "\n";
        }
    }
    
    if (auto* type = get_current_type()) {
        ss << "Current Type: " << type->name() << "\n";
    } 
    
    if (auto* func = get_current_function()) {
        ss << "Current Function: " << func->name() << "\n";
    }
    
    ss << "\nScope Hierarchy:\n";
    
    // Recursive function to print the tree
    std::function<void(ScopeNode*, int)> print_tree = [&](ScopeNode* node, int indent) {
        std::string indent_str(indent * 2, ' ');
        
        if (auto* sym = node->as_symbol()) {
            // Print symbol info
            ss << indent_str;
            ss << sym->kind_name() << " " << sym->name();
            
            // Add type-specific info
            if (auto* typed_symbol = sym->as<TypedSymbol>()) {
                if (typed_symbol->type()) {
                    if (auto* func = sym->as<FunctionSymbol>()) {
                        ss << " -> " << typed_symbol->type()->get_name();
                    } else {
                        ss << ": " << typed_symbol->type()->get_name();
                    }
                }
            } else if (auto* type = sym->as<TypeSymbol>()) {
                if (type->is_ref_type()) ss << " (ref)";
                if (type->is_abstract()) ss << " (abstract)";
            } else if (auto* enum_sym = sym->as<EnumSymbol>()) {
                ss << " (enum)";
            } else if (auto* case_sym = sym->as<EnumCaseSymbol>()) {
                if (case_sym->is_tagged()) {
                    ss << "(";
                    bool first = true;
                    for (const auto& type : case_sym->associated_types()) {
                        if (!first) ss << ", ";
                        ss << type->get_name();
                        first = false;
                    }
                    ss << ")";
                }
            }
            
            // Add brackets if this node has children
            if (!node->children.empty()) {
                ss << " {\n";
                // Print children
                for (const auto& [key, child] : node->children) {
                    print_tree(child.get(), indent + 1);
                }
                ss << indent_str << "}";
            }
            ss << "\n";
        } else if (auto* block = node->as_block()) {
            ss << indent_str << "block (" << block->debug_name << ")";
            
            // Add brackets if this block has children
            if (!node->children.empty()) {
                ss << " {\n";
                // Print children
                for (const auto& [key, child] : node->children) {
                    print_tree(child.get(), indent + 1);
                }
                ss << indent_str << "}";
            }
            ss << "\n";
        }
    };
    
    print_tree(global_symbol.get(), 0);
    
    // Display unresolved symbols
    if (!unresolved_symbols.empty()) {
        ss << "\nUnresolved Symbols:\n";
        for (const auto* sym : unresolved_symbols) {
            ss << " - " << sym->name() << "\n";
        }
    }
    
    return ss.str();
}

} // namespace Myre