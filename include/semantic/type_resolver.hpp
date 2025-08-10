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
        
        for (auto* symbol : symbolTable.get_unresolved_symbols()) {
            // Skip if we already have a constraint for this symbol
            bool already_has_constraint = false;
            for (const auto& constraint : constraints) {
                if (constraint.left_type == symbol->type) {
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
    
    bool generate_constraints_for_symbol(Symbol* symbol) {
        auto& unresolved = std::get<UnresolvedType>(symbol->type->value);
        
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
        
        add_constraint(symbol->type, expr_type);
        return true;
    }

    TypePtr analyze_type_name(TypeNameNode* type_name, ScopeNode* scope) {
        if (!type_name || type_name->name->identifiers.size == 0) return nullptr;
        
        // Resolve type name starting from the given scope
        std::string type_name_str(type_name->get_full_name());
        TypePtr resolved_type = symbolTable.resolve_type_name(type_name_str, scope);
        
        if (!resolved_type) {
            errors.push_back("Unresolved type: " + type_name_str);
            return nullptr;
        }
        
        return resolved_type;
    }
    
    TypePtr analyze_expression(ExpressionNode* expr, ScopeNode* scope) {
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
    
    TypePtr analyze_new_expression(NewExpressionNode* new_expr, ScopeNode* scope)
    {
        // TODO: Handle constructor call validation later
        return symbolTable.resolve_type_name(new_expr->type->get_full_name(), scope);
    }
    
    TypePtr analyze_binary_expression(BinaryExpressionNode* binary, ScopeNode* scope) {
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
    
    TypePtr analyze_identifier(IdentifierExpressionNode* ident, ScopeNode* scope) {
        std::string var_name(ident->identifier->name);
        
        // Look up the identifier in the current scope
        Symbol* symbol = scope->lookup(var_name);
        if (!symbol) {
            errors.push_back("Undefined identifier: " + var_name);
            return nullptr;
        }
        
        // For variables, parameters, and fields, return their type
        if (symbol->is_variable() || symbol->is_parameter() || symbol->is_field()) {
            return symbol->type;
        } 
        // For functions, return their return type
        else if (symbol->is_function()) {
            return symbol->type; // type field holds return type for functions
        }

        errors.push_back("Identifier '" + var_name + "' is not a variable or function");
        return nullptr;
    }
    
    TypePtr analyze_call_expression(CallExpressionNode* call, ScopeNode* scope) {
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

    TypePtr analyze_member_access(MemberAccessExpressionNode* member, ScopeNode* scope) {
        // First, resolve the object being accessed
        TypePtr object_type = analyze_expression(member->target, scope);
        if (!object_type) {
            errors.push_back("Could not resolve member access target");
            return nullptr;
        }
        
        // Try to resolve through constraints if it's unresolved
        object_type = resolve_through_constraints(object_type);
        
        // Get the member name
        std::string member_name(member->member->name);
        
        // Get the type symbol from the type
        Symbol* type_symbol = object_type->get_type_symbol();
        if (!type_symbol) {
            errors.push_back("Could not find type symbol for: " + object_type->get_name());
            return nullptr;
        }
        
        // Look up the member in the type's children
        Symbol* member_symbol = type_symbol->lookup_local(member_name);
        if (!member_symbol) {
            errors.push_back("Member '" + member_name + "' not found in type '" + object_type->get_name() + "'");
            return nullptr;
        }
        
        // Return the member's type
        return member_symbol->type;
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
            for (auto* symbol : symbolTable.get_unresolved_symbols()) {
                if (symbol->type == constraint.left_type) {
                    // Update the symbol's type to the resolved type
                    symbol->type = constraint.right_type;
                    
                    // Only remove from unresolved list if the resolved type is concrete
                    // Check if the right_type is actually resolved (not an UnresolvedType)
                    if (!std::holds_alternative<UnresolvedType>(constraint.right_type->value)) {
                        symbolTable.mark_symbol_resolved(symbol);
                    }
                    break; // Found the symbol for this constraint
                }
            }
        }
    }

    
};