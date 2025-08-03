#include "semantic/type_system.hpp"
#include "semantic/symbol_registry.hpp"
#include "semantic/error_system.hpp"
#include "codegen/ir_command.hpp"
#include "codegen/ir_builder.hpp"
#include <iostream>
#include <cassert>
#include <functional>

using namespace Myre;

// === TEST FRAMEWORK ===

class TestRunner {
private:
    int tests_run_ = 0;
    int tests_passed_ = 0;
    
public:
    void run_test(const std::string& name, std::function<bool()> test_func) {
        tests_run_++;
        std::cout << "Running test: " << name << "... ";
        
        try {
            if (test_func()) {
                tests_passed_++;
                std::cout << "✅ PASSED\n";
            } else {
                std::cout << "❌ FAILED\n";
            }
        } catch (const std::exception& e) {
            std::cout << "❌ FAILED (exception: " << e.what() << ")\n";
        }
    }
    
    void print_summary() {
        std::cout << "\n=== Test Summary ===\n";
        std::cout << "Tests run: " << tests_run_ << "\n";
        std::cout << "Tests passed: " << tests_passed_ << "\n";
        std::cout << "Tests failed: " << (tests_run_ - tests_passed_) << "\n";
        
        if (tests_passed_ == tests_run_) {
            std::cout << "🎉 All tests passed!\n";
        } else {
            std::cout << "💥 Some tests failed.\n";
        }
    }
};

// === TYPE SYSTEM TESTS ===

bool test_primitive_types() {
    auto i32_type = TypeFactory::i32();
    auto bool_type = TypeFactory::bool_type();
    
    assert(i32_type->name() == "i32");
    assert(bool_type->name() == "bool");
    assert(i32_type->kind() == Type::Kind::Primitive);
    assert(bool_type->kind() == Type::Kind::Primitive);
    
    // Test equality
    auto another_i32 = TypeFactory::i32();
    assert(i32_type->equals(*another_i32));
    assert(!i32_type->equals(*bool_type));
    
    return true;
}

bool test_struct_types() {
    auto i32_type = TypeFactory::i32();
    auto bool_type = TypeFactory::bool_type();
    
    // Create Player struct
    std::vector<FieldInfo> fields = {
        FieldInfo("health", i32_type, 0),
        FieldInfo("alive", bool_type, 4)
    };
    
    std::vector<MethodInfo> methods = {
        MethodInfo("getHealth", i32_type),
        MethodInfo("isAlive", bool_type)
    };
    
    auto player_type = TypeFactory::create_struct("Player", fields, methods);
    
    assert(player_type->name() == "Player");
    assert(player_type->kind() == Type::Kind::Struct);
    assert(player_type->fields().size() == 2);
    assert(player_type->methods().size() == 2);
    
    // Test field lookup
    auto health_field = player_type->find_field("health");
    assert(health_field.has_value());
    assert(health_field->name == "health");
    assert(health_field->type->equals(*i32_type));
    
    // Test method lookup
    auto get_health_method = player_type->find_method("getHealth");
    assert(get_health_method.has_value());
    assert(get_health_method->name == "getHealth");
    assert(get_health_method->return_type->equals(*i32_type));
    
    return true;
}

bool test_function_types() {
    auto i32_type = TypeFactory::i32();
    auto bool_type = TypeFactory::bool_type();
    
    // Create function type: (i32, bool) -> i32
    std::vector<std::shared_ptr<Type>> param_types = {i32_type, bool_type};
    auto func_type = TypeFactory::create_function(i32_type, param_types);
    
    assert(func_type->kind() == Type::Kind::Function);
    assert(func_type->return_type().equals(*i32_type));
    assert(func_type->parameter_types().size() == 2);
    assert(func_type->parameter_types()[0]->equals(*i32_type));
    assert(func_type->parameter_types()[1]->equals(*bool_type));
    
    return true;
}

bool test_pointer_types() {
    auto i32_type = TypeFactory::i32();
    auto i32_ptr_type = TypeFactory::create_pointer(i32_type);
    
    assert(i32_ptr_type->kind() == Type::Kind::Pointer);
    assert(i32_ptr_type->pointee_type().equals(*i32_type));
    
    return true;
}

// === SYMBOL REGISTRY TESTS ===

bool test_symbol_registry_basic() {
    SymbolRegistry registry;
    
    auto i32_type = TypeFactory::i32();
    
    // Add a variable
    auto new_registry = registry.add_variable("test_var", i32_type);
    
    // Look it up
    auto symbol = new_registry.lookup("test_var");
    assert(symbol.has_value());
    assert(symbol->name() == "test_var");
    assert(symbol->kind() == Symbol::Kind::Variable);
    assert(symbol->type().equals(*i32_type));
    
    return true;
}

