#pragma once

#include "ast/ast.hpp"
#include "semantic/symbol_registry.hpp"
#include "semantic/error_system.hpp"
#include "codegen/ir_command.hpp"
#include <unordered_map>
#include <stack>

namespace Myre {

// === CODE GENERATION CONTEXT ===

class CodeGenContext {
private:
    std::shared_ptr<Scope> current_scope_;
    std::string current_function_;
    std::unordered_map<std::string, ValueRef> local_bindings_;
    std::stack<std::string> break_labels_;
    std::stack<std::string> continue_labels_;
    
public:
    CodeGenContext(std::shared_ptr<Scope> scope, std::string function_name)
        : current_scope_(std::move(scope))
        , current_function_(std::move(function_name)) {}
    
    // Immutable context updates
    CodeGenContext bind_value(const std::string& name, ValueRef value) const {
        CodeGenContext new_context = *this;
        new_context.local_bindings_[name] = value;
        return new_context;
    }
    
    std::optional<ValueRef> lookup_value(const std::string& name) const {
        auto it = local_bindings_.find(name);
        return it != local_bindings_.end() ? it->second : std::nullopt;
    }
    
    // Label generation
    std::string make_label(const std::string& prefix) const {
        static int counter = 0;
        return current_function_ + "_" + prefix + "_" + std::to_string(counter++);
    }
    
    // Getters
    const Scope& scope() const { return *current_scope_; }
    const std::string& current_function() const { return current_function_; }
};

// === CODE GENERATION RESULT ===

struct CodeGenResult {
    bool success;
    ValueRef value;
    std::string error_message;
    
    CodeGenResult(ValueRef v) : success(true), value(v) {}
    CodeGenResult(std::string error) : success(false), value(ValueRef::invalid()), error_message(std::move(error)) {}
    
    static CodeGenResult ok(ValueRef value) { return CodeGenResult(value); }
    static CodeGenResult error(const std::string& message) { return CodeGenResult(message); }
};

// === IR COMMAND BUILDER ===

class IRCommandBuilder : public StructuralVisitor {
private:
    CommandStream& stream_;
    const SymbolRegistry& registry_;
    CodeGenContext context_;
    ValueRef last_value_;  // Result of last expression
    
public:
    IRCommandBuilder(CommandStream& stream, 
                    const SymbolRegistry& registry,
                    CodeGenContext context)
        : stream_(stream), registry_(registry), context_(std::move(context))
        , last_value_(ValueRef::invalid()) {}
    
    // Main entry point
    Result<CommandStream> build_commands(CompilationUnitNode* root) {
        try {
            root->accept(this);
            stream_.finalize();
            return success(std::move(stream_));
        } catch (const std::exception& e) {
            return codegen_error("Code generation failed: " + std::string(e.what()));
        }
    }
    
    // AST Visitor Methods
    void visit(CompilationUnitNode* node) override {
        if (!node) return;
        
        for (const auto& stmt : node->statements) {
            stmt->accept(this);
        }
    }
    
    void visit(LiteralExpressionNode* node) override {
        if (!node) return;
        
        if (node->value.type == Token::Type::INTEGER_LITERAL) {
            int32_t value = std::stoi(node->value.value);
            auto result = stream_.next_value(TypeFactory::i32());
            stream_.add_command(CommandFactory::constant_i32(result, value));
            last_value_ = result;
        }
        else if (node->value.type == Token::Type::BOOLEAN_LITERAL) {
            bool value = (node->value.value == "true");
            auto result = stream_.next_value(TypeFactory::bool_type());
            stream_.add_command(CommandFactory::constant_bool(result, value));
            last_value_ = result;
        }
        else {
            throw std::runtime_error("Unsupported literal type");
        }
    }
    
