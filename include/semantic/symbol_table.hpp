#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace Mycelium::Scripting::Lang {

enum class SymbolType {
    VARIABLE,
    FUNCTION,
    CLASS,
    PARAMETER
};

struct Symbol {
    std::string name;
    SymbolType type;
    std::string data_type;
    int scope_level;
    
    Symbol(const std::string& n, SymbolType t, const std::string& dt, int level)
        : name(n), type(t), data_type(dt), scope_level(level) {}
};

struct Scope {
    std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols;
    int parent_scope_id = -1;
    std::string scope_name;
    
    Scope(const std::string& name = "", int parent = -1) 
        : scope_name(name), parent_scope_id(parent) {}
};

class SymbolTable {
private:
    // Persistent storage of all scopes
    std::vector<Scope> all_scopes;
    std::unordered_map<std::string, int> scope_name_to_id;
    int next_scope_id = 0;
    
    // Navigation stack for traversal
    std::vector<int> active_scope_stack;
    
    // Building state (used during symbol table construction)
    int building_scope_level = 0;

public:
    SymbolTable();
    ~SymbolTable() = default;
    
    // === BUILDING PHASE API ===
    // Used during initial symbol table construction
    void enter_scope();  // For building phase
    void enter_named_scope(const std::string& scope_name);  // For building phase with name
    void exit_scope();   // For building phase
    bool declare_symbol(const std::string& name, SymbolType type, const std::string& data_type);
    
    // === NAVIGATION API ===
    // Used during code generation/analysis phases
    int push_scope(const std::string& scope_name);  // Returns scope ID
    int push_scope(int scope_id);                   // Push by ID
    void pop_scope();                               // Pop from navigation stack
    void reset_navigation();                        // Reset to global scope
    
    // === QUERY API ===
    // Works with current navigation state
    std::shared_ptr<Symbol> lookup_symbol(const std::string& name);
    std::shared_ptr<Symbol> lookup_symbol_current_scope(const std::string& name);
    std::shared_ptr<Symbol> lookup_symbol_in_scope(int scope_id, const std::string& name);
    
    bool symbol_exists(const std::string& name);
    bool symbol_exists_current_scope(const std::string& name);
    
    // === SCOPE MANAGEMENT ===
    int find_scope_by_name(const std::string& scope_name);
    int get_current_scope_id() const;
    int get_current_scope_level() const { return building_scope_level; }
    std::string get_current_scope_name() const;
    
    void clear();
    void print_symbol_table() const;
    void print_navigation_state() const;
};

// Forward declaration
struct CompilationUnitNode;

void build_symbol_table(SymbolTable& table, CompilationUnitNode* ast);

} // namespace Mycelium::Scripting::Lang