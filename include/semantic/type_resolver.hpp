#pragma once

#include "symbol_table.hpp"
#include "type_system.hpp"
#include "ast/ast.hpp"
#include "symbol.hpp"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include "type.hpp"

namespace Myre
{

    /**
     * @class TypeResolver
     * @brief Performs full semantic analysis, including type checking, type inference,
     * symbol resolution, and AST annotation.
     *
     * This class is the core of the "middle-end". It validates the semantic
     * correctness of the program and enriches the AST with the information
     * needed for subsequent stages like code generation.
     */
    class TypeResolver : public DefaultVisitor
    {

    private:
        SymbolTable &symbolTable;
        TypeSystem &typeSystem;
        std::vector<std::string> errors;

        // --- Core of the Unification Solver ---
        std::unordered_map<TypePtr, TypePtr> substitution;
        std::unordered_set<TypePtr> pendingConstraints;

        // --- Intermediate State ---
        // Maps each AST node to its resolved semantic type during analysis.
        std::unordered_map<Node *, TypePtr> nodeTypes;

    public:
        TypeResolver(SymbolTable &symbol_table)
            : symbolTable(symbol_table), typeSystem(symbol_table.get_type_system()) {}

        const std::vector<std::string> &get_errors() const { return errors; }

        /**
         * @brief Main entry point for the semantic analysis phase.
         * @param unit The root of the AST.
         * @return True if resolution was successful with no errors, false otherwise.
         */
        bool resolve(CompilationUnit *unit)
        {
            errors.clear();
            nodeTypes.clear();
            substitution.clear();
            pendingConstraints.clear();

            // Phase 1: Repeatedly traverse the AST to gather constraints and solve for
            // inferred types until no more progress can be made.
            int pass = 0;
            const int max_passes = 10; // Failsafe for complex inference cycles
            while (pass < max_passes)
            {
                pass++;
                size_t errors_before_pass = errors.size();
                size_t constraints_before_pass = pendingConstraints.size();
                

                unit->accept(this); // Run a full traversal

                // Check if we made progress: either resolved constraints or found new errors
                bool made_progress = (errors.size() > errors_before_pass) ||
                                     (pendingConstraints.size() < constraints_before_pass);

                // if (!made_progress || pendingConstraints.empty())
                // {
                //     break;
                // }
            }

            // Phase 2: Report any constraints that are still unresolved.
            report_final_errors();

            // Phase 3: Final pass to update all AST nodes with canonical types
            update_ast_with_final_types(unit);

            return errors.empty();
        }

    private:
        // --- Core Unification Logic ---

        TypePtr apply_substitution(TypePtr type);
        void unify(TypePtr t1, TypePtr t2, Node *error_node, const std::string &context);
        bool has_pending_constraints();

        void report_final_errors();
        void update_ast_with_final_types(CompilationUnit *unit);

        // --- Visitor Overrides for Constraint Gathering ---

        // Expressions are the primary source of type information
        void visit(LiteralExpr *node) override;
        void visit(ArrayLiteralExpr *node) override;
        void visit(NameExpr *node) override;
        void visit(BinaryExpr *node) override;
        void visit(AssignmentExpr *node) override;
        void visit(CallExpr *node) override;
        void visit(MemberAccessExpr *node) override;
        void visit(UnaryExpr *node) override;
        void visit(IndexerExpr *node) override;
        void visit(ConditionalExpr *node) override;
        void visit(IfExpr *node) override;
        void visit(CastExpr *node) override;
        void visit(ThisExpr *node) override;
        void visit(NewExpr *node) override;

        // Statements need type checking for their expressions
        void visit(ReturnStmt *node) override;
        void visit(ForStmt *node) override;
        void visit(WhileStmt *node) override;
        // Block and ExpressionStmt visitors removed - default behavior sufficient

        // Declarations can introduce new types that need inference
        void visit(VariableDecl *node) override;
        void visit(PropertyDecl *node) override;
        void visit(PropertyAccessor *node) override;
        void visit(FunctionDecl *node) override;
        void visit(ParameterDecl *node) override;
        void visit(TypedIdentifier *node) override;

        // --- Helper Methods ---
        TypePtr get_node_type(Node *node);
        void set_node_type(Node *node, TypePtr type);
        void annotate_expression(Expression *expr, TypePtr type);
        void annotate_name_expr(NameExpr *expr, Symbol *symbol);
        TypePtr resolve_ast_type_expr(Expression *type_expr, Scope *scope);
        TypePtr infer_function_return_type(Block *body);
    };

    // ============================================================================
    // --- TypeResolver Implementation ---
    // ============================================================================

    TypePtr TypeResolver::apply_substitution(TypePtr type)
    {
        if (!type)
            return nullptr;
        auto it = substitution.find(type);
        if (it == substitution.end())
        {
            return type; // It's a canonical type (concrete or unresolved root)
        }
        // Path compression for efficiency
        TypePtr root = apply_substitution(it->second);
        substitution[type] = root;
        return root;
    }