    void visit(BinaryExpressionNode* node) override {
        if (!node) return;
        
        // Generate left operand
        node->left->accept(this);
        ValueRef lhs = last_value_;
        
        // Generate right operand  
        node->right->accept(this);
        ValueRef rhs = last_value_;
        
        if (!lhs.is_valid() || !rhs.is_valid()) {
            throw std::runtime_error("Invalid operands for binary expression");
        }
        
        // Generate operation based on operator
        auto result = stream_.next_value(lhs.type_ptr());  // Assume same type for now
        
        switch (node->op.type) {
            case Token::Type::PLUS:
                stream_.add_command(CommandFactory::add(result, lhs, rhs));
                break;
            case Token::Type::EQUAL_EQUAL:
                result = stream_.next_value(TypeFactory::bool_type());
                stream_.add_command(CommandFactory::icmp_eq(result, lhs, rhs));
                break;
            // TODO: Add other operators
            default:
                throw std::runtime_error("Unsupported binary operator");
        }
        
        last_value_ = result;
    }
    
    void visit(IdentifierExpressionNode* node) override {
        if (!node) return;
        
        // Check local bindings first
        auto local_value = context_.lookup_value(node->name->name);
        if (local_value) {
            last_value_ = *local_value;
            return;
        }
        
        // Look up in symbol registry
        auto symbol = registry_.lookup(node->name->name);
        if (!symbol) {
            throw std::runtime_error("Symbol not found: " + std::string(node->name->name));
        }
        
        if (symbol->kind() == Symbol::Kind::Variable) {
            // Load variable value
            // For now, assume it's already allocated somewhere
            // TODO: Implement proper variable storage
            auto result = stream_.next_value(symbol->type_ptr());
            last_value_ = result;
        }
        else {
            throw std::runtime_error("Cannot use " + symbol->kind_string() + " as value");
        }
    }
    
    void visit(CallExpressionNode* node) override {
        if (!node) return;
        
        // Handle member function calls
        if (auto* member_access = node->target->as<MemberAccessExpressionNode>()) {
            generate_member_function_call(*member_access, *node);
            return;
        }
        
        // Handle regular function calls
        if (auto* identifier = node->target->as<IdentifierExpressionNode>()) {
            generate_regular_function_call(*identifier, *node);
            return;
        }
        
        throw std::runtime_error("Unsupported call target");
    }
    
    void visit(MemberAccessExpressionNode* node) override {
        if (!node) return;
        
        // Generate target object
        node->target->accept(this);
        ValueRef target = last_value_;
        
        if (!target.is_valid()) {
            throw std::runtime_error("Invalid target for member access");
        }
        
        // Get target type
        if (target.type().kind() != Type::Kind::Pointer) {
            throw std::runtime_error("Member access requires pointer type");
        }
        
        const auto& pointer_type = static_cast<const PointerType&>(target.type());
        if (pointer_type.pointee_type().kind() != Type::Kind::Struct) {
            throw std::runtime_error("Member access requires struct pointer");
        }
        
        const auto& struct_type = static_cast<const StructType&>(pointer_type.pointee_type());
        
        // Find field
        auto field = struct_type.find_field(node->member->name);
        if (!field) {
            throw std::runtime_error("Field not found: " + std::string(node->member->name));
        }
        
        // Generate GEP for field access
        auto index = stream_.next_value(TypeFactory::i32());
        stream_.add_command(CommandFactory::constant_i32(index, field->offset));
        
        auto field_ptr = stream_.next_value(TypeFactory::create_pointer(field->type));
        stream_.add_command(CommandFactory::gep(field_ptr, target, index));
        
        // Load field value
        auto result = stream_.next_value(field->type);
        stream_.add_command(CommandFactory::load(result, field_ptr));
        
        last_value_ = result;
    }
    
    void visit(ReturnStatementNode* node) override {
        if (!node) return;
        
        if (node->expression) {
            node->expression->accept(this);
            stream_.add_command(CommandFactory::ret(last_value_));
        } else {
            stream_.add_command(CommandFactory::ret_void());
        }
    }
    
