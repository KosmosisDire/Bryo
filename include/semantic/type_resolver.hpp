#pragma once

#include "symbol_table.hpp"
#include "type_system.hpp"
#include "ast/ast.hpp"

using TypePtr = std::shared_ptr<Type>;

enum class ConstraintKind {
    Equals,     // T1 = T2 (start with just this)
    // Future: Subtype, HasMember, etc.
};

struct Constraint {
    ConstraintKind kind;
    TypePtr left_type;
    TypePtr right_type;
};

class TypeResolver {
private:
    SymbolTable& symbolTable;
    TypeSystem& typeSystem;
    
    // Temporary constraint data (only exists during inference)
    std::vector<Constraint> constraints;
    std::vector<std::string> errors;
    
public:
    TypeResolver(SymbolTable& symbol_table) 
        : symbolTable(symbol_table), typeSystem(symbol_table.get_type_system()) {}
    
    // Main entry point - does everything
    bool resolve_types() {
        constraints.clear();
        errors.clear();
        
        int max_iterations = 10;
        int iteration = 0;
        
        while (iteration < max_iterations) {
            size_t initial_constraints = constraints.size();
            size_t initial_unresolved = symbolTable.get_unresolved_symbols().size();
            
            // Generate new constraints
            generate_constraints();
            
            // Solve constraints
            solve_constraints();
            
            // Apply solutions
            apply_solution();
            
            // Check if we made progress
            size_t final_constraints = constraints.size();
            size_t final_unresolved = symbolTable.get_unresolved_symbols().size();
            
            if (final_unresolved == initial_unresolved && final_constraints == initial_constraints) {
                // No progress made, stop iterating
                break;
            }
            
            iteration++;
        }
        
        return true;
    }
    
    const std::vector<std::string>& get_errors() const { return errors; }
    std::string to_string()
    {
        std::string result = "TypeResolver State:\n";
        result += "Constraints:\n";
        for (const auto& constraint : constraints) {
            result += "  ";
            switch (constraint.kind) {
                case ConstraintKind::Equals:
                    result += constraint.left_type->get_name() + " == " + constraint.right_type->get_name();
                    break;
                // Future: Handle other kinds
            }
            result += "\n";
        }
        result += "Errors:\n";
        for (const auto& err : errors) {
            result += "  " + err + "\n";
        }
        return result;
    }
private:
    bool generate_constraints() {
        bool any_new_constraints = false;
        
        for (auto symbol : symbolTable.get_unresolved_symbols()) {
            // Skip if we already have a constraint for this symbol
            bool already_has_constraint = false;
            for (const auto& constraint : constraints) {
                if (constraint.left_type == std::get<VariableInfo>(symbol->data).type) {
                    already_has_constraint = true;
                    break;
                }
            }
            
            if (!already_has_constraint) {
                if (generate_constraints_for_symbol(symbol)) {
                    any_new_constraints = true;
                }
            }
        }

        return any_new_constraints;
    }
    
    bool generate_constraints_for_symbol(SymbolPtr symbol) {
        auto& var_info = std::get<VariableInfo>(symbol->data);
        auto& unresolved = std::get<UnresolvedType>(var_info.type->value);
        
        if (!unresolved.can_infer()) {
            errors.push_back("Symbol " + symbol->name + " marked as unresolved but has no initializer, type name, or defining scope");
            return false;
        }

        TypePtr expr_type = analyze_type_name(unresolved.type_name, unresolved.defining_scope);
        if (!expr_type)
        {
            expr_type = analyze_expression(unresolved.initializer, unresolved.defining_scope);
            if (!expr_type) return false;
        }
        
        add_constraint(var_info.type, expr_type);
        return true;
    }

    TypePtr analyze_type_name(TypeNameNode* type_name, ScopePtr scope) {
        if (!type_name || type_name->name->identifiers.size == 0) return nullptr;
        
        // Resolve type name in current scope
        std::string type_name_str(type_name->get_full_name());
        TypePtr resolved_type = symbolTable.resolve_type_name(type_name_str, scope, true);
        
        if (!resolved_type) {
            errors.push_back("Unresolved type: " + type_name_str);
            return nullptr;
        }
        
        return resolved_type;
    }
    