    bool TypeResolver::has_pending_constraints()
    {
        return !pendingConstraints.empty();
    }

    void TypeResolver::unify(TypePtr t1, TypePtr t2, Node *error_node, const std::string &context)
    {
        if (!t1 || !t2)
            return;
        TypePtr root1 = apply_substitution(t1);
        TypePtr root2 = apply_substitution(t2);

        if (root1 == root2)
            return; // Already unified

        bool root1_is_var = root1->is<UnresolvedType>();
        bool root2_is_var = root2->is<UnresolvedType>();

        if (root1_is_var)
        {
            substitution[root1] = root2;
        }
        else if (root2_is_var)
        {
            substitution[root2] = root1;
        }
        else if (root1->get_name() != root2->get_name())
        {
            errors.push_back("Type mismatch in " + context + ": cannot unify '" + root1->get_name() + "' with '" + root2->get_name() + "'.");
        }
    }

    void TypeResolver::report_final_errors()
    {
        for (auto &[node, type] : nodeTypes)
        {
            TypePtr canonical = apply_substitution(type);
            if (canonical->is<UnresolvedType>())
            {
                errors.push_back("Could not infer type for expression at line " + node->location.start.to_string());
            }
        }
    }

    void TypeResolver::update_ast_with_final_types(CompilationUnit *unit)
    {
        class TypeUpdateVisitor : public DefaultVisitor
        {
            TypeResolver* resolver;
        public:
            TypeUpdateVisitor(TypeResolver* res) : resolver(res) {}
            
            void visit(Expression *node) override
            {
                if (node && node->resolvedType)
                {
                    // Update with the final canonical type
                    node->resolvedType = resolver->apply_substitution(node->resolvedType);
                }
                // Continue visiting children
                DefaultVisitor::visit(node);
            }
        };

        TypeUpdateVisitor updater(this);
        unit->accept(&updater);
    }

    TypePtr TypeResolver::get_node_type(Node *node)
    {
        auto it = nodeTypes.find(node);
        return (it != nodeTypes.end()) ? it->second : nullptr;
    }

    void TypeResolver::set_node_type(Node *node, TypePtr type)
    {
        if (node && type)
        {
            nodeTypes[node] = type;
        }
    }

    void TypeResolver::annotate_expression(Expression *expr, TypePtr type)
    {
        if (expr && type)
        {
            set_node_type(expr, type);
            // Apply substitution and annotate the AST node directly
            TypePtr canonical = apply_substitution(type);
            expr->resolvedType = canonical;

            // Track pending constraints
            if (canonical->is<UnresolvedType>())
            {
                pendingConstraints.insert(canonical);
            }
            else
            {
                pendingConstraints.erase(canonical);
            }
        }
    }

    void TypeResolver::annotate_name_expr(NameExpr *expr, Symbol *symbol)
    {
        if (expr && symbol)
        {
            expr->resolvedSymbol = symbol->handle;
        }
    }

    TypePtr TypeResolver::resolve_ast_type_expr(Expression *type_expr, Scope *scope)
    {
        if (!type_expr || !scope)
            return typeSystem.get_unresolved_type();

        // Single identifier (like "i32", "String")
        if (auto *name = type_expr->as<NameExpr>())
        {
            // Check for primitive types first
            TypePtr primType = typeSystem.get_primitive(name->get_name());
            if (primType) {
                return primType;
            }
            // Otherwise look up in symbol table
            return symbolTable.resolve_type_name(name->get_name(), scope->as_scope_node());
        }
        // Qualified name (like "System.String")
        else if (auto *member = type_expr->as<MemberAccessExpr>())
        {
            // Build qualified name from MemberAccessExpr chain inline to avoid linking issues
            std::string qualifiedName;
            auto *currentMember = member;
            std::vector<std::string> parts;

            // Collect all parts
            while (currentMember)
            {
                if (currentMember->member)
                {
                    parts.insert(parts.begin(), std::string(currentMember->member->text));
                }
                if (auto *nestedMember = currentMember->object->as<MemberAccessExpr>())
                {
                    currentMember = nestedMember;
                }
                else if (auto *name = currentMember->object->as<NameExpr>())
                {
                    parts.insert(parts.begin(), name->get_name());
                    break;
                }
                else
                {
                    break;
                }
            }

            // Build qualified name
            for (size_t i = 0; i < parts.size(); ++i)
            {
                if (i > 0)
                    qualifiedName += ".";
                qualifiedName += parts[i];
            }

            return symbolTable.resolve_type_name(qualifiedName, scope->as_scope_node());
        }
        // Array type (like "i32[]")
        else if (auto *array = type_expr->as<ArrayTypeExpr>())
        {
            TypePtr elemType = resolve_ast_type_expr(array->elementType, scope);
            int arrSize = -1;
            if (array->size)
            {
                arrSize = std::stoi(std::string(array->size->value));
            }
            return typeSystem.get_array_type(elemType, arrSize);
        }
        // Function type (like "fn(i32, i32) -> i32")
        else if (auto *func = type_expr->as<FunctionTypeExpr>())
        {
            std::vector<TypePtr> paramTypes;
            for (auto *paramExpr : func->parameterTypes)
            {
                paramTypes.push_back(resolve_ast_type_expr(paramExpr, scope));
            }
            TypePtr returnType = func->returnType ? resolve_ast_type_expr(func->returnType, scope) : typeSystem.get_primitive("void");
            return typeSystem.get_function_type(paramTypes, returnType);
        }

        return typeSystem.get_unresolved_type();
    }

