#include "compiler.hpp"
#include "semantic/symbol_table.hpp"
#include "semantic/type_registry.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
using namespace Myre;


std::string read_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void test_type_system() {
    std::cout << "\n=== TYPE SYSTEM TEST ===\n\n";
    
    // Create symbol table and type registry
    SymbolTable symbolTable;
    TypeRegistry& registry = symbolTable.get_type_registry();
    
    std::cout << "1. Initial Type Registry:\n";
    std::cout << registry.to_string() << "\n";
    
    // Create a custom type: Player
    std::cout << "2. Creating Player type...\n";
    auto playerDef = std::make_shared<TypeDefinition>("Player", "Game.Player");
    
    // Add health field
    auto healthSymbol = std::make_shared<Symbol>();
    healthSymbol->kind = SymbolKind::Field;
    healthSymbol->name = "health";
    healthSymbol->type = registry.get_primitive("i32");
    healthSymbol->access = AccessLevel::Public;
    playerDef->add_member(healthSymbol);
    
    // Add name field
    auto nameSymbol = std::make_shared<Symbol>();
    nameSymbol->kind = SymbolKind::Field;
    nameSymbol->name = "name";
    nameSymbol->type = registry.get_primitive("string");
    nameSymbol->access = AccessLevel::Public;
    playerDef->add_member(nameSymbol);
    
    // Add move method
    auto moveSymbol = std::make_shared<Symbol>();
    moveSymbol->kind = SymbolKind::Function;
    moveSymbol->name = "move";
    moveSymbol->type = registry.get_function_type(
        registry.get_primitive("void"),
        {registry.get_primitive("f32"), registry.get_primitive("f32")}
    );
    moveSymbol->access = AccessLevel::Public;
    playerDef->add_member(moveSymbol);
    
    // Register the type
    registry.register_type_definition("Game.Player", playerDef);
    
    // Create type symbol and add to symbol table
    auto playerTypeSymbol = std::make_shared<Symbol>();
    playerTypeSymbol->kind = SymbolKind::Type;
    playerTypeSymbol->name = "Player";
    playerTypeSymbol->type = registry.get_defined_type(playerDef);
    playerTypeSymbol->access = AccessLevel::Public;
    symbolTable.define(playerTypeSymbol);
    
    std::cout << "3. After adding Player type:\n";
    std::cout << registry.to_string() << "\n";
    std::cout << symbolTable.to_string() << "\n";
    
    // Test type canonicalization
    std::cout << "4. Testing type canonicalization...\n";
    TypePtr player1 = registry.get_type_reference("Game.Player");
    TypePtr player2 = registry.get_type_reference("Game.Player");
    TypePtr playerArray1 = registry.get_array_type(player1, 1);
    TypePtr playerArray2 = registry.get_array_type(player2, 1);
    
    std::cout << "player1 == player2: " << (player1 == player2 ? "true" : "false") << "\n";
    std::cout << "playerArray1 == playerArray2: " << (playerArray1 == playerArray2 ? "true" : "false") << "\n";
    
    // Test member lookup
    std::cout << "\n5. Testing member lookup...\n";
    auto healthMember = playerDef->lookup_member("health");
    if (healthMember) {
        std::cout << "Found health member: " << healthMember->name << " : " << healthMember->type->get_name() << "\n";
    }
    
    auto moveMember = playerDef->lookup_member("move");
    if (moveMember) {
        std::cout << "Found move member: " << moveMember->name << " : " << moveMember->type->get_name() << "\n";
    }
    
    // Test compound types
    std::cout << "\n6. Testing compound types...\n";
    TypePtr i32Type = registry.get_primitive("i32");
    TypePtr i32Array = registry.get_array_type(i32Type, 1);
    TypePtr i32Array2D = registry.get_array_type(i32Type, 2);
    
    std::cout << "i32 type: " << i32Type->get_name() << "\n";
    std::cout << "i32[] type: " << i32Array->get_name() << "\n";
    std::cout << "i32[,] type: " << i32Array2D->get_name() << "\n";
    
    std::cout << "\n7. Final Type Registry state:\n";
    std::cout << registry.to_string() << "\n";
    
    std::cout << "=== TYPE SYSTEM TEST COMPLETE ===\n\n";
}



int main(int argc, char* argv[])
{
    Logger& logger = Logger::get_instance();
    logger.initialize();
    logger.set_console_level(LogLevel::ERR); // Only show errors for cleaner output

    // Test the type system
    test_type_system();

    // Optionally run the compiler if a file is provided
    if (argc > 1) {
        std::cout << "\n=== COMPILER TEST ===\n";
        Compiler compiler;
        auto source = read_file(argv[1]);
        compiler.compile(source);
    }

    return 0;
}