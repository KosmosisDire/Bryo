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
                           const std::variant<std::monostate, int64_t, bool, double, std::string>& data);
    
public:
    IRBuilder();
    
    // Constants
    ValueRef const_i32(int32_t value);
    ValueRef const_i64(int64_t value);
    ValueRef const_bool(bool value);
    ValueRef const_f32(float value);
    ValueRef const_f64(double value);
    
    // Binary operations
    ValueRef add(ValueRef lhs, ValueRef rhs);
    ValueRef sub(ValueRef lhs, ValueRef rhs);
    ValueRef mul(ValueRef lhs, ValueRef rhs);
    ValueRef div(ValueRef lhs, ValueRef rhs);
    
    // Memory operations
    ValueRef alloca(IRType type);
    void store(ValueRef value, ValueRef ptr);
    ValueRef load(ValueRef ptr, IRType type);
    
    // Control flow
    void ret(ValueRef value);
    void ret_void();
    
    // Function management
    void function_begin(const std::string& name, IRType return_type);
    void function_end();
    
    // Getters
    const std::vector<Command>& commands() const { return commands_; }
    void clear() { commands_.clear(); next_id_ = 1; }
    void set_ignore_writes(bool ignore) { ignore_writes_ = ignore; }
    
    // For debugging
    void dump_commands() const;
};

} // namespace Mycelium::Scripting::Lang