bool test_symbol_registry_struct_types() {
    SymbolRegistry registry;
    
    // Create Player struct with methods
    auto i32_type = TypeFactory::i32();
    auto bool_type = TypeFactory::bool_type();
    
    std::vector<FieldInfo> fields = {
        FieldInfo("health", i32_type, 0),
        FieldInfo("alive", bool_type, 4)
    };
    
    std::vector<MethodInfo> methods = {
        MethodInfo("getHealth", i32_type),
        MethodInfo("isAlive", bool_type)
    };
    
    auto player_type = TypeFactory::create_struct("Player", fields, methods);
    
    // Add struct type (this should also add its methods)
    auto new_registry = registry.add_struct_type(player_type);
    
    // Look up the type
    auto type_symbol = new_registry.lookup("Player");
    assert(type_symbol.has_value());
    assert(type_symbol->kind() == Symbol::Kind::Type);
    
    // Look up a method
    auto method_symbol = new_registry.lookup_member_function("Player", "getHealth");
    assert(method_symbol.has_value());
    assert(method_symbol->name() == "Player::getHealth");
    assert(method_symbol->kind() == Symbol::Kind::Function);
    
    return true;
}

// === COMMAND STREAM TESTS ===

bool test_command_stream_basic() {
    CommandStream stream;
    
    // Create some values and commands
    auto i32_type = TypeFactory::i32();
    auto val1 = stream.next_value(i32_type);
    auto val2 = stream.next_value(i32_type);
    auto result = stream.next_value(i32_type);
    
    // Add commands
    stream.add_command(CommandFactory::constant_i32(val1, 42));
    stream.add_command(CommandFactory::constant_i32(val2, 24));
    stream.add_command(CommandFactory::add(result, val1, val2));
    
    assert(stream.size() == 3);
    assert(!stream.is_finalized());
    
    // Finalize stream
    stream.finalize();
    assert(stream.is_finalized());
    
    // Test commands
    assert(stream[0].opcode() == OpCode::ConstantI32);
    assert(stream[1].opcode() == OpCode::ConstantI32);
    assert(stream[2].opcode() == OpCode::Add);
    
    return true;
}

bool test_command_stream_serialization() {
    CommandStream stream;
    
    auto i32_type = TypeFactory::i32();
    auto val = stream.next_value(i32_type);
    stream.add_command(CommandFactory::constant_i32(val, 123));
    
    std::string serialized = stream.to_string();
    assert(serialized.find("const_i32") != std::string::npos);
    assert(serialized.find("123") != std::string::npos);
    
    return true;
}

// === IR BUILDER TESTS ===

bool test_ir_builder_basic() {
    CommandStream stream;
    
    // Create a simple function that returns a constant
    auto i32_type = TypeFactory::i32();
    auto void_type = TypeFactory::void_type();
    
    // Function declaration
    auto func_type = TypeFactory::create_function(i32_type, {});
    stream.add_command(CommandFactory::func_decl("test_func", func_type));
    
    // Function body - return constant
    auto const_val = stream.next_value(i32_type);
    stream.add_command(CommandFactory::constant_i32(const_val, 42));
    stream.add_command(CommandFactory::ret(const_val));
    
    stream.finalize();
    
    // Generate LLVM IR
    IRBuilder builder("TestModule");
    auto result = builder.build_ir(stream);
    
    assert(result.is_success());
    
    std::string ir = result.value().llvm_ir();
    std::cout << "\nGenerated LLVM IR:\n" << ir << "\n";
    
    // Check that IR contains expected elements
    assert(ir.find("define i32 @test_func") != std::string::npos);
    assert(ir.find("ret i32") != std::string::npos);
    
    return true;
}

bool test_ir_builder_with_control_flow() {
    CommandStream stream;
    
    auto i32_type = TypeFactory::i32();
    auto bool_type = TypeFactory::bool_type();
    
    // Function declaration: i32 test_func()
    auto func_type = TypeFactory::create_function(i32_type, {});
    stream.add_command(CommandFactory::func_decl("test_func", func_type));
    
    // Create values
    auto val1 = stream.next_value(i32_type);
    auto val2 = stream.next_value(i32_type);
    auto cmp_result = stream.next_value(bool_type);
    auto add_result = stream.next_value(i32_type);
    
    // Function body:
    // val1 = 10
    // val2 = 20
    // cmp_result = (val1 == val2)
    // if (cmp_result) goto then_label else goto else_label
    // then_label:
    //   add_result = val1 + val2
    //   return add_result
    // else_label:
    //   return val1
    
    stream.add_command(CommandFactory::constant_i32(val1, 10));
    stream.add_command(CommandFactory::constant_i32(val2, 20));
    stream.add_command(CommandFactory::icmp_eq(cmp_result, val1, val2));
    stream.add_command(CommandFactory::branch_cond(cmp_result, "then_label", "else_label"));
    
    stream.add_command(CommandFactory::label("then_label"));
    stream.add_command(CommandFactory::add(add_result, val1, val2));
    stream.add_command(CommandFactory::ret(add_result));
    
    stream.add_command(CommandFactory::label("else_label"));
    stream.add_command(CommandFactory::ret(val1));
    
    stream.finalize();
    
    // Generate LLVM IR
    IRBuilder builder("ControlFlowModule");
    auto result = builder.build_ir(stream);
    
    assert(result.is_success());
    
    std::string ir = result.value().llvm_ir();
    std::cout << "\nGenerated Control Flow LLVM IR:\n" << ir << "\n";
    
    // Check that IR contains expected elements
    assert(ir.find("br i1") != std::string::npos);  // conditional branch
    assert(ir.find("then_label:") != std::string::npos);
    assert(ir.find("else_label:") != std::string::npos);
    
    return true;
}