    TypePtr TypeResolver::infer_function_return_type(Block *body)
    {
        if (!body)
            return typeSystem.get_primitive("void");

        // Simple return type inference: look for return statements and unify their types
        TypePtr inferredType = nullptr;
        
        // Helper class to find return statements
        class ReturnTypeFinder : public DefaultVisitor
        {
        public:
            TypePtr commonReturnType = nullptr;
            TypeResolver* resolver;
            bool hasVoidReturn = false;
            
            ReturnTypeFinder(TypeResolver* res) : resolver(res) {}
            
            void visit(ReturnStmt *node) override
            {
                if (node->value)
                {
                    // Return with value
                    TypePtr valueType = resolver->get_node_type(node->value);
                    if (valueType)
                    {
                        if (!commonReturnType)
                        {
                            commonReturnType = valueType;
                        }
                        else
                        {
                            // Try to unify with existing return type
                            resolver->unify(commonReturnType, valueType, node, "return type inference");
                            commonReturnType = resolver->apply_substitution(commonReturnType);
                        }
                    }
                }
                else
                {
                    // Return without value
                    hasVoidReturn = true;
                }
                
            }
            
            void visit(FunctionDecl *node) override
            {
                // Don't visit nested functions - their return statements don't affect outer function
                // Skip visiting this node entirely
            }
        };
        
        ReturnTypeFinder finder(this);
        body->accept(&finder);
        
        // Determine final return type
        if (finder.commonReturnType && !finder.hasVoidReturn)
        {
            // All returns have values and consistent type
            return finder.commonReturnType;
        }
        else if (finder.hasVoidReturn && !finder.commonReturnType)
        {
            // All returns are void
            return typeSystem.get_primitive("void");
        }
        else if (finder.commonReturnType && finder.hasVoidReturn)
        {
            // Mixed return types - this is an error, but for now default to the value type
            // A more sophisticated implementation would report an error
            return finder.commonReturnType;
        }
        else
        {
            // No return statements found - default to void
            return typeSystem.get_primitive("void");
        }
    }

    // --- Visitor Implementations ---

    void TypeResolver::visit(LiteralExpr *node)
    {
        std::string type_name = std::string(to_string(node->kind));
        annotate_expression(node, typeSystem.get_primitive(type_name));
    }

    void TypeResolver::visit(ArrayLiteralExpr *node)
    {
        // Visit all elements first
        for (auto *elem : node->elements)
        {
            if (elem)
                elem->accept(this);
        }

        // Handle empty array - type remains unresolved until context provides type
        if (node->elements.empty())
        {
            annotate_expression(node, typeSystem.get_unresolved_type());
            return;
        }

        // Determine element type by unifying all elements
        TypePtr elementType = nullptr;
        for (auto *elem : node->elements)
        {
            if (!elem)
                continue;
                
            TypePtr elemType = get_node_type(elem);
            if (!elemType)
            {
                // Error already reported by element
                annotate_expression(node, typeSystem.get_unresolved_type());
                return;
            }

            if (!elementType)
            {
                elementType = elemType;
            }
            else
            {
                unify(elementType, elemType, node, "array element types");
                elementType = apply_substitution(elementType);
            }
        }

        // Create array type with the unified element type
        if (elementType)
        {
            TypePtr arrayType = typeSystem.get_array_type(elementType, node->elements.size());
            annotate_expression(node, arrayType);
        }
        else
        {
            annotate_expression(node, typeSystem.get_unresolved_type());
        }
    }

    void TypeResolver::visit(NameExpr *node)
    {
        // Visit children manually (just the name)
        if (node->name)
            node->name->accept(this);
        
        // Skip type expressions - they are processed via resolve_ast_type_expr() when needed
        if (node->isTypeExpression)
        {
            return;
        }
        
        auto *scope = symbolTable.lookup_handle(node->containingScope)->as<Scope>();
        if (!scope)
        {
            errors.push_back("Internal error: Name expression has no containing scope.");
            annotate_expression(node, typeSystem.get_unresolved_type());
            return;
        }
        
        auto *symbol = scope->lookup(node->get_name());
        if (!symbol)
        {
            errors.push_back("Identifier not found: '" + node->get_name() + "'.");
            annotate_expression(node, typeSystem.get_unresolved_type());
            return;
        }

        if (auto *typed_symbol = symbol->as<TypedSymbol>())
        {
            annotate_expression(node, typed_symbol->type());
            annotate_name_expr(node, symbol);
        }
        else
        {
            errors.push_back("Identifier '" + node->get_name() + "' is not a value.");
            annotate_expression(node, typeSystem.get_unresolved_type());
        }
    }

