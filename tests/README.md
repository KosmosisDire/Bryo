# Mycelium Compiler Test Suite

This directory contains the unit tests for the Mycelium compiler system.

## Test Architecture

The test system is designed with the following principles:

1. **Multi-Level Testing**: Tests at different abstraction levels
   - Command Generation Tests: Test AST → IR Commands
   - IR Generation Tests: Test Commands → LLVM IR  
   - JIT Execution Tests: Test IR → Execution Results

2. **Lightweight Framework**: No external dependencies, built-in assertions and test management

3. **Colored Output**: Easy-to-read results with color-coded pass/fail indicators

4. **Comprehensive Reporting**: Final summary shows all results across all test suites

## Running Tests

```bash
# Build and run all tests
cmake --build build --target TestRunner
cd build && ./TestRunner
```

## Adding New Tests

### Option 1: Create a new test file

1. Create a new `.cpp` file in the `tests/` directory
2. Add it to `CMakeLists.txt` in the `TEST_FILES` list
3. Add a declaration and call in `test_runner.cpp`

Example structure:
```cpp
#include "test/test_framework.hpp"
#include "test/test_helpers.hpp"
// Include other necessary headers

using namespace Mycelium::Testing;
using namespace Mycelium::Scripting::Lang;

TestResult test_my_feature() {
    // Setup
    AstAllocator allocator;
    TestASTBuilder builder(allocator);
    
    // Test logic
    auto node = builder.create_int_literal(42);
    
    // Assertions
    ASSERT_TRUE(node != nullptr, "Should create valid node");
    ASSERT_EQ(42, get_literal_value(node), "Should have correct value");
    
    return TestResult(true);
}

void run_my_feature_tests() {
    TestSuite suite("My Feature Tests");
    
    suite.add_test("Basic Feature Test", test_my_feature);
    // Add more tests...
    
    suite.run_all();
}
```

### Option 2: Use the helper macros

```cpp
#include "test/test_suite_helpers.hpp"

void run_my_feature_tests() {
    CREATE_TEST_SUITE(My Feature Tests);
    
    ADD_TEST(test_basic_functionality);
    ADD_TEST(test_edge_cases);
    ADD_TEST(test_error_handling);
    
    RUN_SUITE();
}
```

## Test Categories

### Command Generation Tests (`test_command_generation.cpp`)
- Tests that AST nodes generate correct IR commands
- Verifies visitor pattern implementation
- Checks command parameters and types

### IR Generation Tests (`test_ir_generation.cpp`) 
- Tests that commands produce valid LLVM IR
- Verifies LLVM integration
- Handles optimization behaviors (e.g., constant folding)

### JIT Execution Tests (`test_jit_execution.cpp`)
- Tests end-to-end execution
- Verifies JIT compilation and execution
- Tests different function types and return values

## Available Assertions

```cpp
ASSERT_TRUE(condition, message)
ASSERT_FALSE(condition, message)
ASSERT_EQ(expected, actual, message)
ASSERT_STR_EQ(expected_string, actual_string, message)
ASSERT_NOT_EMPTY(container, message)
```

## Test Helpers

The `TestASTBuilder` class provides convenient methods for creating test AST nodes:

```cpp
AstAllocator allocator;
TestASTBuilder builder(allocator);

// Create basic nodes
auto identifier = builder.create_identifier("myVar");
auto type = builder.create_type_name("i32");
auto literal = builder.create_int_literal(42);

// Create complex structures
auto binary_expr = builder.create_binary_expression(
    left_expr, BinaryOperatorKind::Add, right_expr);
auto function = builder.create_simple_function("test", "i32", body);
```

## Best Practices

1. **Test One Thing**: Each test should verify one specific behavior
2. **Clear Names**: Use descriptive test names that explain what's being tested
3. **Good Error Messages**: Write helpful assertion messages
4. **Clean Setup**: Use test helpers to reduce boilerplate
5. **Test Edge Cases**: Include boundary conditions and error cases

## Example: Adding a New Language Feature Test

```cpp
// tests/test_new_feature.cpp
#include "test/test_framework.hpp"
#include "test/test_helpers.hpp"
#include "codegen/codegen.hpp"

using namespace Mycelium::Testing;
using namespace Mycelium::Scripting::Lang;

TestResult test_new_syntax() {
    AstAllocator allocator;
    TestASTBuilder builder(allocator);
    SymbolTable symbol_table;
    
    // Create test AST for new syntax
    auto node = builder.create_new_syntax_node();
    auto compilation_unit = builder.create_compilation_unit({node});
    
    // Generate and verify commands
    CodeGenerator generator(symbol_table);
    auto commands = generator.generate_code(compilation_unit);
    
    ASSERT_NOT_EMPTY(commands, "Should generate commands");
    ASSERT_TRUE(has_expected_command_type(commands), "Should generate correct command type");
    
    return TestResult(true);
}

void run_new_feature_tests() {
    TestSuite suite("New Feature Tests");
    suite.add_test("New Syntax Test", test_new_syntax);
    suite.run_all();
}
```

Don't forget to:
1. Add `tests/test_new_feature.cpp` to `CMakeLists.txt`
2. Add `void run_new_feature_tests();` declaration to `test_runner.cpp`
3. Add `run_new_feature_tests();` call to `main()` in `test_runner.cpp`