// === ERROR HANDLING TESTS ===

bool test_error_system() {
    // Test successful result
    auto success_result = success(42);
    assert(success_result.is_success());
    assert(success_result.value() == 42);
    
    // Test error result
    auto error_result = Result<int>(type_error("Test error message"));
    assert(error_result.is_error());
    assert(error_result.error().message() == "Test error message");
    
    // Test monadic operations
    auto doubled = success_result.and_then([](int value) {
        return value * 2;  // Return the value directly, not wrapped in Result
    });
    assert(doubled.is_success());
    assert(doubled.value() == 84);
    
    return true;
}

// === INTEGRATION TESTS ===

bool test_member_function_simulation() {
    // This test simulates member function calls without requiring full AST
    
    SymbolRegistry registry;
    
    // Create Player struct
    auto i32_type = TypeFactory::i32();
    auto bool_type = TypeFactory::bool_type();
    
    std::vector<FieldInfo> fields = {
        FieldInfo("health", i32_type, 0)
    };
    
    std::vector<MethodInfo> methods = {
        MethodInfo("getHealth", i32_type),
        MethodInfo("isAlive", bool_type)
    };
    
    auto player_type = TypeFactory::create_struct("Player", fields, methods);
    auto new_registry = registry.add_struct_type(player_type);
    
    // Simulate member function call: player.getHealth()
    CommandStream stream;
    
    // Allocate player object
    auto player_ptr_type = TypeFactory::create_pointer(player_type);
    auto player_ptr = stream.next_value(player_ptr_type);
    stream.add_command(CommandFactory::alloca(player_ptr, player_type));
    
    // Call member function
    auto health_result = stream.next_value(i32_type);
    stream.add_command(CommandFactory::call(health_result, "Player::getHealth", {player_ptr}));
    
    stream.finalize();
    
    // Verify we can look up the member function
    auto method_symbol = new_registry.lookup_member_function("Player", "getHealth");
    assert(method_symbol.has_value());
    
    // Verify command stream
    assert(stream.size() == 2);
    assert(stream[0].opcode() == OpCode::Alloca);
    assert(stream[1].opcode() == OpCode::Call);
    
    std::cout << "\nMember Function Call Commands:\n";
    std::cout << stream.to_string() << "\n";
    
    return true;
}

// === MAIN TEST RUNNER ===

int main() {
    std::cout << "🚀 Testing Optimal Myre Architecture\n";
    std::cout << "=====================================\n\n";
    
    TestRunner runner;
    
    // Type System Tests
    std::cout << "📦 Type System Tests:\n";
    runner.run_test("Primitive Types", test_primitive_types);
    runner.run_test("Struct Types", test_struct_types);
    runner.run_test("Function Types", test_function_types);
    runner.run_test("Pointer Types", test_pointer_types);
    
    // Symbol Registry Tests
    std::cout << "\n🗂️  Symbol Registry Tests:\n";
    runner.run_test("Basic Symbol Registry", test_symbol_registry_basic);
    runner.run_test("Struct Types in Registry", test_symbol_registry_struct_types);
    
    // Command Stream Tests
    std::cout << "\n⚡ Command Stream Tests:\n";
    runner.run_test("Basic Command Stream", test_command_stream_basic);
    runner.run_test("Command Stream Serialization", test_command_stream_serialization);
    
    // IR Builder Tests
    std::cout << "\n🔧 IR Builder Tests:\n";
    runner.run_test("Basic IR Generation", test_ir_builder_basic);
    runner.run_test("Control Flow IR Generation", test_ir_builder_with_control_flow);
    
    // Error Handling Tests
    std::cout << "\n❗ Error Handling Tests:\n";
    runner.run_test("Error System", test_error_system);
    
    // Integration Tests
    std::cout << "\n🔗 Integration Tests:\n";
    runner.run_test("Member Function Simulation", test_member_function_simulation);
    
    runner.print_summary();
    
    return 0;
}