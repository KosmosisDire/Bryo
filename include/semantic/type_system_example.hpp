#pragma once

#include "type.hpp"
#include "type_definition.hpp"
#include "symbol.hpp"
#include "scope.hpp"
#include "symbol_table.hpp"
#include "type_registry.hpp"
#include <cassert>
#include <unordered_map>

namespace Myre {

// Example: How the type system works
//
// When you write in Myre:
//   type Player {
//       var health: i32 = 100;
//       fn move(x: f32, y: f32) { ... }
//   }
//
// This creates:

inline void example_type_definition() {
    SymbolTable symbol_table;
    TypeRegistry& registry = symbol_table.get_type_registry();
    
    // 1. Create a TypeDefinition for Player
    auto player_def = std::make_shared<TypeDefinition>("Player", "Game.Player");
    player_def->modifiers = SymbolModifiers::None; // Not a ref type by default
    
    // 2. Create a Symbol for the type itself
    auto player_type_symbol = std::make_shared<Symbol>();
    player_type_symbol->kind = SymbolKind::Type;
    player_type_symbol->name = "Player";
    player_type_symbol->type = registry.get_defined_type(player_def);
    player_type_symbol->access = AccessLevel::Public;
    
    // 3. Register the type definition
    registry.register_type_definition("Game.Player", player_def);
    
    // 4. Define the type symbol in current scope
    symbol_table.define(player_type_symbol);
    
    // 5. Enter the type's scope to add members
    symbol_table.enter_type("Player", player_type_symbol);
    
    // 6. Add the health field
    auto health_symbol = std::make_shared<Symbol>();
    health_symbol->kind = SymbolKind::Field;
    health_symbol->name = "health";
    health_symbol->type = registry.get_primitive("i32");
    health_symbol->access = AccessLevel::Public;
    
    symbol_table.define(health_symbol);
    player_def->add_member(health_symbol); // Also add to type definition
    
    // 7. Add the move method
    auto move_symbol = std::make_shared<Symbol>();
    move_symbol->kind = SymbolKind::Function;
    move_symbol->name = "move";
    move_symbol->type = registry.get_function_type(
        registry.get_primitive("void"),
        {registry.get_primitive("f32"), registry.get_primitive("f32")}
    );
    move_symbol->access = AccessLevel::Public;
    
    symbol_table.define(move_symbol);
    player_def->add_member(move_symbol);
    
    // 8. Exit the type's scope
    symbol_table.exit_scope();
}

// Example: How to look up type members
inline void example_member_lookup() {
    TypeRegistry registry;
    
    // Assume Player type is already registered
    auto player_def = registry.lookup_type_definition("Game.Player");
    if (player_def) {
        // Direct member lookup
        SymbolPtr health = player_def->lookup_member("health");
        if (health) {
            // Found the health field!
            std::string type_name = health->type->get_name(); // "i32"
        }
        
        // Iterate all members
        for (const auto& [name, symbol] : player_def->member_scope->symbols) {
            // Process each member
        }
    }
}

// Example: Type references vs definitions
inline void example_type_usage() {
    TypeRegistry registry;
    
    // Type definition (what you declare)
    auto player_def = registry.lookup_type_definition("Game.Player");
    
    // Type references (how you use types)
    TypePtr player_type = registry.get_type_reference("Game.Player");
    TypePtr player_array = registry.get_array_type(player_type, 1);
    TypePtr player_list = registry.get_instantiated_type(
        registry.lookup_type_definition("System.Collections.List"),
        {player_type}
    );
    
    // These are all different Type objects, but player_type and 
    // the Player in player_array point to the same TypeDefinition
}

// Example: Complex compound types like List<Player>[]
inline void example_compound_types() {
    TypeRegistry registry;
    
    // First, get the base types
    TypePtr player_type = registry.get_type_reference("Game.Player");
    auto list_def = registry.lookup_type_definition("System.Collections.List");
    
    // Create List<Player>
    TypePtr list_of_player = registry.get_instantiated_type(list_def, {player_type});
    
    // Create List<Player>[] (array of List<Player>)
    TypePtr array_of_list_of_player = registry.get_array_type(list_of_player, 1);
    
    // You can also create multi-dimensional arrays: List<Player>[][]
    TypePtr array_2d_of_list_of_player = registry.get_array_type(list_of_player, 2);
    
    // Or even more complex: Map<string, List<Player>[]>
    auto map_def = registry.lookup_type_definition("System.Collections.Map");
    TypePtr string_type = registry.get_primitive("string");
    TypePtr map_type = registry.get_instantiated_type(
        map_def, 
        {string_type, array_of_list_of_player}
    );
    
    // Due to canonicalization, these will be true:
    TypePtr list_of_player2 = registry.get_instantiated_type(list_def, {player_type});
    assert(list_of_player == list_of_player2); // Same pointer!
    assert(list_of_player->equals(list_of_player2)); // Also true
    
    // How to inspect compound types
    if (auto* array = std::get_if<ArrayType>(&array_of_list_of_player->value)) {
        // This is an array type
        TypePtr element = array->elementType; // This is List<Player>
        
        if (auto* inst = std::get_if<InstantiatedType>(&element->value)) {
            // The element is an instantiated generic
            auto generic_def = inst->generic_definition.lock(); // List<T>
            TypePtr type_arg = inst->type_arguments[0]; // Player
        }
    }
}

// Example: Looking up members on compound types
inline void example_compound_member_lookup() {
    TypeRegistry registry;
    
    // For List<Player>[], how do we find members?
    TypePtr player_type = registry.get_type_reference("Game.Player");
    auto list_def = registry.lookup_type_definition("System.Collections.List");
    TypePtr list_of_player = registry.get_instantiated_type(list_def, {player_type});
    TypePtr array_of_list_of_player = registry.get_array_type(list_of_player, 1);
    
    // To find members of the array type itself (like Length property)
    // Arrays don't have a TypeDefinition, they have built-in members
    // This would be handled by the semantic analyzer
    
    // To find members of the element type (List<Player>)
    if (auto* array = std::get_if<ArrayType>(&array_of_list_of_player->value)) {
        if (auto* inst = std::get_if<InstantiatedType>(&array->elementType->value)) {
            // Get the generic type definition (List<T>)
            if (auto def = inst->generic_definition.lock()) {
                // Now we can look up List's members
                SymbolPtr add_method = def->lookup_member("Add");
                SymbolPtr count_property = def->lookup_member("Count");
                
                // Note: The actual types would need substitution
                // Add(T item) becomes Add(Player item)
                // This is handled during semantic analysis
            }
        }
    }
}

// Example: Primitives as TypeDefinitions (like C#)
inline void example_primitive_type_definitions() {
    TypeRegistry registry;
    
    // In this model, i32 is just an alias for System.Int32
    TypePtr i32_type = registry.get_primitive("i32");
    
    // We can get the underlying TypeDefinition
    auto int32_def = i32_type->get_type_definition();
    if (int32_def) {
        // This would be "System.Int32"
        std::string full_name = int32_def->full_name;
        
        // We could add members to built-in types (like extension methods)
        // int32_def->lookup_member("ToString");
        // int32_def->lookup_member("CompareTo");
    }
    
    // For compound types like List<i32>[], we can now find members at every level
    TypePtr list_of_int = registry.get_instantiated_type(
        registry.lookup_type_definition("System.Collections.List"),
        {i32_type}
    );
    TypePtr array_of_list_of_int = registry.get_array_type(list_of_int, 1);
    
    // Array type: has built-in members (Length, etc.) - handled by semantic analyzer
    // Element type (List<i32>): has List<T> members with i32 substituted
    // Type arguments (i32): has System.Int32 members (ToString, CompareTo, etc.)
    
    if (auto* array = std::get_if<ArrayType>(&array_of_list_of_int->value)) {
        if (auto* inst = std::get_if<InstantiatedType>(&array->elementType->value)) {
            // List<i32> type
            auto list_def = inst->generic_definition.lock();
            TypePtr element_type = inst->type_arguments[0]; // i32
            
            // Get the underlying type definition for i32
            auto i32_def = element_type->get_type_definition(); // System.Int32
            if (i32_def) {
                // Now we can look up i32/System.Int32 members
                SymbolPtr toString = i32_def->lookup_member("ToString");
            }
        }
    }
}

// Example: Type canonicalization benefits
inline void example_type_canonicalization() {
    TypeRegistry registry;
    
    // Get the same type from different contexts
    TypePtr player1 = registry.get_type_reference("Player");
    TypePtr player2 = registry.get_type_reference("Player");
    
    // These are the SAME object due to canonicalization
    assert(player1 == player2); // True! Same pointer
    assert(player1.get() == player2.get()); // Also true
    
    // Complex types are also canonicalized
    TypePtr player_array1 = registry.get_array_type(player1, 1);
    TypePtr player_array2 = registry.get_array_type(player2, 1);
    assert(player_array1 == player_array2); // True!
    
    // Generic instantiations too
    auto list_def = registry.lookup_type_definition("System.Collections.List");
    TypePtr list1 = registry.get_instantiated_type(list_def, {player1});
    TypePtr list2 = registry.get_instantiated_type(list_def, {player2});
    assert(list1 == list2); // True!
    
    // This means:
    // 1. Type comparisons are just pointer comparisons (fast!)
    // 2. Memory efficient - no duplicate type objects
    // 3. Can use types as map keys directly
    std::unordered_map<TypePtr, std::string> type_names;
    type_names[player1] = "Player Type";
    assert(type_names[player2] == "Player Type"); // Works!
}

} // namespace Myre