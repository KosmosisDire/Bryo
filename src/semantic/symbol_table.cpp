#include "semantic/symbol_table.hpp"
#include "ast/ast.hpp"
#include "ast/ast_rtti.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace Mycelium::Scripting::Lang {

using namespace Mycelium::Scripting::Common;


SymbolTable::SymbolTable() : building_scope_level(0) {
    // Create global scope
    all_scopes.emplace_back("global", -1);
    scope_name_to_id["global"] = 0;
    next_scope_id = 1;
    
    // Start with global scope active
    active_scope_stack.push_back(0);
}

// === BUILDING PHASE API ===
void SymbolTable::enter_scope() {
    // Create an anonymous scope for building
    std::string scope_name = "scope_" + std::to_string(next_scope_id);
    enter_named_scope(scope_name);
}

void SymbolTable::enter_named_scope(const std::string& scope_name) {
    int parent_id = building_scope_level;
    all_scopes.emplace_back(scope_name, parent_id);
    scope_name_to_id[scope_name] = next_scope_id;
    building_scope_level = next_scope_id;
    next_scope_id++;
}

void SymbolTable::exit_scope() {
    if (building_scope_level > 0) {
        // Find parent scope
        building_scope_level = all_scopes[building_scope_level].parent_scope_id;
    }
}

bool SymbolTable::declare_symbol(const std::string& name, SymbolType type, const std::string& data_type) {
    if (symbol_exists_current_scope(name)) {
        return false;
    }
    
    auto symbol = std::make_shared<Symbol>(name, type, data_type, building_scope_level);
    all_scopes[building_scope_level].symbols[name] = symbol;
    return true;
}

// === NAVIGATION API ===
int SymbolTable::push_scope(const std::string& scope_name) {
    auto it = scope_name_to_id.find(scope_name);
    if (it != scope_name_to_id.end()) {
        active_scope_stack.push_back(it->second);
        return it->second;
    }
    return -1; // Scope not found
}

int SymbolTable::push_scope(int scope_id) {
    if (scope_id >= 0 && scope_id < all_scopes.size()) {
        active_scope_stack.push_back(scope_id);
        return scope_id;
    }
    return -1; // Invalid scope ID
}

void SymbolTable::pop_scope() {
    if (active_scope_stack.size() > 1) { // Keep at least global scope
        active_scope_stack.pop_back();
    }
}

void SymbolTable::reset_navigation() {
    active_scope_stack.clear();
    active_scope_stack.push_back(0); // Reset to global scope
}

// === QUERY API ===
std::shared_ptr<Symbol> SymbolTable::lookup_symbol(const std::string& name) {
    // Search from current scope up through parent chain
    for (int i = active_scope_stack.size() - 1; i >= 0; i--) {
        int scope_id = active_scope_stack[i];
        auto it = all_scopes[scope_id].symbols.find(name);
        if (it != all_scopes[scope_id].symbols.end()) {
            return it->second;
        }
    }
    return nullptr;
}