    void TypeResolver::visit(BinaryExpr *node)
    {
        // Visit children manually 
        node->left->accept(this);
        node->right->accept(this);
        TypePtr leftType = get_node_type(node->left);
        TypePtr rightType = get_node_type(node->right);

        if (!leftType || !rightType)
        {
            annotate_expression(node, typeSystem.get_unresolved_type());
            return; // Errors already reported by children
        }

        // For now, assume types must match. A real implementation would have promotion rules.
        unify(leftType, rightType, node, "binary expression");

        // Determine result type
        TypePtr resultType = apply_substitution(leftType);
        switch (node->op)
        {
        case BinaryOperatorKind::Equals:
        case BinaryOperatorKind::NotEquals:
        case BinaryOperatorKind::LessThan:
        case BinaryOperatorKind::LessThanOrEqual:
        case BinaryOperatorKind::GreaterThan:
        case BinaryOperatorKind::GreaterThanOrEqual:
            annotate_expression(node, typeSystem.get_primitive("bool"));
            break;
        default: // Arithmetic
            annotate_expression(node, resultType);
            break;
        }
    }

    void TypeResolver::visit(AssignmentExpr *node)
    {
        // Visit children manually
        node->target->accept(this);
        node->value->accept(this);
        TypePtr targetType = get_node_type(node->target);
        TypePtr valueType = get_node_type(node->value);

        if (!targetType || !valueType)
        {
            annotate_expression(node, typeSystem.get_unresolved_type());
            return;
        }

        unify(targetType, valueType, node, "assignment");
        annotate_expression(node, apply_substitution(targetType));
    }

    void TypeResolver::visit(CallExpr *node)
    {
        // Visit children manually
        node->callee->accept(this);
        for (auto *arg : node->arguments)
        {
            if (arg)
                arg->accept(this);
        }

        // Handle simple name function calls (e.g., foo())
        if (auto *name = node->callee->as<NameExpr>())
        {
            auto *scope = symbolTable.lookup_handle(name->containingScope)->as<Scope>();
            if (scope)
            {
                auto *symbol = scope->lookup(name->get_name());
                if (auto *func = symbol->as<FunctionSymbol>())
                {
                    annotate_expression(node, func->return_type());
                    node->resolvedCallee = symbol->handle;
                    return;
                }
            }
        }

        // Handle member access function calls (e.g., obj.method())
        else if (auto *memberAccess = node->callee->as<MemberAccessExpr>())
        {
            TypePtr objectType = get_node_type(memberAccess->object);
            if (objectType)
            {
                TypePtr canonicalType = apply_substitution(objectType);

                // Check if the object type has the called method
                if (auto *typeRef = std::get_if<TypeReference>(&canonicalType->value))
                {
                    TypeLikeSymbol *typeSymbol = typeRef->definition;
                    if (auto *scope = typeSymbol->as<Scope>())
                    {
                        std::string methodName = std::string(memberAccess->member->text);
                        auto *member = scope->lookup_local(methodName);
                        if (auto *func = member ? member->as<FunctionSymbol>() : nullptr)
                        {
                            annotate_expression(node, func->return_type());
                            node->resolvedCallee = member->handle;
                            memberAccess->resolvedMember = member->handle;
                            return;
                        }
                    }
                }
            }
        }

        // If we get here, the expression is not callable
        errors.push_back("Expression is not callable.");
        annotate_expression(node, typeSystem.get_unresolved_type());
    }

    void TypeResolver::visit(NewExpr *node)
    {
        // Don't visit the type expression as a value - it's resolved as a type
        // Only visit the constructor arguments
        for (auto *arg : node->arguments)
        {
            if (arg)
                arg->accept(this);
        }

        auto *scope = symbolTable.lookup_handle(node->containingScope)->as<Scope>();
        if (!scope)
        {
            errors.push_back("Internal error: NewExpr has no containing scope.");
            annotate_expression(node, typeSystem.get_unresolved_type());
            return;
        }

        annotate_expression(node, resolve_ast_type_expr(node->type, scope));
    }