    TypePtr analyze_expression(ExpressionNode* expr, ScopePtr scope) {
        if (expr->is_a<LiteralExpressionNode>()) {
            return analyze_literal(expr->as<LiteralExpressionNode>());
        }
        else if (expr->is_a<NewExpressionNode>()) {
            return analyze_new_expression(expr->as<NewExpressionNode>(), scope);
        }
        else if (expr->is_a<CallExpressionNode>()) {
            return analyze_call_expression(expr->as<CallExpressionNode>(), scope);
        }
        else if (expr->is_a<MemberAccessExpressionNode>()) {
            return analyze_member_access(expr->as<MemberAccessExpressionNode>(), scope);
        }
        else if (expr->is_a<IdentifierExpressionNode>()){
            return analyze_identifier(expr->as<IdentifierExpressionNode>(), scope);
        }
        else if (expr->is_a<BinaryExpressionNode>()) {
            return analyze_binary_expression(expr->as<BinaryExpressionNode>(), scope);
        }

        return nullptr; // Unknown expression type
    }

    TypePtr analyze_literal(LiteralExpressionNode* lit) {
        return typeSystem.get_primitive(std::string(Myre::to_string(lit->kind)));
    }
    
    TypePtr analyze_new_expression(NewExpressionNode* new_expr, ScopePtr scope)
    {
        // TODO: Handle constructor call validation later
        return symbolTable.resolve_type_name(new_expr->type->get_full_name(), scope, true);
    }
    
    TypePtr analyze_binary_expression(BinaryExpressionNode* binary, ScopePtr scope) {
        // Analyze left operand
        if (!binary->left || !binary->left->is_a<ExpressionNode>()) {
            errors.push_back("Invalid left operand in binary expression");
            return nullptr;
        }
        TypePtr left_type = analyze_expression(binary->left->as<ExpressionNode>(), scope);
        if (!left_type) {
            errors.push_back("Could not resolve left operand type");
            return nullptr;
        }
        
        // Analyze right operand  
        if (!binary->right || !binary->right->is_a<ExpressionNode>()) {
            errors.push_back("Invalid right operand in binary expression");
            return nullptr;
        }
        TypePtr right_type = analyze_expression(binary->right->as<ExpressionNode>(), scope);
        if (!right_type) {
            errors.push_back("Could not resolve right operand type");
            return nullptr;
        }
        
        // Resolve through constraints if needed
        left_type = resolve_through_constraints(left_type);
        right_type = resolve_through_constraints(right_type);
        
        // For now, implement basic arithmetic rules
        // TODO: Add proper operator overloading and type checking
        switch (binary->opKind) {
            // Arithmetic operators - if both operands are same numeric type, result is that type
            case BinaryOperatorKind::Add:
            case BinaryOperatorKind::Subtract:
            case BinaryOperatorKind::Multiply:
            case BinaryOperatorKind::Divide:
            case BinaryOperatorKind::Modulo:
                if (left_type->get_name() == right_type->get_name()) {
                    return left_type; // Same type, return that type
                }
                errors.push_back("Type mismatch in binary expression: " + left_type->get_name() + " and " + right_type->get_name());
                return nullptr;
                
            // Comparison operators - return bool
            case BinaryOperatorKind::Equals:
            case BinaryOperatorKind::NotEquals:
            case BinaryOperatorKind::LessThan:
            case BinaryOperatorKind::LessThanOrEqual:
            case BinaryOperatorKind::GreaterThan:
            case BinaryOperatorKind::GreaterThanOrEqual:
                return typeSystem.get_primitive("bool");
                
            // Logical operators - expect bool operands, return bool
            case BinaryOperatorKind::LogicalAnd:
            case BinaryOperatorKind::LogicalOr:
                return typeSystem.get_primitive("bool");
                
            default:
                errors.push_back("Unsupported binary operator");
                return nullptr;
        }
    }
    
