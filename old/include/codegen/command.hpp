#pragma once

#include "codegen/ir_value.hpp"
#include <vector>
#include <string>

namespace Myre {

// Simple operation codes for IR commands
enum class Op {
    // Constants
    ConstI32,      // Load i32 constant
    ConstBool,     // Load boolean constant
    
    // Memory
    Alloca,        // Allocate stack variable
    Store,         // Store value to memory
    Load,          // Load value from memory
    
    // Arithmetic
    Add,           // Add two integers
    Sub,           // Subtract two integers
    Mul,           // Multiply two integers
    Div,           // Divide two integers
    Neg,           // Negate integer (unary minus)
    
    // Comparison  
    ICmpEQ,        // Integer compare equal
    ICmpNE,        // Integer compare not equal
    ICmpLT,        // Integer compare less than (signed)
    ICmpGT,        // Integer compare greater than (signed)
    ICmpLE,        // Integer compare less than or equal (signed)
    ICmpGE,        // Integer compare greater than or equal (signed)
    
    // Logical
    And,           // Logical AND
    Or,            // Logical OR
    Not,           // Logical NOT
    
    // Control flow
    Label,         // Basic block label
    Br,            // Unconditional branch
    BrCond,        // Conditional branch
    Ret,           // Return from function
    
    // Functions
    Call,          // Function call
    FuncDecl,      // Function declaration
};

// Simple IR command structure
struct Command {
    Op op;
    ValueRef result;  // Result value (invalid if no result)
    std::vector<ValueRef> args;  // Arguments
    int32_t immediate;  // For constants and other immediate values
    std::string label;  // For labels, branches, and function names
    
    Command(Op operation, ValueRef res = ValueRef::invalid())
        : op(operation), result(res), immediate(0) {}
    
    Command(Op operation, ValueRef res, const std::vector<ValueRef>& arguments, 
            int32_t imm = 0, const std::string& lbl = "")
        : op(operation), result(res), args(arguments), immediate(imm), label(lbl) {}
    
    std::string op_name() const {
        switch (op) {
            case Op::ConstI32: return "const_i32";
            case Op::ConstBool: return "const_bool";
            case Op::Alloca: return "alloca";
            case Op::Store: return "store";
            case Op::Load: return "load";
            case Op::Add: return "add";
            case Op::Sub: return "sub";
            case Op::Mul: return "mul";
            case Op::Div: return "div";
            case Op::Neg: return "neg";
            case Op::ICmpEQ: return "icmp_eq";
            case Op::ICmpNE: return "icmp_ne";
            case Op::ICmpLT: return "icmp_lt";
            case Op::ICmpGT: return "icmp_gt";
            case Op::ICmpLE: return "icmp_le";
            case Op::ICmpGE: return "icmp_ge";
            case Op::And: return "and";
            case Op::Or: return "or";
            case Op::Not: return "not";
            case Op::Label: return "label";
            case Op::Br: return "br";
            case Op::BrCond: return "br_cond";
            case Op::Ret: return "ret";
            case Op::Call: return "call";
            case Op::FuncDecl: return "func_decl";
        }
        return "unknown";
    }
    
    std::string to_string() const {
        std::string result_str;
        
        if (result.is_valid()) {
            result_str = result.to_string() + " = ";
        }
        
        result_str += op_name();
        
        if (!args.empty()) {
            result_str += " ";
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) result_str += ", ";
                result_str += args[i].to_string();
            }
        }
        
        if (immediate != 0) {
            result_str += " " + std::to_string(immediate);
        }
        
        if (!label.empty()) {
            result_str += " " + label;
        }
        
        return result_str;
    }
};

} // namespace Myre