    void TypeResolver::visit(VariableDecl *node)
    {
        // First, resolve the type if it's still unresolved
        auto *scope = symbolTable.lookup_handle(node->containingScope)->as<Scope>();
        if (!scope)
        {
            errors.push_back("Internal error: Variable declaration has no containing scope.");
            return;
        }

        auto *symbol = scope->lookup(node->variable->name->text);
        auto *var_symbol = symbol ? symbol->as<TypedSymbol>() : nullptr;

        if (var_symbol)
        {
            TypePtr varType = var_symbol->type();

            // Resolve explicit type annotations that were deferred during symbol building
            if (varType && varType->is<UnresolvedType>())
            {
                auto& unresolved = varType->as<UnresolvedType>();
                if (unresolved.typeName)
                {
                    // Resolve the type expression now that all types are in symbol table
                    TypePtr resolvedType = resolve_ast_type_expr(unresolved.typeName, scope);
                    var_symbol->set_type(resolvedType);
                    varType = resolvedType;
                    symbolTable.mark_symbol_resolved(symbol);
                }
            }

            // Handle initializer (existing code)
            if (node->initializer)
            {
                node->initializer->accept(this);
                TypePtr initType = get_node_type(node->initializer);
                if (initType)
                {
                    unify(varType, initType, node, "variable initialization");

                    // If unification resolved the variable's type, update the symbol
                    TypePtr finalType = apply_substitution(varType);
                    if (!finalType->is<UnresolvedType>())
                    {
                        var_symbol->set_type(finalType);
                        symbolTable.mark_symbol_resolved(symbol);
                    }
                }
            }
        }
    }

    void TypeResolver::visit(MemberAccessExpr *node)
    {
        // Visit children manually
        if (node->object)
            node->object->accept(this);
        if (node->member)
            node->member->accept(this);
            
        TypePtr objectType = get_node_type(node->object);
        if (!objectType)
        {
            annotate_expression(node, typeSystem.get_unresolved_type());
            return; // Error already reported by child
        }

        // Apply substitution to get canonical type
        TypePtr canonicalType = apply_substitution(objectType);

        // Check if the type is a type reference that has members
        if (auto *typeRef = std::get_if<TypeReference>(&canonicalType->value))
        {
            TypeLikeSymbol *typeSymbol = typeRef->definition;
            if (auto *scope = typeSymbol->as<Scope>())
            {
                // Look up the member in the type's scope
                std::string memberName = std::string(node->member->text);
                auto *member = scope->lookup_local(memberName);
                if (member)
                {
                    if (auto *typed_member = member->as<TypedSymbol>())
                    {
                        annotate_expression(node, typed_member->type());
                        node->resolvedMember = member->handle;
                    }
                    else
                    {
                        errors.push_back("Member '" + memberName + "' is not a value.");
                    }
                }
                else
                {
                    errors.push_back("Type '" + typeSymbol->name() + "' has no member '" + memberName + "'.");
                }
            }
            else
            {
                errors.push_back("Type '" + typeSymbol->name() + "' is not a scoped type.");
            }
        }
        else
        {
            errors.push_back("Cannot access members of non-type expression (type: " + canonicalType->get_name() + ").");
        }
    }

    void TypeResolver::visit(UnaryExpr *node)
    {
        // Visit children manually
        if (node->operand)
            node->operand->accept(this);
            
        TypePtr operandType = get_node_type(node->operand);
        if (!operandType)
        {
            annotate_expression(node, typeSystem.get_unresolved_type());
            return; // Error already reported by child
        }

        TypePtr canonicalType = apply_substitution(operandType);

        switch (node->op)
        {
        case UnaryOperatorKind::Plus:
        case UnaryOperatorKind::Minus:
            // Arithmetic unary operators: operand and result have same type
            // Should be numeric types (i32, f32, etc.)
            annotate_expression(node, canonicalType);
            break;

        case UnaryOperatorKind::Not:
            // Logical not: operand should be bool, result is bool
            unify(canonicalType, typeSystem.get_primitive("bool"), node, "logical not operand");
            annotate_expression(node, typeSystem.get_primitive("bool"));
            break;

        case UnaryOperatorKind::BitwiseNot:
            // Bitwise not: operand and result have same type (should be integer)
            annotate_expression(node, canonicalType);
            break;

        case UnaryOperatorKind::PreIncrement:
        case UnaryOperatorKind::PreDecrement:
        case UnaryOperatorKind::PostIncrement:
        case UnaryOperatorKind::PostDecrement:
            // Increment/decrement: operand and result have same type (should be numeric)
            annotate_expression(node, canonicalType);
            break;

        case UnaryOperatorKind::AddressOf:
            // Address-of: creates a pointer type (not implemented yet)
            errors.push_back("Address-of operator not yet implemented");
            annotate_expression(node, typeSystem.get_unresolved_type());
            break;

        case UnaryOperatorKind::Dereference:
            // Dereference: operand should be pointer, result is pointed-to type (not implemented yet)
            errors.push_back("Dereference operator not yet implemented");
            annotate_expression(node, typeSystem.get_unresolved_type());
            break;

        default:
            errors.push_back("Unknown unary operator");
            annotate_expression(node, typeSystem.get_unresolved_type());
            break;
        }
    }