std::shared_ptr<Symbol> SymbolTable::lookup_symbol_current_scope(const std::string& name) {
    if (active_scope_stack.empty()) return nullptr;
    
    int current_scope = active_scope_stack.back();
    auto it = all_scopes[current_scope].symbols.find(name);
    if (it != all_scopes[current_scope].symbols.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Symbol> SymbolTable::lookup_symbol_in_scope(int scope_id, const std::string& name) {
    if (scope_id < 0 || scope_id >= all_scopes.size()) return nullptr;
    
    auto it = all_scopes[scope_id].symbols.find(name);
    if (it != all_scopes[scope_id].symbols.end()) {
        return it->second;
    }
    return nullptr;
}

bool SymbolTable::symbol_exists(const std::string& name) {
    return lookup_symbol(name) != nullptr;
}

bool SymbolTable::symbol_exists_current_scope(const std::string& name) {
    return lookup_symbol_current_scope(name) != nullptr;
}

// === SCOPE MANAGEMENT ===
int SymbolTable::find_scope_by_name(const std::string& scope_name) {
    auto it = scope_name_to_id.find(scope_name);
    return (it != scope_name_to_id.end()) ? it->second : -1;
}

int SymbolTable::get_current_scope_id() const {
    return active_scope_stack.empty() ? -1 : active_scope_stack.back();
}

std::string SymbolTable::get_current_scope_name() const {
    int scope_id = get_current_scope_id();
    return (scope_id >= 0 && scope_id < all_scopes.size()) ? all_scopes[scope_id].scope_name : "";
}

void SymbolTable::clear() {
    all_scopes.clear();
    scope_name_to_id.clear();
    active_scope_stack.clear();
    building_scope_level = 0;
    next_scope_id = 0;
    
    // Recreate global scope
    all_scopes.emplace_back("global", -1);
    scope_name_to_id["global"] = 0;
    next_scope_id = 1;
    active_scope_stack.push_back(0);
}

void SymbolTable::print_symbol_table() const {
    LOG_HEADER("Symbol Table", LogCategory::SEMANTIC);
    LOG_INFO("Total scopes: " + std::to_string(all_scopes.size()), LogCategory::SEMANTIC);
    
    for (size_t scope_id = 0; scope_id < all_scopes.size(); ++scope_id) {
        const auto& scope = all_scopes[scope_id];
        LOG_SEPARATOR('-', 60, LogCategory::SEMANTIC);
        std::string scope_info = "Scope " + std::to_string(scope_id) + ": \"" + scope.scope_name + "\"";
        if (scope.parent_scope_id >= 0) {
            scope_info += " (parent: " + std::to_string(scope.parent_scope_id) + ")";
        }

        LOG_INFO(scope_info, LogCategory::SEMANTIC);
        
        if (scope.symbols.empty()) {
            LOG_INFO("  (empty)", LogCategory::SEMANTIC);
        } else {
            // Create formatted header
            std::ostringstream header;
            header << Colors::DIM
                   << std::setw(20) << "Name" 
                   << std::setw(12) << "Type" 
                   << std::setw(15) << "Data Type" 
                   << Colors::RESET;
            LOG_INFO(header.str(), LogCategory::SEMANTIC);
            
            for (const auto& [name, symbol] : scope.symbols) {
                std::string type_str;
                switch (symbol->type) {
                    case SymbolType::VARIABLE: type_str = "VARIABLE"; break;
                    case SymbolType::FUNCTION: type_str = "FUNCTION"; break;
                    case SymbolType::CLASS: type_str = "CLASS"; break;
                    case SymbolType::PARAMETER: type_str = "PARAMETER"; break;
                }
                
                std::ostringstream row;
                row << std::setw(20) << symbol->name
                    << std::setw(12) << type_str
                    << std::setw(15) << symbol->data_type;
                LOG_INFO(row.str(), LogCategory::SEMANTIC);
            }
        }
    }
}

void SymbolTable::print_navigation_state() const {
    LOG_SUBHEADER("Navigation State", LogCategory::SEMANTIC);
    
    std::string scope_stack = "Active scope stack: ";
    for (size_t i = 0; i < active_scope_stack.size(); ++i) {
        if (i > 0) scope_stack += " -> ";
        int scope_id = active_scope_stack[i];
        scope_stack += std::to_string(scope_id) + "(\"" + all_scopes[scope_id].scope_name + "\")";
    }
    LOG_INFO(scope_stack, LogCategory::SEMANTIC);
    
    LOG_INFO("Current scope: " + get_current_scope_name() + " (ID: " + std::to_string(get_current_scope_id()) + ")", LogCategory::SEMANTIC);
    LOG_SEPARATOR('-', 30, LogCategory::SEMANTIC);
}

using namespace Mycelium::Scripting::Lang;

class SymbolTableBuilder {
private:
    SymbolTable& symbol_table;
    
    std::string get_type_string(TypeNameNode* type_node) {
        if (!type_node) return "void";
        
        if (auto simple = type_node->as<TypeNameNode>()) {
            return std::string(simple->identifier->name);
        }
        if (auto qualified = type_node->as<QualifiedTypeNameNode>()) {
            return get_type_string(qualified->left) + "::" + std::string(qualified->right->name);
        }
        if (auto pointer = type_node->as<PointerTypeNameNode>()) {
            return get_type_string(pointer->elementType) + "*";
        }
        if (auto array = type_node->as<ArrayTypeNameNode>()) {
            return get_type_string(array->elementType) + "[]";
        }
        if (auto generic = type_node->as<GenericTypeNameNode>()) {
            std::string result = get_type_string(generic->baseType) + "<";
            for (int i = 0; i < generic->arguments.size; i++) {
                if (i > 0) result += ", ";
                result += get_type_string(generic->arguments.values[i]);
            }
            result += ">";
            return result;
        }
        return "unknown";
    }
    
    void visit_declaration(DeclarationNode* node) {
        if (!node) return;
        
        if (auto type_decl = node->as<TypeDeclarationNode>()) {
            visit_type_declaration(type_decl);
        } else if (auto interface_decl = node->as<InterfaceDeclarationNode>()) {
            visit_interface_declaration(interface_decl);
        } else if (auto enum_decl = node->as<EnumDeclarationNode>()) {
            visit_enum_declaration(enum_decl);
        } else if (auto func_decl = node->as<FunctionDeclarationNode>()) {
            visit_function_declaration(func_decl);
        } else if (auto field_decl = node->as<FieldDeclarationNode>()) {
            visit_field_declaration(field_decl);
        } else if (auto var_decl = node->as<VariableDeclarationNode>()) {
            visit_variable_declaration(var_decl);
        } else if (auto ns_decl = node->as<NamespaceDeclarationNode>()) {
            visit_namespace_declaration(ns_decl);
        }
    }
    
    void visit_type_declaration(TypeDeclarationNode* node) {
        std::string type_name = std::string(node->name->name);
        // Check modifiers to determine if it's a ref type (class) or value type (struct)
        bool is_ref_type = false;
        for (int i = 0; i < node->modifiers.size; i++) {
            if (node->modifiers.values[i] == ModifierKind::Ref) {
                is_ref_type = true;
                break;
            }
        }
        symbol_table.declare_symbol(type_name, SymbolType::CLASS, is_ref_type ? "ref type" : "type");
        
        symbol_table.enter_named_scope(type_name);
        
        for (int i = 0; i < node->members.size; i++) {
            visit_declaration(node->members.values[i]);
        }
        
        symbol_table.exit_scope();
    }
    
    void visit_interface_declaration(InterfaceDeclarationNode* node) {
        std::string interface_name = std::string(node->name->name);
        symbol_table.declare_symbol(interface_name, SymbolType::CLASS, "interface");
        
        symbol_table.enter_named_scope(interface_name);
        
        for (int i = 0; i < node->members.size; i++) {
            visit_declaration(node->members.values[i]);
        }
        
        symbol_table.exit_scope();
    }
    
    void visit_enum_declaration(EnumDeclarationNode* node) {
        std::string enum_name = std::string(node->name->name);
        symbol_table.declare_symbol(enum_name, SymbolType::CLASS, "enum");
        
        symbol_table.enter_named_scope(enum_name);
        
        // Handle enum cases
        for (int i = 0; i < node->cases.size; i++) {
            if (auto case_node = node->cases.values[i]) {
                std::string case_name = std::string(case_node->name->name);
                symbol_table.declare_symbol(case_name, SymbolType::VARIABLE, "enum case");
            }
        }
        
        // Handle enum methods
        for (int i = 0; i < node->methods.size; i++) {
            visit_function_declaration(node->methods.values[i]);
        }
        
        symbol_table.exit_scope();
    }
    
    void visit_function_declaration(FunctionDeclarationNode* node) {
        std::string func_name = std::string(node->name->name);
        std::string return_type = get_type_string(node->returnType);
        symbol_table.declare_symbol(func_name, SymbolType::FUNCTION, return_type);
        
        symbol_table.enter_named_scope(func_name);
        
        for (int i = 0; i < node->parameters.size; i++) {
            auto param = node->parameters.values[i];
            std::string param_type = get_type_string(param->type);
            symbol_table.declare_symbol(std::string(param->name->name), SymbolType::PARAMETER, param_type);
        }
        
        if (node->body) {
            // Process block contents directly without creating a new scope
            // since the function already has its own scope
            for (int i = 0; i < node->body->statements.size; i++) {
                visit_statement(node->body->statements.values[i]);
            }
        }
        
        symbol_table.exit_scope();
    }
    
    void visit_field_declaration(FieldDeclarationNode* node) {
        std::string field_type = get_type_string(node->type);
        // Handle multiple field names (x, y, z: type)
        for (int i = 0; i < node->names.size; i++) {
            if (node->names.values[i]) {
                symbol_table.declare_symbol(std::string(node->names.values[i]->name), SymbolType::VARIABLE, field_type);
            }
        }
    }
    
    void visit_variable_declaration(VariableDeclarationNode* node) {
        std::string var_type = get_type_string(node->type);
        symbol_table.declare_symbol(std::string(node->name->name), SymbolType::VARIABLE, var_type);
    }
    
    void visit_namespace_declaration(NamespaceDeclarationNode* node) {
        symbol_table.enter_scope();
        
        if (node->body) {
            visit_statement(node->body);
        }
        
        symbol_table.exit_scope();
    }
    
    void visit_statement(StatementNode* node) {
        if (!node) return;
        
        if (auto block = node->as<BlockStatementNode>()) {
            visit_block_statement(block);
        } else if (auto local_var = node->as<LocalVariableDeclarationNode>()) {
            visit_local_variable_declaration(local_var);
        } else if (auto if_stmt = node->as<IfStatementNode>()) {
            visit_if_statement(if_stmt);
        } else if (auto while_stmt = node->as<WhileStatementNode>()) {
            visit_while_statement(while_stmt);
        } else if (auto for_stmt = node->as<ForStatementNode>()) {
            visit_for_statement(for_stmt);
        }
    }
    
    void visit_block_statement(BlockStatementNode* node) {
        symbol_table.enter_scope();
        
        for (int i = 0; i < node->statements.size; i++) {
            visit_statement(node->statements.values[i]);
        }
        
        symbol_table.exit_scope();
    }
    
    void visit_local_variable_declaration(LocalVariableDeclarationNode* node) {
        for (int i = 0; i < node->declarators.size; i++) {
            auto var_decl = node->declarators.values[i];
            std::string var_type = get_type_string(var_decl->type);
            symbol_table.declare_symbol(std::string(var_decl->name->name), SymbolType::VARIABLE, var_type);
        }
    }
    
    void visit_if_statement(IfStatementNode* node) {
        visit_statement(node->thenStatement);
        if (node->elseStatement) {
            visit_statement(node->elseStatement);
        }
    }
    
    void visit_while_statement(WhileStatementNode* node) {
        visit_statement(node->body);
    }
    
    void visit_for_statement(ForStatementNode* node) {
        symbol_table.enter_scope();
        
        if (node->initializer) {
            visit_statement(node->initializer);
        }
        
        visit_statement(node->body);
        
        symbol_table.exit_scope();
    }

public:
    SymbolTableBuilder(SymbolTable& table) : symbol_table(table) {}
    
    void build_from_ast(CompilationUnitNode* root) {
        if (!root) return;
        
        symbol_table.clear();
        
        for (int i = 0; i < root->statements.size; i++) {
            auto stmt = root->statements.values[i];
            // Top-level statements in a compilation unit are often declarations
            if (auto decl = stmt->as<DeclarationNode>()) {
                visit_declaration(decl);
            } else {
                visit_statement(stmt);
            }
        }
    }
};

void build_symbol_table(SymbolTable& table, CompilationUnitNode* ast) {
    SymbolTableBuilder builder(table);
    builder.build_from_ast(ast);
}

} // namespace Mycelium::Scripting::Lang