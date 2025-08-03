#include "semantic/symbol_table.hpp"
#include <iostream>

namespace Myre {

SymbolTable::SymbolTable() {
    // Initialize with global scope
    enter_scope();
    
    // Initialize primitive types
    initialize_primitive_types();
}

void SymbolTable::initialize_primitive_types() {
    for (const auto& info : PRIMITIVE_TYPES) {
        auto* type = create_type<PrimitiveType>(info.name, info.size);
        primitive_types_[info.name] = type;
        
        // Add primitive type as a symbol
        add_symbol(info.name, SymbolKind::Type, type);
    }
}

// === SCOPE MANAGEMENT ===

void SymbolTable::enter_scope() {
    scope_stack_.emplace_back();
}

void SymbolTable::exit_scope() {
    if (scope_stack_.size() > 1) {  // Keep at least global scope
        scope_stack_.pop_back();
    }
}

// === SYMBOL OPERATIONS ===

bool SymbolTable::add_symbol(const std::string& name, SymbolKind kind, Type* type, void* ast_node) {
    if (scope_stack_.empty()) {
        return false;  // No scope to add to
    }
    
    auto& current_scope = scope_stack_.back();
    
    // Check if symbol already exists in current scope
    if (current_scope.find(name) != current_scope.end()) {
        return false;  // Symbol already exists
    }
    
    // Add symbol - O(1) operation
    current_scope.emplace(name, Symbol(name, kind, type, ast_node));
    return true;
}

Symbol* SymbolTable::lookup(const std::string& name) {
    // Search from innermost to outermost scope
    for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
        auto symbol_it = it->find(name);
        if (symbol_it != it->end()) {
            return &symbol_it->second;
        }
    }
    return nullptr;
}

const Symbol* SymbolTable::lookup(const std::string& name) const {
    // Const version
    for (auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it) {
        auto symbol_it = it->find(name);
        if (symbol_it != it->end()) {
            return &symbol_it->second;
        }
    }
    return nullptr;
}

Symbol* SymbolTable::lookup_current_scope(const std::string& name) {
    if (scope_stack_.empty()) {
        return nullptr;
    }
    
    auto& current_scope = scope_stack_.back();
    auto it = current_scope.find(name);
    return it != current_scope.end() ? &it->second : nullptr;
}

Symbol* SymbolTable::lookup_member_function(const std::string& type_name, const std::string& method_name) {
    // First lookup the type
    auto* type_symbol = lookup(type_name);
    if (!type_symbol || type_symbol->kind != SymbolKind::Type) {
        return nullptr;
    }
    
    // Check if it's a struct type
    auto* struct_type = type_symbol->type->as<StructType>();
    if (!struct_type) {
        return nullptr;
    }
    
    // Look for the method - we store as qualified name
    std::string qualified_name = type_name + "::" + method_name;
    return lookup(qualified_name);
}

// === TYPE MANAGEMENT ===

PrimitiveType* SymbolTable::get_primitive_type(const std::string& name) {
    auto it = primitive_types_.find(name);
    if (it != primitive_types_.end()) {
        return it->second;
    }
    
    // Create new primitive type if not found
    auto* type = create_type<PrimitiveType>(name, 0);  // Size 0 for unknown types
    primitive_types_[name] = type;
    return type;
}

StructType* SymbolTable::create_struct_type(const std::string& name) {
    auto* type = create_type<StructType>(name);
    struct_types_[name] = type;
    return type;
}

FunctionType* SymbolTable::create_function_type(Type* return_type, const std::vector<Type*>& params, bool varargs) {
    return create_type<FunctionType>(return_type, params, varargs);
}

PointerType* SymbolTable::create_pointer_type(Type* pointee) {
    return create_type<PointerType>(pointee);
}

ArrayType* SymbolTable::create_array_type(Type* element, size_t size) {
    return create_type<ArrayType>(element, size);
}

Type* SymbolTable::lookup_type(const std::string& name) {
    auto* symbol = lookup(name);
    if (symbol && symbol->kind == SymbolKind::Type) {
        return symbol->type;
    }
    return nullptr;
}

void SymbolTable::register_struct_type(StructType* struct_type) {
    // Add the struct type itself
    add_symbol(struct_type->name(), SymbolKind::Type, struct_type);
    
    // Add all methods as qualified functions
    for (const auto& method : struct_type->methods()) {
        std::string qualified_name = struct_type->name() + "::" + method.name;
        
        // Create function type with implicit 'this' parameter
        std::vector<Type*> params;
        params.push_back(create_pointer_type(struct_type));  // 'this' pointer
        
        // Add original parameters
        const auto& method_params = method.type->parameter_types();
        params.insert(params.end(), method_params.begin(), method_params.end());
        
        auto* method_type = create_function_type(method.type->return_type(), params);
        
        add_symbol(qualified_name, SymbolKind::Function, method_type);
    }
}

// === UTILITY ===

std::vector<Symbol*> SymbolTable::get_current_scope_symbols() {
    std::vector<Symbol*> result;
    if (!scope_stack_.empty()) {
        auto& current_scope = scope_stack_.back();
        for (auto& [name, symbol] : current_scope) {
            result.push_back(&symbol);
        }
    }
    return result;
}

void SymbolTable::dump_symbols() const {
    std::cout << "=== Symbol Table Dump ===" << std::endl;
    for (size_t i = 0; i < scope_stack_.size(); ++i) {
        std::cout << "Scope " << i << ":" << std::endl;
        for (const auto& [name, symbol] : scope_stack_[i]) {
            std::cout << "  " << name << " : ";
            switch (symbol.kind) {
                case SymbolKind::Variable: std::cout << "Variable"; break;
                case SymbolKind::Function: std::cout << "Function"; break;
                case SymbolKind::Type: std::cout << "Type"; break;
                case SymbolKind::Parameter: std::cout << "Parameter"; break;
                case SymbolKind::Constant: std::cout << "Constant"; break;
            }
            std::cout << " of type " << symbol.type->to_string() << std::endl;
        }
    }
}

} // namespace Myre