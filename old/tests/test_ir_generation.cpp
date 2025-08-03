#include "test/test_framework.hpp"
#include "codegen/command_processor.hpp"
#include "codegen/ir_command.hpp"
#include "common/logger.hpp"

using namespace Myre;

TestResult test_simple_function_ir() {
    // Create simple commands for a function that returns 42
    std::vector<Command> commands;
    
    // Function begin
    Command func_begin(Op::FunctionBegin, ValueRef::invalid(), {});
    func_begin.data = std::string("test:i32");
    commands.push_back(func_begin);
    
    // Constant 42
    Command const_cmd(Op::Const, ValueRef(1, IRType::i32()), {});
    const_cmd.data = static_cast<int64_t>(42);
    commands.push_back(const_cmd);
    
    // Return
    Command ret_cmd(Op::Ret, ValueRef::invalid(), {ValueRef(1, IRType::i32())});
    commands.push_back(ret_cmd);
    
    // Function end
    Command func_end(Op::FunctionEnd, ValueRef::invalid(), {});
    commands.push_back(func_end);
    
    // Process to IR
    std::string ir = CommandProcessor::process_to_ir_string(commands, "TestModule");
    
    // Debug: Print the actual generated IR
    LOG_DEBUG("\n=== Generated IR for Simple Function Test ===", LogCategory::TEST);
    LOG_DEBUG(ir, LogCategory::TEST);
    LOG_DEBUG("=== End Generated IR ===", LogCategory::TEST);
    
    ASSERT_FALSE(ir.empty(), "Should generate non-empty IR");
    
    // Check for basic IR structure
    ASSERT_TRUE(ir.find("define i32 @test()") != std::string::npos, 
                "Should contain function definition");
    ASSERT_TRUE(ir.find("ret i32") != std::string::npos, 
                "Should contain return statement");
    
    return TestResult(true);
}

TestResult test_void_function_ir() {
    std::vector<Command> commands;
    
    // Function begin
    Command func_begin(Op::FunctionBegin, ValueRef::invalid(), {});
    func_begin.data = std::string("void_test:void");
    commands.push_back(func_begin);
    
    // Return void
    Command ret_void(Op::RetVoid, ValueRef::invalid(), {});
    commands.push_back(ret_void);
    
    // Function end
    Command func_end(Op::FunctionEnd, ValueRef::invalid(), {});
    commands.push_back(func_end);
    
    // Process to IR
    std::string ir = CommandProcessor::process_to_ir_string(commands, "VoidTestModule");
    
    // Debug: Print the actual generated IR
    LOG_DEBUG("\n=== Generated IR for Void Function Test ===", LogCategory::TEST);
    LOG_DEBUG(ir, LogCategory::TEST);
    LOG_DEBUG("=== End Generated IR ===", LogCategory::TEST);
    
    ASSERT_FALSE(ir.empty(), "Should generate non-empty IR");
    ASSERT_TRUE(ir.find("define void @void_test()") != std::string::npos, 
                "Should contain void function definition");
    ASSERT_TRUE(ir.find("ret void") != std::string::npos, 
                "Should contain void return");
    
    return TestResult(true);
}

TestResult test_arithmetic_ir() {
    std::vector<Command> commands;
    
    // Function begin
    Command func_begin(Op::FunctionBegin, ValueRef::invalid(), {});
    func_begin.data = std::string("add_test:i32");
    commands.push_back(func_begin);
    
    // Constant 5
    Command const1(Op::Const, ValueRef(1, IRType::i32()), {});
    const1.data = static_cast<int64_t>(5);
    commands.push_back(const1);
    
    // Constant 3
    Command const2(Op::Const, ValueRef(2, IRType::i32()), {});
    const2.data = static_cast<int64_t>(3);
    commands.push_back(const2);
    
    // Add operation
    Command add_cmd(Op::Add, ValueRef(3, IRType::i32()), 
                   {ValueRef(1, IRType::i32()), ValueRef(2, IRType::i32())});
    commands.push_back(add_cmd);
    
    // Return result
    Command ret_cmd(Op::Ret, ValueRef::invalid(), {ValueRef(3, IRType::i32())});
    commands.push_back(ret_cmd);
    
    // Function end
    Command func_end(Op::FunctionEnd, ValueRef::invalid(), {});
    commands.push_back(func_end);
    
    // Process to IR
    std::string ir = CommandProcessor::process_to_ir_string(commands, "ArithmeticModule");
    
    // Debug: Print the actual generated IR
    LOG_DEBUG("\n=== Generated IR for Arithmetic Test ===", LogCategory::TEST);
    LOG_DEBUG(ir, LogCategory::TEST);
    LOG_DEBUG("=== End Generated IR ===", LogCategory::TEST);
    
    ASSERT_FALSE(ir.empty(), "Should generate non-empty IR");
    
    // Note: LLVM may optimize constant folding, so "5 + 3" becomes "8"
    // This is actually correct behavior - either the add instruction OR the optimized result should be present
    bool has_add_instruction = ir.find("add i32") != std::string::npos;
    bool has_optimized_result = ir.find("ret i32 8") != std::string::npos;
    
    ASSERT_TRUE(has_add_instruction || has_optimized_result, 
                "Should contain either add instruction or optimized constant result");
    
    return TestResult(true);
}

void run_ir_generation_tests() {
    TestSuite suite("IR Generation Tests");
    
    suite.add_test("Simple Function IR", test_simple_function_ir);
    suite.add_test("Void Function IR", test_void_function_ir);
    suite.add_test("Arithmetic IR", test_arithmetic_ir);
    
    suite.run_all();
}