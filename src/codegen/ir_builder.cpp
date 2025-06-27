#include "codegen/ir_builder.hpp"
#include "common/logger.hpp"
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
    LOG_DEBUG("Command stream (" + std::to_string(commands_.size()) + " commands):", LogCategory::CODEGEN);
    for (size_t i = 0; i < commands_.size(); ++i) {
        const auto& cmd = commands_[i];
        std::string line = "[" + std::to_string(i) + "] ";
        
        // Print operation
        switch (cmd.op) {
            case Op::Const: line += "Const"; break;
            case Op::Add: line += "Add"; break;
            case Op::Sub: line += "Sub"; break;
            case Op::Mul: line += "Mul"; break;
            case Op::Div: line += "Div"; break;
            case Op::Alloca: line += "Alloca"; break;
            case Op::Load: line += "Load"; break;
            case Op::Store: line += "Store"; break;
            case Op::Ret: line += "Ret"; break;
            case Op::RetVoid: line += "RetVoid"; break;
            case Op::FunctionBegin: line += "FunctionBegin"; break;
            case Op::FunctionEnd: line += "FunctionEnd"; break;
            case Op::Call: line += "Call"; break;
        }
        
        // Print result
        if (cmd.result.is_valid()) {
            line += " -> %" + std::to_string(cmd.result.id) + " (" + cmd.result.type.to_string() + ")";
        }
        
        // Print args
        if (!cmd.args.empty()) {
            line += " [";
            for (size_t j = 0; j < cmd.args.size(); ++j) {
                if (j > 0) line += ", ";
                line += "%" + std::to_string(cmd.args[j].id);
            }
            line += "]";
        }
        
        // Print immediate data
        if (!std::holds_alternative<std::monostate>(cmd.data)) {
            line += " ";
            if (auto* int_val = std::get_if<int64_t>(&cmd.data)) {
                line += std::to_string(*int_val);
            } else if (auto* bool_val = std::get_if<bool>(&cmd.data)) {
                line += (*bool_val ? "true" : "false");
            } else if (auto* float_val = std::get_if<double>(&cmd.data)) {
                line += std::to_string(*float_val);
            } else if (auto* str_val = std::get_if<std::string>(&cmd.data)) {
                line += "\"" + *str_val + "\"";
            }
        }
        
        LOG_DEBUG(line, LogCategory::CODEGEN);
    }
}

} // namespace Mycelium::Scripting::Lang