    void TypeResolver::visit(IndexerExpr *node)
    {
        // Visit children manually
        if (node->object)
            node->object->accept(this);
        if (node->index)
            node->index->accept(this);
            
        TypePtr objectType = get_node_type(node->object);
        TypePtr indexType = get_node_type(node->index);

        if (!objectType || !indexType)
        {
            annotate_expression(node, typeSystem.get_unresolved_type());
            return; // Errors already reported by children
        }

        TypePtr canonicalObjectType = apply_substitution(objectType);
        TypePtr canonicalIndexType = apply_substitution(indexType);

        // Check if object is an array type
        if (auto *arrayType = std::get_if<ArrayType>(&canonicalObjectType->value))
        {
            // Index should be an integer type
            TypePtr i32Type = typeSystem.get_primitive("i32");
            unify(canonicalIndexType, i32Type, node, "array index");

            // Result type is the element type
            annotate_expression(node, arrayType->elementType);
        }
        else
        {
            errors.push_back("Cannot index non-array type '" + canonicalObjectType->get_name() + "'.");
            annotate_expression(node, typeSystem.get_unresolved_type());
        }
    }

    void TypeResolver::visit(ConditionalExpr *node)
    {
        // Visit children manually
        if (node->condition)
            node->condition->accept(this);
        if (node->thenExpr)
            node->thenExpr->accept(this);
        if (node->elseExpr)
            node->elseExpr->accept(this);
            
        TypePtr conditionType = get_node_type(node->condition);
        TypePtr thenType = get_node_type(node->thenExpr);
        TypePtr elseType = get_node_type(node->elseExpr);

        if (!conditionType || !thenType || !elseType)
        {
            annotate_expression(node, typeSystem.get_unresolved_type());
            return; // Errors already reported by children
        }

        TypePtr canonicalCondition = apply_substitution(conditionType);
        TypePtr canonicalThen = apply_substitution(thenType);
        TypePtr canonicalElse = apply_substitution(elseType);

        // Condition must be bool
        unify(canonicalCondition, typeSystem.get_primitive("bool"), node, "conditional expression condition");

        // Then and else branches must have the same type
        unify(canonicalThen, canonicalElse, node, "conditional expression branches");

        // Result type is the unified type of the branches
        annotate_expression(node, apply_substitution(canonicalThen));
    }

    void TypeResolver::visit(IfExpr *node)
    {
        // Visit children manually
        if (node->condition)
            node->condition->accept(this);
        if (node->thenBranch)
            node->thenBranch->accept(this);
        if (node->elseBranch)
            node->elseBranch->accept(this);
            
        TypePtr conditionType = get_node_type(node->condition);

        if (!conditionType)
        {
            return; // Error already reported by child
        }

        TypePtr canonicalCondition = apply_substitution(conditionType);

        // Condition must be bool
        unify(canonicalCondition, typeSystem.get_primitive("bool"), node, "if expression condition");
    }

    void TypeResolver::visit(CastExpr *node)
    {
        // Visit children manually
        if (node->expression)
            node->expression->accept(this);
        
        TypePtr expressionType = get_node_type(node->expression);

        if (!expressionType)
        {
            annotate_expression(node, typeSystem.get_unresolved_type());
            return; // Error already reported by child
        }

        // Resolve the target type from the type expression
        auto *scope = symbolTable.lookup_handle(node->containingScope)->as<Scope>();
        TypePtr targetType = resolve_ast_type_expr(node->targetType, scope);

        // TODO: Add cast compatibility checking
        // For now, we trust that any cast is valid
        annotate_expression(node, targetType);
    }

    void TypeResolver::visit(ThisExpr *node)
    {
        // ThisExpr has no children to visit
        
        // Look up the enclosing type to determine what 'this' refers to
        auto *scope = symbolTable.lookup_handle(node->containingScope)->as<Scope>();
        if (!scope)
        {
            errors.push_back("'this' expression has no containing scope.");
            return;
        }

        // Walk up the scope chain to find the enclosing type
        ScopeNode *currentScopeNode = scope->as_scope_node();
        while (currentScopeNode)
        {
            if (auto *typeSymbol = currentScopeNode->as<TypeLikeSymbol>())
            {
                // Found enclosing type, 'this' has that type
                TypePtr thisType = typeSystem.get_type_reference(typeSymbol);
                annotate_expression(node, thisType);
                return;
            }
            currentScopeNode = currentScopeNode->parent;
        }

        errors.push_back("'this' expression is not within a type definition.");
        annotate_expression(node, typeSystem.get_unresolved_type());
    }

