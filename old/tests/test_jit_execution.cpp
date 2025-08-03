#include "test/test_framework.hpp"
#include "codegen/jit_engine.hpp"

using namespace Myre;

TestResult test_simple_jit_execution() {
    // Simple IR that returns 42
    std::string ir = R"(
define i32 @test() {
entry:
  ret i32 42
}
)";
    
    JITEngine jit;
    ASSERT_TRUE(jit.initialize_from_ir(ir, "TestModule"), 
                "Should initialize JIT engine with simple IR");
    
    int result = jit.execute_function("test");
    ASSERT_EQ(42, result, "Function should return 42");
    
    return TestResult(true);
}

TestResult test_arithmetic_jit_execution() {
    // IR that adds two numbers
    std::string ir = R"(
define i32 @add_numbers() {
entry:
  %1 = add i32 10, 20
  %2 = add i32 %1, 5
  ret i32 %2
}
)";
    
    JITEngine jit;
    ASSERT_TRUE(jit.initialize_from_ir(ir, "ArithmeticModule"), 
                "Should initialize JIT engine with arithmetic IR");
    
    int result = jit.execute_function("add_numbers");
    ASSERT_EQ(35, result, "Function should return 35 (10+20+5)");
    
    return TestResult(true);
}

TestResult test_void_function_jit() {
    // Simple void function
    std::string ir = R"(
define void @void_test() {
entry:
  ret void
}
)";
    
    JITEngine jit;
    ASSERT_TRUE(jit.initialize_from_ir(ir, "VoidModule"), 
                "Should initialize JIT engine with void function IR");
    
    int result = jit.execute_function("void_test");
    ASSERT_EQ(0, result, "Void function should return 0");
    
    return TestResult(true);
}

TestResult test_multiple_functions_jit() {
    // Multiple functions in one module
    std::string ir = R"(
define i32 @func1() {
entry:
  ret i32 100
}

define i32 @func2() {
entry:
  ret i32 200
}
)";
    
    JITEngine jit;
    ASSERT_TRUE(jit.initialize_from_ir(ir, "MultiModule"), 
                "Should initialize JIT engine with multiple functions");
    
    int result1 = jit.execute_function("func1");
    ASSERT_EQ(100, result1, "func1 should return 100");
    
    int result2 = jit.execute_function("func2");
    ASSERT_EQ(200, result2, "func2 should return 200");
    
    return TestResult(true);
}

void run_jit_execution_tests() {
    TestSuite suite("JIT Execution Tests");
    
    suite.add_test("Simple JIT Execution", test_simple_jit_execution);
    suite.add_test("Arithmetic JIT Execution", test_arithmetic_jit_execution);
    suite.add_test("Void Function JIT", test_void_function_jit);
    suite.add_test("Multiple Functions JIT", test_multiple_functions_jit);
    
    suite.run_all();
}