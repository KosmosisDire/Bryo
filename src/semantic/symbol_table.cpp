#include "semantic/symbol_table.hpp"
#include <algorithm>
#include <sstream>
#include <functional>

namespace Myre {

SymbolTable::SymbolTable() {
    // Create global namespace symbol
    global_symbol = std::make_unique<Symbol>();
    global_symbol->kind = SymbolKind::Namespace;
    global_symbol->name = "global";
    global_symbol->access = AccessLevel::Public;
    current = global_symbol.get();
}

void SymbolTable::add_child(const std::string& key, std::unique_ptr<ScopeNode> child) {
    child->parent = current;
    current->children[key] = std::move(child);
}

Symbol* SymbolTable::enter_namespace(const std::string& name) {
    auto sym = std::make_unique<Symbol>();
    sym->kind = SymbolKind::Namespace;
    sym->name = name;
    sym->access = AccessLevel::Public;
    Symbol* ptr = sym.get();
    add_child(name, std::move(sym));
    current = ptr;
    return ptr;
}

Symbol* SymbolTable::enter_type(const std::string& name) {
    auto sym = std::make_unique<Symbol>();
    sym->kind = SymbolKind::Type;
    sym->name = name;
    sym->access = AccessLevel::Public;
    Symbol* ptr = sym.get();
    
    // Register type in type system
    std::string full_name = ptr->get_qualified_name();
    type_system.register_type_symbol(full_name, ptr);
    
    add_child(name, std::move(sym));
    current = ptr;
    return ptr;
}

Symbol* SymbolTable::enter_function(const std::string& name, TypePtr return_type, 
                                    std::vector<TypePtr> params) {
    auto sym = std::make_unique<Symbol>();
    sym->kind = SymbolKind::Function;
    sym->name = name;
    sym->type = return_type;
    sym->parameter_types = params;
    sym->access = AccessLevel::Private;
    Symbol* ptr = sym.get();
    add_child(name, std::move(sym));
    current = ptr;
    return ptr;
}

BlockScope* SymbolTable::enter_block(const std::string& debug_name) {
    auto block = std::make_unique<BlockScope>(debug_name);
    BlockScope* ptr = block.get();
    std::string key = "$block_" + std::to_string(next_block_id++);
    add_child(key, std::move(block));
    current = ptr;
    return ptr;
}

void SymbolTable::exit_scope() {
    if (current->parent) {
        current = current->parent;
    }
}

Symbol* SymbolTable::define_variable(const std::string& name, TypePtr type) {
    auto sym = std::make_unique<Symbol>();
    sym->kind = SymbolKind::Variable;
    sym->name = name;
    sym->type = type;
    sym->access = AccessLevel::Private;
    Symbol* ptr = sym.get();
    
    // Check if this symbol has an unresolved type and needs inference
    if (type && std::holds_alternative<UnresolvedType>(type->value)) {
        unresolved_symbols.push_back(ptr);
    }
    
    add_child(name, std::move(sym));
    return ptr;
}

Symbol* SymbolTable::define_parameter(const std::string& name, TypePtr type) {
    auto sym = std::make_unique<Symbol>();
    sym->kind = SymbolKind::Parameter;
    sym->name = name;
    sym->type = type;
    sym->access = AccessLevel::Private;
    Symbol* ptr = sym.get();
    
    // Check if this symbol has an unresolved type and needs inference
    if (type && std::holds_alternative<UnresolvedType>(type->value)) {
        unresolved_symbols.push_back(ptr);
    }
    
    add_child(name, std::move(sym));
    return ptr;
}

Symbol* SymbolTable::define_field(const std::string& name, TypePtr type) {
    auto sym = std::make_unique<Symbol>();
    sym->kind = SymbolKind::Field;
    sym->name = name;
    sym->type = type;
    sym->access = AccessLevel::Private;
    Symbol* ptr = sym.get();
    
    // Check if this symbol has an unresolved type and needs inference
    if (type && std::holds_alternative<UnresolvedType>(type->value)) {
        unresolved_symbols.push_back(ptr);
    }
    
    add_child(name, std::move(sym));
    return ptr;
}

Symbol* SymbolTable::get_current_namespace() const {
    return current->get_enclosing_namespace();
}

Symbol* SymbolTable::get_current_type() const {
    return current->get_enclosing_type();
}

Symbol* SymbolTable::get_current_function() const {
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
    auto symbol = scope->lookup(type_name);
    if (!symbol || !symbol->is_type()) {
        return type_system.get_unresolved_type();
    }
    
    // Create a type reference to this symbol
    return type_system.get_type_reference(symbol);
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
        if (ns->name != "global") {
            ss << "Current Namespace: " << ns->name << "\n";
        }
    }
    
    if (auto* type = get_current_type()) {
        ss << "Current Type: " << type->name << "\n";
    } 
    
    if (auto* func = get_current_function()) {
        ss << "Current Function: " << func->name << "\n";
    }
    
    ss << "\nScope Hierarchy:\n";
    
    // Recursive function to print the tree
    std::function<void(ScopeNode*, int)> print_tree = [&](ScopeNode* node, int indent) {
        std::string indent_str(indent * 2, ' ');
        
        if (auto* sym = node->as_symbol()) {
            // Print symbol info
            ss << indent_str;
            switch (sym->kind) {
                case SymbolKind::Namespace:
                    ss << "namespace " << sym->name;
                    break;
                case SymbolKind::Type:
                    ss << "type " << sym->name;
                    if (sym->has_modifier(SymbolModifiers::Ref)) ss << " (ref)";
                    if (sym->has_modifier(SymbolModifiers::Abstract)) ss << " (abstract)";
                    break;
                case SymbolKind::Function:
                    ss << "fn " << sym->name;
                    if (sym->type) ss << " -> " << sym->type->get_name();
                    break;
                case SymbolKind::Variable:
                    ss << "var " << sym->name;
                    if (sym->type) ss << ": " << sym->type->get_name();
                    break;
                case SymbolKind::Parameter:
                    ss << "param " << sym->name;
                    if (sym->type) ss << ": " << sym->type->get_name();
                    break;
                case SymbolKind::Field:
                    ss << "field " << sym->name;
                    if (sym->type) ss << ": " << sym->type->get_name();
                    break;
                default:
                    ss << "unknown " << sym->name;
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
            ss << " - " << sym->name << "\n";
        }
    }
    
    return ss.str();
}

} // namespace Myre