#include "codegen/ir_builder.hpp"
#include <iostream>

namespace Mycelium::Scripting::Lang {

IRBuilder::IRBuilder() : next_id_(1), ignore_writes_(false) {
}

ValueRef IRBuilder::emit(Op op, IRType type, const std::vector<ValueRef>& args) {
    if (ignore_writes_) {
        // In analysis mode, just return a fake value
        return ValueRef(-next_id_++, type);
    }
    
    ValueRef result = (type.kind == IRType::Kind::Void) ? 
        ValueRef::invalid() : ValueRef(next_id_++, type);
    
    Command cmd(op, result, args);
    commands_.push_back(cmd);
    
    return result;
}

ValueRef IRBuilder::emit_with_data(Op op, IRType type, const std::vector<ValueRef>& args,
                                   const std::variant<std::monostate, int64_t, bool, double, std::string>& data) {
    if (ignore_writes_) {
        // In analysis mode, just return a fake value
        return ValueRef(-next_id_++, type);
    }
    
    ValueRef result = (type.kind == IRType::Kind::Void) ? 
        ValueRef::invalid() : ValueRef(next_id_++, type);
    
    Command cmd(op, result, args);
    cmd.data = data;
    commands_.push_back(cmd);
    
    return result;
}

// Constants
ValueRef IRBuilder::const_i32(int32_t value) {
    return emit_with_data(Op::Const, IRType::i32(), {}, static_cast<int64_t>(value));
}

ValueRef IRBuilder::const_i64(int64_t value) {
    return emit_with_data(Op::Const, IRType::i64(), {}, value);
}

ValueRef IRBuilder::const_bool(bool value) {
    return emit_with_data(Op::Const, IRType::bool_(), {}, value);
}

ValueRef IRBuilder::const_f32(float value) {
    return emit_with_data(Op::Const, IRType::f32(), {}, static_cast<double>(value));
}

ValueRef IRBuilder::const_f64(double value) {
    return emit_with_data(Op::Const, IRType::f64(), {}, value);
}

// Binary operations
ValueRef IRBuilder::add(ValueRef lhs, ValueRef rhs) {
    // Basic type checking
    if (lhs.type != rhs.type) {
        std::cerr << "Type mismatch in add operation\n";
        return ValueRef::invalid();
    }
    
    return emit(Op::Add, lhs.type, {lhs, rhs});
}

ValueRef IRBuilder::sub(ValueRef lhs, ValueRef rhs) {
    if (lhs.type != rhs.type) {
        std::cerr << "Type mismatch in sub operation\n";
        return ValueRef::invalid();
    }
    
    return emit(Op::Sub, lhs.type, {lhs, rhs});
}

ValueRef IRBuilder::mul(ValueRef lhs, ValueRef rhs) {
    if (lhs.type != rhs.type) {
        std::cerr << "Type mismatch in mul operation\n";
        return ValueRef::invalid();
    }
    
    return emit(Op::Mul, lhs.type, {lhs, rhs});
}

ValueRef IRBuilder::div(ValueRef lhs, ValueRef rhs) {
    if (lhs.type != rhs.type) {
        std::cerr << "Type mismatch in div operation\n";
        return ValueRef::invalid();
    }
    
    return emit(Op::Div, lhs.type, {lhs, rhs});
}

// Memory operations
ValueRef IRBuilder::alloca(IRType type) {
    return emit_with_data(Op::Alloca, IRType::ptr(), {}, std::string(type.to_string()));
}

void IRBuilder::store(ValueRef value, ValueRef ptr) {
    if (ptr.type.kind != IRType::Kind::Ptr) {
        std::cerr << "Store target must be a pointer\n";
        return;
    }
    
    emit(Op::Store, IRType::void_(), {value, ptr});
}

ValueRef IRBuilder::load(ValueRef ptr, IRType type) {
    if (ptr.type.kind != IRType::Kind::Ptr) {
        std::cerr << "Load source must be a pointer\n";
        return ValueRef::invalid();
    }
    
    return emit(Op::Load, type, {ptr});
}

// Control flow
void IRBuilder::ret(ValueRef value) {
    emit(Op::Ret, IRType::void_(), {value});
}

void IRBuilder::ret_void() {
    emit(Op::RetVoid, IRType::void_(), {});
}

// Function management
void IRBuilder::function_begin(const std::string& name, IRType return_type) {
    emit_with_data(Op::FunctionBegin, IRType::void_(), {}, name + ":" + return_type.to_string());
}

void IRBuilder::function_end() {
    emit(Op::FunctionEnd, IRType::void_(), {});
}

// For debugging
void IRBuilder::dump_commands() const {
    std::cout << "Command stream (" << commands_.size() << " commands):\n";
    for (size_t i = 0; i < commands_.size(); ++i) {
        const auto& cmd = commands_[i];
        std::cout << "[" << i << "] ";
        
        // Print operation
        switch (cmd.op) {
            case Op::Const: std::cout << "Const"; break;
            case Op::Add: std::cout << "Add"; break;
            case Op::Sub: std::cout << "Sub"; break;
            case Op::Mul: std::cout << "Mul"; break;
            case Op::Div: std::cout << "Div"; break;
            case Op::Alloca: std::cout << "Alloca"; break;
            case Op::Load: std::cout << "Load"; break;
            case Op::Store: std::cout << "Store"; break;
            case Op::Ret: std::cout << "Ret"; break;
            case Op::RetVoid: std::cout << "RetVoid"; break;
            case Op::FunctionBegin: std::cout << "FunctionBegin"; break;
            case Op::FunctionEnd: std::cout << "FunctionEnd"; break;
            case Op::Call: std::cout << "Call"; break;
        }
        
        // Print result
        if (cmd.result.is_valid()) {
            std::cout << " -> %" << cmd.result.id << " (" << cmd.result.type.to_string() << ")";
        }
        
        // Print args
        if (!cmd.args.empty()) {
            std::cout << " [";
            for (size_t j = 0; j < cmd.args.size(); ++j) {
                if (j > 0) std::cout << ", ";
                std::cout << "%" << cmd.args[j].id;
            }
            std::cout << "]";
        }
        
        // Print immediate data
        if (!std::holds_alternative<std::monostate>(cmd.data)) {
            std::cout << " ";
            if (auto* int_val = std::get_if<int64_t>(&cmd.data)) {
                std::cout << *int_val;
            } else if (auto* bool_val = std::get_if<bool>(&cmd.data)) {
                std::cout << (*bool_val ? "true" : "false");
            } else if (auto* float_val = std::get_if<double>(&cmd.data)) {
                std::cout << *float_val;
            } else if (auto* str_val = std::get_if<std::string>(&cmd.data)) {
                std::cout << "\"" << *str_val << "\"";
            }
        }
        
        std::cout << "\n";
    }
}

} // namespace Mycelium::Scripting::Lang