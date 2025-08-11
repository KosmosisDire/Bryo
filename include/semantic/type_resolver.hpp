#pragma once

#include "symbol_table.hpp"
#include "type_system.hpp"
#include "ast/ast.hpp"
#include "symbol.hpp"
#include <set>

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
    
    // Main entry point
    bool resolve_types() {
        constraints.clear();
        errors.clear();
        
        int max_iterations = 10;
        int iteration = 0;
        
        while (iteration < max_iterations) {
            size_t initial_constraints = constraints.size();
            size_t initial_unresolved = symbolTable.get_unresolved_symbols().size();
            
            // Clear errors at the start of each iteration - only final errors matter
            errors.clear();
            
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
        
        report_final_errors();
        
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
                // TODO: Handle other kinds
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
    // Collect all return statements from a statement tree
    void collect_return_statements(StatementNode* stmt, std::vector<ExpressionNode*>& returns) {
        if (!stmt) return;
        
        if (stmt->is_a<ReturnStatementNode>()) {
            auto* ret_stmt = stmt->as<ReturnStatementNode>();
            returns.push_back(ret_stmt->expression); // Can be null for void returns
        }
        else if (stmt->is_a<BlockStatementNode>()) {
            auto* block = stmt->as<BlockStatementNode>();
            for (int i = 0; i < block->statements.size; ++i) {
                if (block->statements[i] && block->statements[i]->is_a<StatementNode>()) {
                    collect_return_statements(block->statements[i]->as<StatementNode>(), returns);
                }
            }
        }
        else if (stmt->is_a<IfStatementNode>()) {
            auto* if_stmt = stmt->as<IfStatementNode>();
            collect_return_statements(if_stmt->thenStatement, returns);
            collect_return_statements(if_stmt->elseStatement, returns);
        }
        else if (stmt->is_a<WhileStatementNode>()) {
            auto* while_stmt = stmt->as<WhileStatementNode>();
            collect_return_statements(while_stmt->body, returns);
        }
        else if (stmt->is_a<ForStatementNode>()) {
            auto* for_stmt = stmt->as<ForStatementNode>();
            collect_return_statements(for_stmt->body, returns);
        }
        else if (stmt->is_a<ForInStatementNode>()) {
            auto* for_in = stmt->as<ForInStatementNode>();
            collect_return_statements(for_in->body, returns);
        }
    }
    
    bool generate_constraints() {
        bool any_new_constraints = false;
        
        for (auto* symbol : symbolTable.get_unresolved_symbols()) {
            // Skip if we already have a constraint for this symbol
            bool already_has_constraint = false;
            for (const auto& constraint : constraints) {
                if (auto* typed_symbol = symbol->as<TypedSymbol>()) {
                    if (constraint.left_type == typed_symbol->type()) {
                        already_has_constraint = true;
                        break;
                    }
                } 
                
                if (already_has_constraint) {
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
        TypePtr symbol_type = nullptr;
        
        // Special handling for function symbols with unresolved return types
        if (auto* func_symbol = symbol->as<FunctionSymbol>()) {
            symbol_type = func_symbol->return_type();
            
            if (!symbol_type || !std::holds_alternative<UnresolvedType>(symbol_type->value)) {
                // Function doesn't need type inference
                return false;
            }
            
            auto& unresolved = std::get<UnresolvedType>(symbol_type->value);
            
            // For functions, we need to analyze the body to find return statements
            if (unresolved.body) {
                // Use the function symbol itself as the scope for analysis since it contains local variables
                TypePtr inferred_type = analyze_function_body(unresolved.body, func_symbol);
                if (inferred_type) {
                    add_constraint(symbol_type, inferred_type);
                    return true;
                } else {
                    errors.push_back("Could not infer return type for function " + symbol->name());
                    return false;
                }
            }
        }
        // Handle properties with getter bodies (similar to functions)
        else if (auto* prop_symbol = symbol->as<PropertySymbol>()) {
            symbol_type = prop_symbol->type();
            
            if (!symbol_type || !std::holds_alternative<UnresolvedType>(symbol_type->value)) {
                // Property doesn't need type inference
                return false;
            }
            
            auto& unresolved = std::get<UnresolvedType>(symbol_type->value);
            
            // If property has initializer (arrow syntax or backing field), analyze it as expression
            if (unresolved.initializer) {
                // Property expressions are analyzed in the property scope
                // The property scope can look up to its parent (type scope) to find sibling fields
                TypePtr expr_type = analyze_expression(unresolved.initializer, unresolved.defining_scope);
                if (expr_type) {
                    add_constraint(symbol_type, expr_type);
                    return true;
                } else {
                    return false;
                }
            }
            // If property has a body (getter with return statements), analyze it like a function
            else if (unresolved.body) {
                // Property getter bodies are also analyzed in the property scope
                TypePtr inferred_type = analyze_function_body(unresolved.body, unresolved.defining_scope);
                if (inferred_type) {
                    add_constraint(symbol_type, inferred_type);
                    return true;
                } else {
                    errors.push_back("Could not infer type for property " + symbol->name());
                    return false;
                }
            }
        }
        // Regular typed symbols (variables, parameters, fields)
        else if (auto* typed_symbol = symbol->as<TypedSymbol>()) {
            symbol_type = typed_symbol->type();
        } else {
            errors.push_back("Unsupported symbol type for type resolution: " + symbol->name());
            return false;
        }
        
        if (!symbol_type) {
            errors.push_back("Symbol " + symbol->name() + " has no type");
            return false;
        }
        
        if (!std::holds_alternative<UnresolvedType>(symbol_type->value)) {
            // Already resolved
            return false;
        }
        
        auto& unresolved = std::get<UnresolvedType>(symbol_type->value);
        
        if (!unresolved.can_infer()) {
            errors.push_back("Symbol " + symbol->name() + " marked as unresolved but has no initializer, type name, or defining scope");
            return false;
        }

        TypePtr expr_type = analyze_type_name(unresolved.type_name, unresolved.defining_scope);
        if (!expr_type)
        {
            expr_type = analyze_expression(unresolved.initializer, unresolved.defining_scope);
            if (!expr_type) return false;
            
            // No special range inference - all range expressions result in Range type
            // TODO: proper iterables support will handle for-in loop variable types
        }
        
        add_constraint(symbol_type, expr_type);
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
        if (!expr) return nullptr;
        
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
        else if (expr->is_a<UnaryExpressionNode>()) {
            return analyze_unary_expression(expr->as<UnaryExpressionNode>(), scope);
        }
        else if (expr->is_a<RangeExpressionNode>()) {
            return analyze_range_expression(expr->as<RangeExpressionNode>(), scope);
        }
        else if (expr->is_a<ParenthesizedExpressionNode>()) {
            return analyze_parenthesized_expression(expr->as<ParenthesizedExpressionNode>(), scope);
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
    
    TypePtr analyze_unary_expression(UnaryExpressionNode* unary, ScopeNode* scope) {
        // Analyze operand
        if (!unary->operand || !unary->operand->is_a<ExpressionNode>()) {
            errors.push_back("Invalid operand in unary expression");
            return nullptr;
        }
        TypePtr operand_type = analyze_expression(unary->operand->as<ExpressionNode>(), scope);
        if (!operand_type) {
            errors.push_back("Could not resolve operand type");
            return nullptr;
        }
        
        // Resolve through constraints if needed
        operand_type = resolve_through_constraints(operand_type);
        
        // TODO: PREMATURE - Unary operator analysis assumes built-in operator semantics
        // Should support operator overloading and proper type validation
        switch (unary->opKind) {
            case UnaryOperatorKind::Plus:
            case UnaryOperatorKind::Minus:
                // Arithmetic unary operators - return same type if numeric
                if (operand_type->get_name() == "i32" || operand_type->get_name() == "i64" ||
                    operand_type->get_name() == "f32" || operand_type->get_name() == "f64" ||
                    operand_type->get_name() == "u32" || operand_type->get_name() == "u64" ||
                    operand_type->get_name() == "i8" || operand_type->get_name() == "u8" ||
                    operand_type->get_name() == "i16" || operand_type->get_name() == "u16") {
                    return operand_type;
                }
                errors.push_back("Unary arithmetic operator requires numeric type, got: " + operand_type->get_name());
                return nullptr;
                
            case UnaryOperatorKind::Not:
                // TODO: PREMATURE - Should validate operand is boolean-convertible
                // Logical not - return bool (operand should be bool but we'll be lenient)
                return typeSystem.get_primitive("bool");
                
            case UnaryOperatorKind::BitwiseNot:
                // Bitwise not - return same type if integer
                if (operand_type->get_name() == "i32" || operand_type->get_name() == "i64" ||
                    operand_type->get_name() == "u32" || operand_type->get_name() == "u64" ||
                    operand_type->get_name() == "i8" || operand_type->get_name() == "u8" ||
                    operand_type->get_name() == "i16" || operand_type->get_name() == "u16") {
                    return operand_type;
                }
                errors.push_back("Bitwise not operator requires integer type, got: " + operand_type->get_name());
                return nullptr;
                
            case UnaryOperatorKind::PreIncrement:
            case UnaryOperatorKind::PostIncrement:
            case UnaryOperatorKind::PreDecrement:
            case UnaryOperatorKind::PostDecrement:
                // Increment/decrement operators - return same type if numeric
                if (operand_type->get_name() == "i32" || operand_type->get_name() == "i64" ||
                    operand_type->get_name() == "f32" || operand_type->get_name() == "f64" ||
                    operand_type->get_name() == "u32" || operand_type->get_name() == "u64" ||
                    operand_type->get_name() == "i8" || operand_type->get_name() == "u8" ||
                    operand_type->get_name() == "i16" || operand_type->get_name() == "u16") {
                    return operand_type;
                }
                errors.push_back("Increment/decrement operator requires numeric type, got: " + operand_type->get_name());
                return nullptr;
                
            case UnaryOperatorKind::AddressOf:
            case UnaryOperatorKind::Dereference:
                // TODO: Implement pointer types
                errors.push_back("Pointer operators not yet implemented");
                return nullptr;
                
            default:
                errors.push_back("Unsupported unary operator");
                return nullptr;
        }
    }
    
    TypePtr analyze_range_expression(RangeExpressionNode* range, ScopeNode* scope) {
        // Analyze start expression
        if (!range->start || !range->start->is_a<ExpressionNode>()) {
            errors.push_back("Range missing start expression");
            return nullptr;
        }
        TypePtr start_type = analyze_expression(range->start->as<ExpressionNode>(), scope);
        if (!start_type) {
            errors.push_back("Could not resolve range start type");
            return nullptr;
        }
        
        // Analyze end expression
        if (!range->end || !range->end->is_a<ExpressionNode>()) {
            errors.push_back("Range missing end expression");
            return nullptr;
        }
        TypePtr end_type = analyze_expression(range->end->as<ExpressionNode>(), scope);
        if (!end_type) {
            errors.push_back("Could not resolve range end type");
            return nullptr;
        }
        
        // resolve types
        start_type = resolve_through_constraints(start_type);
        end_type = resolve_through_constraints(end_type);
        
        // Verify start and end are compatible types
        if (start_type->get_name() != end_type->get_name()) {
            errors.push_back("Range start and end must be same type: " + 
                           start_type->get_name() + " != " + end_type->get_name());
            return nullptr;
        }
        
        // Analyze step expression if present
        if (range->stepExpression) {
            TypePtr step_type = analyze_expression(range->stepExpression->as<ExpressionNode>(), scope);
            if (!step_type) {
                errors.push_back("Could not resolve range step type");
                return nullptr;
            }
            step_type = resolve_through_constraints(step_type);
            
            // Verify step type is compatible with range element type
            if (start_type->get_name() != step_type->get_name()) {
                errors.push_back("Range step must be same type as range elements: " + 
                               step_type->get_name() + " != " + start_type->get_name());
                return nullptr;
            }
        }
        
        // All ranges return the Range type for now
        return typeSystem.get_primitive("Range");
    }
    
    TypePtr analyze_parenthesized_expression(ParenthesizedExpressionNode* paren, ScopeNode* scope) {
        // Parenthesized expressions simply pass through the type of their inner expression
        if (!paren->expression) {
            errors.push_back("Empty parenthesized expression");
            return nullptr;
        }
        
        return analyze_expression(paren->expression, scope);
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
        
        // TODO: assumes built-in operator semantics
        // Should be replaced with proper operator overloading system and method resolution
        switch (binary->opKind) {
            // Arithmetic operators - with basic type coercion
            case BinaryOperatorKind::Add:
            case BinaryOperatorKind::Subtract:
            case BinaryOperatorKind::Multiply:
            case BinaryOperatorKind::Divide:
            case BinaryOperatorKind::Modulo:
                // Same type - return that type
                if (left_type->get_name() == right_type->get_name()) {
                    return left_type;
                }
                
                // TODO: Hardcoded type coercion rules

                // Type coercion rules: promote integers to floats
                // f32 + i32 -> f32
                if (left_type->get_name() == "f32" && right_type->get_name() == "i32") {
                    return left_type; // f32
                }
                // i32 + f32 -> f32  
                if (left_type->get_name() == "i32" && right_type->get_name() == "f32") {
                    return right_type; // f32
                }
                // f64 + i32 -> f64
                if (left_type->get_name() == "f64" && right_type->get_name() == "i32") {
                    return left_type; // f64
                }
                // i32 + f64 -> f64
                if (left_type->get_name() == "i32" && right_type->get_name() == "f64") {
                    return right_type; // f64
                }
                
                errors.push_back("Type mismatch in binary expression: " + left_type->get_name() + " and " + right_type->get_name());
                return nullptr;
                
            // TODO: Comparison operators assume all types are comparable
            // Currently allows nonsensical comparisons like "string" < "Rectangle" 
            case BinaryOperatorKind::Equals:
            case BinaryOperatorKind::NotEquals:
            case BinaryOperatorKind::LessThan:
            case BinaryOperatorKind::LessThanOrEqual:
            case BinaryOperatorKind::GreaterThan:
            case BinaryOperatorKind::GreaterThanOrEqual:
                return typeSystem.get_primitive("bool");

            // TODO: Logical operators don't validate operand types
            // Should verify operands are boolean or boolean-convertible
            case BinaryOperatorKind::LogicalAnd:
            case BinaryOperatorKind::LogicalOr:
                return typeSystem.get_primitive("bool");
                
            default:
                errors.push_back("Unsupported binary operator");
                return nullptr;
        }
    }
    
    TypePtr resolve_through_constraints(TypePtr type) {
        // Follow constraint chains to find the most resolved type
        TypePtr current = type;
        std::set<TypePtr> visited; // Prevent infinite loops
        
        while (visited.find(current) == visited.end()) {
            visited.insert(current);
            
            // Look for a constraint where current is the left side
            bool found_constraint = false;
            for (const auto& constraint : constraints) {
                if (constraint.left_type == current && constraint.kind == ConstraintKind::Equals) {
                    current = constraint.right_type;
                    found_constraint = true;
                    break;
                }
            }
            
            if (!found_constraint) {
                break; // No more constraints to follow
            }
        }
        
        return current;
    }
    
    TypePtr analyze_identifier(IdentifierExpressionNode* ident, ScopeNode* scope) {
        std::string var_name(ident->identifier->name);
        
        // Look up the identifier in the current scope
        Symbol* symbol = nullptr;
        if (auto* scope_container = scope->as_scope()) {
            symbol = scope_container->lookup(var_name);
        }
        
        if (!symbol) {
            errors.push_back("Undefined identifier: " + var_name);
            return nullptr;
        }
        
        // For typed symbols, return their type
        if (auto* typed_symbol = symbol->as<TypedSymbol>()) {
            return typed_symbol->type();
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
        
        // Try to resolve through constraints in case it's a forward reference
        target_type = resolve_through_constraints(target_type);
        
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
        auto* type_symbol = object_type->get_type_symbol();
        if (!type_symbol) {
            errors.push_back("Could not find type symbol for: " + object_type->get_name());
            return nullptr;
        }
        
        // Look up the member in the type's scope (if it has one)
        Symbol* member_symbol = nullptr;
        if (auto* type_scope = type_symbol->as_scope()) {
            member_symbol = type_scope->lookup_local(member_name);
        }
        if (!member_symbol) {
            errors.push_back("Member '" + member_name + "' not found in type '" + object_type->get_name() + "'");
            return nullptr;
        }
        
        // Return the member's type based on symbol type
        if (auto* typed_symbol = member_symbol->as<TypedSymbol>()) {
            return typed_symbol->type();
        }
        
        errors.push_back("Member '" + member_name + "' has unsupported type");
        return nullptr;
    }
    
    void add_constraint(TypePtr left, TypePtr right) {
        constraints.push_back({ConstraintKind::Equals, left, right});
    }
    
    bool solve_constraints() {
        // Basic unification algorithm - iteratively apply substitutions
        bool changed = true;
        int iterations = 0;
        const int max_iterations = 100; // Prevent infinite loops
        
        while (changed && iterations < max_iterations) {
            changed = false;
            iterations++;
            
            // For each constraint, try to propagate type information
            for (auto& constraint : constraints) {
                if (constraint.kind != ConstraintKind::Equals) continue;
                
                TypePtr left = constraint.left_type;
                TypePtr right = constraint.right_type;
                
                // Resolve through existing constraints (follow chains)
                left = resolve_through_constraints(left);
                right = resolve_through_constraints(right);
                
                // Skip if both sides are the same
                if (left == right) continue;
                
                // If one side is unresolved and the other is concrete, we can make progress
                bool left_unresolved = std::holds_alternative<UnresolvedType>(left->value);
                bool right_unresolved = std::holds_alternative<UnresolvedType>(right->value);
                
                if (left_unresolved && !right_unresolved) {
                    // Left is unresolved, right is concrete - update constraint
                    constraint.left_type = right;
                    changed = true;
                } else if (!left_unresolved && right_unresolved) {
                    // Right is unresolved, left is concrete - update constraint
                    constraint.right_type = left;
                    changed = true;
                } else if (!left_unresolved && !right_unresolved) {
                    // Both concrete - check compatibility
                    if (left->get_name() != right->get_name()) {
                        errors.push_back("Type mismatch in constraints: " + 
                                       left->get_name() + " != " + right->get_name());
                        return false;
                    }
                }
                // Both unresolved - can't make progress on this constraint yet
            }
        }
        
        if (iterations >= max_iterations) {
            errors.push_back("Constraint solving did not converge after " + 
                           std::to_string(max_iterations) + " iterations");
            return false;
        }
        
        return true;
    }
    
    TypePtr analyze_function_body(BlockStatementNode* body, ScopeNode* scope) {
        // Collect all return statements from the function body
        std::vector<ExpressionNode*> return_expressions;
        collect_return_statements(body, return_expressions);
        
        if (return_expressions.empty()) {
            // No return statements found - function returns void
            return typeSystem.get_primitive("void");
        }
        
        // Analyze all return expressions and find a common type
        TypePtr common_type = nullptr;
        
        for (auto* expr : return_expressions) {
            if (!expr) {
                // Empty return statement - function returns void
                TypePtr void_type = typeSystem.get_primitive("void");
                if (!common_type) {
                    common_type = void_type;
                } else if (common_type->get_name() != "void") {
                    errors.push_back("Mixed void and non-void return statements in function");
                    return nullptr;
                }
                continue;
            }
            
            TypePtr expr_type = analyze_expression(expr, scope);
            if (!expr_type) {
                errors.push_back("Could not analyze return expression type");
                return nullptr;
            }
            
            // Resolve through constraints if needed
            expr_type = resolve_through_constraints(expr_type);
            
            if (!common_type) {
                common_type = expr_type;
            } else if (common_type->get_name() != expr_type->get_name()) {
                // Handle case where one type is unresolved and the other is concrete
                bool common_unresolved = std::holds_alternative<UnresolvedType>(common_type->value);
                bool expr_unresolved = std::holds_alternative<UnresolvedType>(expr_type->value);
                
                if (common_unresolved && !expr_unresolved) {
                    // common_type is unresolved, expr_type is concrete - use concrete type
                    common_type = expr_type;
                } else if (!common_unresolved && expr_unresolved) {
                    // common_type is concrete, expr_type is unresolved - keep concrete type
                    // common_type stays the same
                } else if (!common_unresolved && !expr_unresolved) {
                    // Both are concrete but different - this is a real error that should not be ignored
                    errors.push_back("Function has inconsistent return types: '" + common_type->get_name() + 
                                   "' and '" + expr_type->get_name() + "'");
                    return nullptr;
                }
                // If both are unresolved, we can't determine compatibility yet - defer the check
                // by keeping the first unresolved type and hoping future iterations resolve both
            }
        }
        
        return common_type ? common_type : typeSystem.get_primitive("void");
    }
    
    void apply_solution()
    {
        // TODO: Extend this when we have multiple constraints per symbol

        for (const auto& constraint : constraints) {
            // Find the symbol whose type matches this constraint's left_type
            for (auto* symbol : symbolTable.get_unresolved_symbols()) {
                bool type_matches = false;
                
                // Special handling for FunctionSymbol
                if (auto* func_symbol = symbol->as<FunctionSymbol>()) {
                    if (func_symbol->return_type() == constraint.left_type) {
                        func_symbol->set_return_type(constraint.right_type);
                        type_matches = true;
                    }
                }
                // Special handling for PropertySymbol
                else if (auto* prop_symbol = symbol->as<PropertySymbol>()) {
                    if (prop_symbol->type() == constraint.left_type) {
                        prop_symbol->set_type(constraint.right_type);
                        type_matches = true;
                    }
                }
                // Regular typed symbols
                else if (auto* typed_symbol = symbol->as<TypedSymbol>()) {
                    if (typed_symbol->type() == constraint.left_type) {
                        typed_symbol->set_type(constraint.right_type);
                        type_matches = true;
                    }
                }
                
                if (type_matches) {
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
    
    void report_final_errors() {
        // Only report errors for symbols that remain unresolved after all iterations
        for (auto* symbol : symbolTable.get_unresolved_symbols()) {
            if (auto* func_symbol = symbol->as<FunctionSymbol>()) {
                errors.push_back("Could not infer return type for function '" + symbol->name() + 
                               "' - check for type incompatibilities or missing type information");
            } else if (auto* prop_symbol = symbol->as<PropertySymbol>()) {
                errors.push_back("Could not infer type for property '" + symbol->name() + 
                               "' - check getter implementation");
            } else if (auto* typed_symbol = symbol->as<TypedSymbol>()) {
                errors.push_back("Could not infer type for " + std::string(symbol->kind_name()) + " '" + symbol->name() + "'");
            }
        }
    }

    
};