    void TypeResolver::visit(ReturnStmt *node)
    {
        // Visit children manually
        if (node->value)
            node->value->accept(this);
        
        // Find the enclosing function to check return type compatibility
        auto *scope = symbolTable.lookup_handle(node->containingScope)->as<Scope>();
        if (!scope)
            return;

        ScopeNode *currentScopeNode = scope->as_scope_node();
        while (currentScopeNode)
        {
            if (auto *funcSymbol = currentScopeNode->as<FunctionSymbol>())
            {
                TypePtr expectedReturnType = funcSymbol->return_type();

                if (node->value)
                {
                    // Return with value
                    TypePtr valueType = get_node_type(node->value);
                    if (valueType)
                    {
                        unify(valueType, expectedReturnType, node, "return statement");
                    }
                }
                else
                {
                    // Return without value - should be void
                    TypePtr voidType = typeSystem.get_primitive("void");
                    unify(voidType, expectedReturnType, node, "void return statement");
                }
                return;
            }
            currentScopeNode = currentScopeNode->parent;
        }

        errors.push_back("Return statement not within a function.");
    }

    void TypeResolver::visit(ForStmt *node)
    {
        // Visit children manually
        if (node->initializer)
            node->initializer->accept(this);
        if (node->condition)
            node->condition->accept(this);
        for (auto *update : node->updates)
        {
            if (update)
                update->accept(this);
        }
        if (node->body)
            node->body->accept(this);
            
        // Type check the condition if present
        if (node->condition)
        {
            TypePtr conditionType = get_node_type(node->condition);
            if (conditionType)
            {
                TypePtr canonicalCondition = apply_substitution(conditionType);
                unify(canonicalCondition, typeSystem.get_primitive("bool"), node, "for loop condition");
            }
        }
    }

    void TypeResolver::visit(WhileStmt *node)
    {
        // Visit children manually
        if (node->condition)
            node->condition->accept(this);
        if (node->body)
            node->body->accept(this);
            
        // Condition must be bool
        TypePtr conditionType = get_node_type(node->condition);
        if (conditionType)
        {
            TypePtr canonicalCondition = apply_substitution(conditionType);
            unify(canonicalCondition, typeSystem.get_primitive("bool"), node, "while loop condition");
        }
    }

    void TypeResolver::visit(FunctionDecl *node)
    {
        // Visit children manually
        if (node->name)
            node->name->accept(this);

        for (auto *param : node->parameters)
        {
            if (param)
                param->accept(this);
        }

        if (node->returnType)
            node->returnType->accept(this);

        if (node->body)
            node->body->accept(this);

        // Look up the function symbol to work with its type
        auto *scope = symbolTable.lookup_handle(node->containingScope)->as<Scope>();
        if (!scope)
            return;

        auto *symbol = scope->lookup_local(node->name->text);
        auto *funcSymbol = symbol ? symbol->as<FunctionSymbol>() : nullptr;
        if (!funcSymbol)
            return;

        // If return type is unresolved, try to resolve it
        TypePtr returnType = funcSymbol->return_type();
        if (returnType && returnType->is<UnresolvedType>())
        {
            auto& unresolved = returnType->as<UnresolvedType>();
            
            // If there's a type expression, resolve it first
            if (unresolved.typeName)
            {
                TypePtr resolvedType = resolve_ast_type_expr(unresolved.typeName, scope);
                funcSymbol->set_return_type(resolvedType);
                symbolTable.mark_symbol_resolved(symbol);
            }
            // If no explicit type but has function body, infer from return statements
            else if (unresolved.body)
            {
                TypePtr inferredType = infer_function_return_type(unresolved.body);
                // if we inferred unresolved then just skip
                if (!inferredType->is<UnresolvedType>())
                {
                    funcSymbol->set_return_type(inferredType);
                    symbolTable.mark_symbol_resolved(symbol);
                }
            }
            // No explicit type and no body - default to void
            else
            {
                TypePtr voidType = typeSystem.get_primitive("void");
                funcSymbol->set_return_type(voidType);
                symbolTable.mark_symbol_resolved(symbol);
            }
        }
        else if (returnType && !returnType->is<UnresolvedType>())
        {
            // Return type is already resolved
            symbolTable.mark_symbol_resolved(symbol);
        }

        // Update parameter types after all parameters have been resolved
        std::vector<TypePtr> resolvedParamTypes;
        for (auto *paramDecl : node->parameters)
        {
            if (paramDecl && paramDecl->param && paramDecl->param->name)
            {
                std::string paramName(paramDecl->param->name->text);
                auto *functionScope = static_cast<Scope*>(funcSymbol);
                auto *paramSymbol = functionScope->lookup_local(paramName);
                if (auto *typedParamSymbol = paramSymbol ? paramSymbol->as<TypedSymbol>() : nullptr)
                {
                    TypePtr paramType = typedParamSymbol->type();
                    TypePtr resolvedType = apply_substitution(paramType);
                    
                    // Update the parameter symbol with resolved type if it's now concrete
                    if (!resolvedType->is<UnresolvedType>())
                    {
                        typedParamSymbol->set_type(resolvedType);
                        symbolTable.mark_symbol_resolved(paramSymbol);
                    }
                    
                    resolvedParamTypes.push_back(resolvedType);
                }
                else
                {
                    // Fallback - use unresolved type to maintain vector size
                    resolvedParamTypes.push_back(typeSystem.get_unresolved_type());
                }
            }
        }
    }