    void visit(IfStatementNode* node) override {
        if (!node) return;
        
        // Generate condition
        node->condition->accept(this);
        ValueRef condition = last_value_;
        
        if (!condition.is_valid()) {
            throw std::runtime_error("Invalid condition for if statement");
        }
        
        // Generate labels
        std::string then_label = context_.make_label("if_then");
        std::string end_label = context_.make_label("if_end");
        
        // Conditional branch
        stream_.add_command(CommandFactory::branch_cond(condition, then_label, end_label));
        
        // Then block
        stream_.add_command(CommandFactory::label(then_label));
        node->then_statement->accept(this);
        stream_.add_command(CommandFactory::branch(end_label));
        
        // End label
        stream_.add_command(CommandFactory::label(end_label));
    }
    
    // TODO: Implement other visitor methods as needed
    
private:
    void generate_member_function_call(const MemberAccessExpressionNode& member_access,
                                     const CallExpressionNode& call) {
        // Generate target object (this parameter)
        member_access.target->accept(this);
        ValueRef this_ptr = last_value_;
        
        if (!this_ptr.is_valid()) {
            throw std::runtime_error("Invalid 'this' pointer for member function call");
        }
        
        // Determine target type
        std::string target_type_name;
        if (this_ptr.type().kind() == Type::Kind::Pointer) {
            const auto& pointer_type = static_cast<const PointerType&>(this_ptr.type());
            if (pointer_type.pointee_type().kind() == Type::Kind::Struct) {
                target_type_name = pointer_type.pointee_type().name();
            }
        }
        
        if (target_type_name.empty()) {
            throw std::runtime_error("Cannot determine target type for member function call");
        }
        
        // Look up member function
        std::string method_name = std::string(member_access.member->name);
        auto method_symbol = registry_.lookup_member_function(target_type_name, method_name);
        if (!method_symbol) {
            throw std::runtime_error("Member function not found: " + target_type_name + "::" + method_name);
        }
        
        // Generate arguments
        std::vector<ValueRef> args = {this_ptr};  // Start with 'this'
        
        for (const auto& arg : call.arguments) {
            arg->accept(this);
            if (!last_value_.is_valid()) {
                throw std::runtime_error("Invalid argument in member function call");
            }
            args.push_back(last_value_);
        }
        
        // Generate call
        std::string qualified_name = target_type_name + "::" + method_name;
        
        const auto& func_type = static_cast<const FunctionType&>(method_symbol->type());
        if (func_type.return_type().name() == "void") {
            stream_.add_command(CommandFactory::call(ValueRef::invalid(), qualified_name, args));
            last_value_ = ValueRef::invalid();
        } else {
            auto result = stream_.next_value(std::const_pointer_cast<Type>(func_type.return_type().type_ptr()));
            stream_.add_command(CommandFactory::call(result, qualified_name, args));
            last_value_ = result;
        }
    }
    
    void generate_regular_function_call(const IdentifierExpressionNode& identifier,
                                      const CallExpressionNode& call) {
        // Look up function
        auto symbol = registry_.lookup(identifier.name->name);
        if (!symbol || symbol->kind() != Symbol::Kind::Function) {
            throw std::runtime_error("Function not found: " + std::string(identifier.name->name));
        }
        
        // Generate arguments
        std::vector<ValueRef> args;
        for (const auto& arg : call.arguments) {
            arg->accept(this);
            if (!last_value_.is_valid()) {
                throw std::runtime_error("Invalid argument in function call");
            }
            args.push_back(last_value_);
        }
        
        // Generate call
        const auto& func_type = static_cast<const FunctionType&>(symbol->type());
        if (func_type.return_type().name() == "void") {
            stream_.add_command(CommandFactory::call(ValueRef::invalid(), identifier.name->name, args));
            last_value_ = ValueRef::invalid();
        } else {
            auto result = stream_.next_value(std::const_pointer_cast<Type>(func_type.return_type().type_ptr()));
            stream_.add_command(CommandFactory::call(result, identifier.name->name, args));
            last_value_ = result;
        }
    }
};

} // namespace Myre