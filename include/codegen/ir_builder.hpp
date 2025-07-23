#pragma once
#include "codegen/ir_command.hpp"
#include <vector>
#include <unordered_map>

namespace Mycelium::Scripting::Lang {

class IRBuilder {
private:
    std::vector<Command> commands_;
    int next_id_;
    bool ignore_writes_;  // For analysis mode
    
    // Helper to emit commands
    ValueRef emit(Op op, IRType type, const std::vector<ValueRef>& args = {});
    ValueRef emit_with_data(Op op, IRType type, const std::vector<ValueRef>& args,
                           const std::variant<std::monostate, int64_t, bool, double, std::string, ICmpPredicate>& data);
    
public:
    IRBuilder();
    
    // Constants
    ValueRef const_i32(int32_t value);
    ValueRef const_i64(int64_t value);
    ValueRef const_bool(bool value);
    ValueRef const_f32(float value);
    ValueRef const_f64(double value);
    ValueRef const_null(IRType ptr_type);
    
    // Binary operations
    ValueRef add(ValueRef lhs, ValueRef rhs);
    ValueRef sub(ValueRef lhs, ValueRef rhs);
    ValueRef mul(ValueRef lhs, ValueRef rhs);
    ValueRef div(ValueRef lhs, ValueRef rhs);
    
    // Comparison operations
    ValueRef icmp(ICmpPredicate predicate, ValueRef lhs, ValueRef rhs);
    
    // Logical operations
    ValueRef logical_and(ValueRef lhs, ValueRef rhs);
    ValueRef logical_or(ValueRef lhs, ValueRef rhs);
    ValueRef logical_not(ValueRef operand);
    
    // Memory operations
    ValueRef alloca(IRType type);
    void store(ValueRef value, ValueRef ptr);
    ValueRef load(ValueRef ptr, IRType type);
    ValueRef gep(ValueRef ptr, const std::vector<int>& indices, IRType result_type);
    
    // Control flow
    void label(const std::string& name);
    void br(const std::string& target_label);
    void br_cond(ValueRef condition, const std::string& true_label, const std::string& false_label);
    void ret(ValueRef value);
    void ret_void();
    bool has_terminator() const;
    
    // Function management
    void function_begin(const std::string& name, IRType return_type, const std::vector<IRType>& param_types = {});
    void function_end();
    ValueRef call(const std::string& function_name, IRType return_type, const std::vector<ValueRef>& args);

    
    // Getters
    const std::vector<Command>& commands() const { return commands_; }
    void clear() { commands_.clear(); next_id_ = 1; }
    void set_ignore_writes(bool ignore) { ignore_writes_ = ignore; }
    
    // For debugging
    void dump_commands() const;
};

} // namespace Mycelium::Scripting::Lang