    void TypeResolver::visit(ParameterDecl *node)
    {
        // Visit children manually
        if (node->param)
            node->param->accept(this);
        if (node->defaultValue)
            node->defaultValue->accept(this);

        // Look up the parameter symbol in the function's scope
        // Parameters are defined in the function scope, not where the ParameterDecl node is annotated
        auto *scope = symbolTable.lookup_handle(node->containingScope)->as<Scope>();
        if (!scope)
        {
            return;
        }

        // Parameters should be in the current scope (function scope)
        auto *symbol = scope->lookup_local(node->param->name->text);
        if (!symbol)
        {
            // If not found locally, try looking in parent scope (might be in function scope)
            symbol = scope->lookup(node->param->name->text);
        }
        auto *paramSymbol = symbol ? symbol->as<ParameterSymbol>() : nullptr;
        if (!paramSymbol)
        {
            return;
        }

        // Resolve explicit type annotations that were deferred during symbol building
        TypePtr paramType = paramSymbol->type();

        if (paramType && paramType->is<UnresolvedType>())
        {
            auto& unresolved = paramType->as<UnresolvedType>();

            if (unresolved.typeName)
            {
                // Resolve the type expression now that all types are in symbol table
                TypePtr resolvedType = resolve_ast_type_expr(unresolved.typeName, scope);
                
                paramSymbol->set_type(resolvedType);
                paramType = resolvedType;
                symbolTable.mark_symbol_resolved(symbol);
            }
            else
            {
                // Parameter with no explicit type (var parameter) - needs inference
                // Create a fresh unresolved type for inference via unification
                TypePtr inferredType = typeSystem.get_unresolved_type();
                paramSymbol->set_type(inferredType);
                
                // Track this type as needing resolution through unification
                pendingConstraints.insert(inferredType);
            }
        }
    }

    void TypeResolver::visit(PropertyDecl *node)
    {
        // Handle property symbol resolution in the property's own scope context
        // Don't visit the nested VariableDecl - handle the property symbol directly
        
        auto *scope = symbolTable.lookup_handle(node->containingScope)->as<Scope>();
        if (!scope)
        {
            errors.push_back("Internal error: Property declaration has no containing scope.");
            return;
        }

        if (!node->variable || !node->variable->variable || !node->variable->variable->name)
            return;

        std::string prop_name(node->variable->variable->name->text);
        auto *symbol = scope->lookup(prop_name);
        auto *prop_symbol = symbol ? symbol->as<TypedSymbol>() : nullptr;

        if (prop_symbol)
        {
            TypePtr propType = prop_symbol->type();

            // Resolve explicit type annotations that were deferred during symbol building
            if (propType && propType->is<UnresolvedType>())
            {
                auto& unresolved = propType->as<UnresolvedType>();
                if (unresolved.typeName)
                {
                    TypePtr resolvedType = resolve_ast_type_expr(unresolved.typeName, scope);
            
                    // Remove the old unresolved type from pending constraints
                    pendingConstraints.erase(propType);
                    
                    // Add substitution so expressions holding the old type can resolve
                    substitution[propType] = resolvedType;
                    
                    prop_symbol->set_type(resolvedType);
                    symbolTable.mark_symbol_resolved(symbol);
                }
            }
        }
        
        // Visit accessors (they contain the expressions that need resolution)
        if (node->getter)
        {
            node->getter->accept(this);
            
            // For type inference: if property type is unresolved, infer from getter
            if (prop_symbol && prop_symbol->type()->is<UnresolvedType>())
            {
                // Get the type of the getter expression
                TypePtr getterType = nullptr;
                if (auto *expr = std::get_if<Expression *>(&node->getter->body))
                {
                    if (*expr)
                    {
                        getterType = get_node_type(*expr);
                    }
                }
                
                if (getterType && !getterType->is<UnresolvedType>())
                {
                    // Infer the property type from the getter
                    TypePtr oldType = prop_symbol->type();
                    
                    // Remove old unresolved type from pending constraints
                    pendingConstraints.erase(oldType);
                    
                    // Add substitution for type unification
                    substitution[oldType] = getterType;
                    
                    // Update the symbol
                    prop_symbol->set_type(getterType);
                    symbolTable.mark_symbol_resolved(symbol);
                }
            }
        }
        if (node->setter)
        {
            node->setter->accept(this);
        }
    }

    void TypeResolver::visit(PropertyAccessor *node)
    {
        // Visit the accessor body to resolve types in expressions
        if (auto *expr = std::get_if<Expression *>(&node->body))
        {
            if (*expr)
                (*expr)->accept(this);
        }
        else if (auto *block = std::get_if<Block *>(&node->body))
        {
            if (*block)
                (*block)->accept(this);
        }
    }

    void TypeResolver::visit(TypedIdentifier *node)
    {
        // Visit the name
        if (node->name)
        {
            node->name->accept(this);
        }
    }

} // namespace Myre