    TypePtr resolve_through_constraints(TypePtr type) {
        // If this type has a constraint, follow it to the resolved type
        for (const auto& constraint : constraints) {
            if (constraint.left_type == type && constraint.kind == ConstraintKind::Equals) {
                return constraint.right_type;
            }
        }
        return type; // Return original if no constraint found
    }
    
    TypePtr analyze_identifier(IdentifierExpressionNode* ident, ScopePtr scope) {
        std::string var_name(ident->identifier->name);
        
        // Look up the identifier in the current scope
        SymbolPtr symbol = scope->lookup(var_name);
        if (!symbol) {
            errors.push_back("Undefined identifier: " + var_name);
            return nullptr;
        }
        // Check if the symbol has VariableInfo or FunctionInfo before getting the type
        if (std::holds_alternative<VariableInfo>(symbol->data)) {
            auto& var_info = std::get<VariableInfo>(symbol->data);
            return var_info.type;
        } else if (std::holds_alternative<FunctionInfo>(symbol->data)) {
            auto& func_info = std::get<FunctionInfo>(symbol->data);
            return func_info.return_type;
        }

        errors.push_back("Identifier '" + var_name + "' is not a variable or function");
        return nullptr;
    }
    
    TypePtr analyze_call_expression(CallExpressionNode* call, ScopePtr scope) {
        // Analyze the target being called
        TypePtr target_type = analyze_expression(call->target, scope);
        if (!target_type) {
            errors.push_back("Could not resolve call target");
            return nullptr;
        }
        
        // For method calls, target_type will be the method's return type
        // For direct function calls, we need different logic
        return target_type;
    }

    TypePtr analyze_member_access(MemberAccessExpressionNode* member, ScopePtr scope) {
        // First, resolve the object being accessed
        TypePtr object_type = analyze_expression(member->target, scope);
        if (!object_type) {
            errors.push_back("Could not resolve member access target");
            return nullptr;
        }
        
        // Try to resolve through constraints if it's unresolved
        object_type = resolve_through_constraints(object_type);
        
        // Get the method name
        std::string method_name(member->member->name);
        
        // Look up the type symbol to get its scope
        SymbolPtr type_symbol = symbolTable.lookup(object_type->get_name());
        if (!type_symbol) {
            errors.push_back("Could not find type symbol for: " + object_type->get_name());
            return nullptr;
        }
        
        auto& type_info = std::get<TypeInfo>(type_symbol->data);
        
        // Look up the method in the type's body scope
        SymbolPtr method_symbol = type_info.body_scope->lookup_local(method_name);
        if (!method_symbol) {
            errors.push_back("Method '" + method_name + "' not found in type '" + object_type->get_name() + "'");
            return nullptr;
        }
        
        auto& function_info = std::get<FunctionInfo>(method_symbol->data);
        return function_info.return_type;
    }
    
    void add_constraint(TypePtr left, TypePtr right) {
        constraints.push_back({ConstraintKind::Equals, left, right});
    }
    
    bool solve_constraints() {
        // Unification algorithm implementation
        // TODO: Implement constraint solving
        return true;
    }
    
    void apply_solution()
    {
        // TODO: Extend this when we have multiple constraints per symbol

        for (const auto& constraint : constraints) {
            // Find the symbol whose type matches this constraint's left_type
            for (auto symbol : symbolTable.get_unresolved_symbols()) {
                auto& var_info = std::get<VariableInfo>(symbol->data);
                
                if (var_info.type == constraint.left_type) {
                    // Update the symbol's type to the resolved type
                    var_info.type = constraint.right_type;
                    
                    // Only remove from unresolved list if the resolved type is concrete
                    // Check if the right_type is actually resolved (not an UnresolvedType)
                    if (!std::holds_alternative<UnresolvedType>(constraint.right_type->value)) {
                        symbolTable.set_symbol_resolved(symbol);
                    }
                    break; // Found the symbol for this constraint
                }
            }
        }
    }

    
};