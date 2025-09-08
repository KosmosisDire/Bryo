#include "semantic/type_resolver.hpp"

namespace Bryo
{

    // ============================================================================
    // --- TypeResolver Implementation ---
    // ============================================================================
    bool TypeResolver::resolve(CompilationUnitSyntax *unit)
    {
        nodeTypes.clear();
        substitution.clear();
        pendingConstraints.clear();
        currentTypeParameters.clear();

        // Phase 1: Repeatedly traverse the AST to gather constraints and solve for
        // inferred types until no more progress can be made.
        int pass = 0;
        while (pass < passes_to_run)
        {
            errors.clear();
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

    void TypeResolver::unify(TypePtr t1, TypePtr t2, BaseSyntax *error_node, const std::string &context)
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
            report_error(error_node, "'" + root1->get_name() + "' and '" + root2->get_name() + "' are not compatible types.");
        }
    }

    void TypeResolver::report_final_errors()
    {
        for (auto &[node, type] : nodeTypes)
        {
            TypePtr canonical = apply_substitution(type);
            if (canonical->is<UnresolvedType>())
            {
                // errors.push_back("Could not infer type for expression at line " + node->location.start.to_string());
            }
        }
    }

    void TypeResolver::report_error(BaseSyntax *error_node, const std::string &message)
    {
        if (error_node)
        {
            errors.push_back("Error at " + error_node->location.start.to_string() + ": " + message);
        }
        else
        {
            errors.push_back("Error: " + message);
        }
    }

    void TypeResolver::update_ast_with_final_types(CompilationUnitSyntax *unit)
    {
        class TypeUpdateVisitor : public DefaultVisitor
        {
            TypeResolver *resolver;

        public:
            TypeUpdateVisitor(TypeResolver *res) : resolver(res) {}

            void visit(BaseExprSyntax *node) override
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

    TypePtr TypeResolver::get_node_type(BaseSyntax *node)
    {
        auto it = nodeTypes.find(node);
        return (it != nodeTypes.end()) ? it->second : nullptr;
    }

    void TypeResolver::set_node_type(BaseSyntax *node, TypePtr type)
    {
        if (node && type)
        {
            nodeTypes[node] = type;
        }
    }

    void TypeResolver::annotate_expression(BaseExprSyntax *expr, TypePtr type, Symbol *symbol)
    {
        if (!expr || !type)
            return;

        // 1. Type annotation (always happens)
        set_node_type(expr, type);
        TypePtr canonical = apply_substitution(type);
        expr->resolvedType = canonical;

        // 2. Set expression-specific symbol fields using is() and as()
        if (symbol)
        {
            set_expression_symbol(expr, symbol);
        }

        // 3. Lvalue/rvalue determination (uses both expression type and symbol)
        expr->isLValue = compute_lvalue_status(expr, symbol);

        // 4. Constraint tracking
        if (canonical->is<UnresolvedType>())
        {
            pendingConstraints.insert(canonical);
        }
        else
        {
            pendingConstraints.erase(canonical);
        }
    }

    void TypeResolver::set_expression_symbol(BaseExprSyntax *expr, Symbol *symbol)
    {
        if (!expr || !symbol)
            return;

        // Use is() and as() to set the appropriate symbol field
        if (auto baseName = expr->as<BaseNameExprSyntax>())
        {
            baseName->resolvedSymbol = symbol->handle;
        }
        else if (auto qualified = expr->as<QualifiedNameSyntax>())
        {
            qualified->resolvedSymbol = symbol->handle;
        }
        else if (expr->is<CallExprSyntax>())
        {
            expr->as<CallExprSyntax>()->resolvedCallee = symbol->handle;
        }
        // Other expression types don't have symbol fields
    }

    Scope *TypeResolver::get_containing_scope(BaseSyntax *node)
    {
        if (!node || node->containingScope.id == 0)
            return nullptr;
        auto sym = symbolTable.lookup_handle(node->containingScope)->as<Scope>();

        if (!sym)
        {
            report_error(node, "Internal Error: Expression has no containing scope.");
            if (auto exp = node->as<BaseExprSyntax>())
                annotate_expression(exp, typeSystem.get_unresolved_type());
        }

        return sym;
    }

    bool TypeResolver::compute_lvalue_status(BaseExprSyntax *expr, Symbol *symbol)
    {
        if (!expr)
            return false;

        // Array indexing is always an lvalue
        if (expr->is<IndexerExprSyntax>())
        {
            return true;
        }

        // 'this' is always an lvalue
        if (expr->is<ThisExprSyntax>())
        {
            return true;
        }

        // Dereference produces lvalue
        if (expr->is<UnaryExprSyntax>())
        {
            auto unary = expr->as<UnaryExprSyntax>();
            if (unary->op == UnaryOperatorKind::Dereference)
            {
                return true;
            }
        }

        // For expressions with symbols, check the symbol type
        if (symbol)
        {
            // Variables and parameters are lvalues
            if (symbol->is<VariableSymbol>() || symbol->is<ParameterSymbol>())
            {
                return true;
            }

            // Properties with setters are lvalues
            if (symbol->is<PropertySymbol>())
            {
                auto prop = symbol->as<PropertySymbol>();
                return prop->has_setter();
            }
        }

        // Everything else is an rvalue
        return false;
    }

    TypePtr TypeResolver::resolve_expr_type(BaseExprSyntax *type_expr, Scope *scope)
    {
        if (!type_expr || !scope)
            return typeSystem.get_unresolved_type();

        // Single identifier (like "i32", "String", "T")
        if (auto name = type_expr->as<BaseNameExprSyntax>())
        {
            std::string typeName = name->get_name();

            // Check for primitive types first
            TypePtr primType = typeSystem.get_primitive_type(typeName);
            if (primType)
            {
                return primType;
            }

            // Check if this is a type parameter in the current generic context
            auto typeParamIt = currentTypeParameters.find(typeName);
            if (typeParamIt != currentTypeParameters.end())
            {
                return typeParamIt->second;
            }

            // Otherwise look up in symbol table
            return symbolTable.resolve_type_name(typeName, scope->as_scope_node());
        }
        // Array type (like "i32[]")
        else if (auto array = type_expr->as<ArrayTypeSyntax>())
        {
            TypePtr elemType = resolve_expr_type(array->baseType, scope);
            int arrSize = -1;
            if (array->size)
            {
                arrSize = std::stoi(std::string(array->size->value));
            }
            return typeSystem.get_array_type(elemType, arrSize);
        }
        else if (auto pointer = type_expr->as<PointerTypeSyntax>())
        {
            TypePtr baseType = resolve_expr_type(pointer->baseType, scope);
            if (!baseType)
                return typeSystem.get_unresolved_type();
            return typeSystem.get_pointer_type(baseType);
        }

        return typeSystem.get_unresolved_type();
    }

    TypePtr TypeResolver::infer_function_return_type(BlockSyntax *body)
    {
        if (!body)
            return typeSystem.get_primitive_type("void");

        // Helper class to find return statements and check their types
        class ReturnTypeFinder : public DefaultVisitor
        {
        public:
            TypePtr commonReturnType = nullptr;
            TypeResolver *resolver;
            bool hasVoidReturn = false;
            bool hasInvalidVoidReturn = false;
            BaseSyntax *invalidVoidReturnNode = nullptr;

            ReturnTypeFinder(TypeResolver *res) : resolver(res) {}

            void visit(ReturnStmtSyntax *node) override
            {
                if (node->value)
                {
                    // Return with value
                    TypePtr valueType = resolver->get_node_type(node->value);
                    if (valueType)
                    {
                        TypePtr canonicalValue = resolver->apply_substitution(valueType);

                        // Check if trying to return void expression (which is invalid)
                        if (canonicalValue && !canonicalValue->is<UnresolvedType>() &&
                            canonicalValue->is_void())
                        {
                            hasInvalidVoidReturn = true;
                            invalidVoidReturnNode = node;
                            // Don't include this in type inference - it's an error
                            return;
                        }

                        // Valid non-void return value
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
                    // Return without value (valid for void functions)
                    hasVoidReturn = true;
                }
            }

            void visit(FunctionDeclSyntax *node) override
            {
                // Don't visit nested functions - their return statements don't affect outer function
            }
        };

        ReturnTypeFinder finder(this);
        body->accept(&finder);

        // Report error if we found invalid void returns
        if (finder.hasInvalidVoidReturn && finder.invalidVoidReturnNode)
        {
            report_error(finder.invalidVoidReturnNode, "Cannot return void expression");
            // Continue with inference anyway to find other errors
        }

        // Determine final return type
        if (finder.commonReturnType && !finder.hasVoidReturn)
        {
            // All valid returns have values and consistent type
            return finder.commonReturnType;
        }
        else if (finder.hasVoidReturn && !finder.commonReturnType)
        {
            // All returns are void (no value)
            return typeSystem.get_primitive_type("void");
        }
        else if (finder.commonReturnType && finder.hasVoidReturn)
        {
            // Mixed return types - some with value, some without
            // This is an error - a function can't sometimes return void and sometimes return a value
            // For now, prefer the non-void type to continue analysis
            report_error(body, "Inconsistent return types: function has both void and non-void returns");
            return finder.commonReturnType;
        }
        else
        {
            // No return statements found - default to void
            return typeSystem.get_primitive_type("void");
        }
    }

    ConversionKind TypeResolver::check_conversion(TypePtr from, TypePtr to)
    {
        if (!from || !to)
            return ConversionKind::NoConversion;

        TypePtr canonicalFrom = apply_substitution(from);
        TypePtr canonicalTo = apply_substitution(to);

        // Check for unresolved types
        if (canonicalFrom->is<UnresolvedType>() || canonicalTo->is<UnresolvedType>())
        {
            // Can't determine conversion for unresolved types yet
            return ConversionKind::NoConversion;
        }

        return Conversions::ClassifyConversion(canonicalFrom, canonicalTo);
    }

    bool TypeResolver::check_implicit_conversion(TypePtr from, TypePtr to, BaseSyntax *error_node, const std::string &context)
    {
        ConversionKind kind = check_conversion(from, to);

        if (Conversions::IsImplicitConversion(kind))
        {
            return true;
        }
        else if (Conversions::IsExplicitConversion(kind))
        {
            report_error(error_node, "Cannot implicitly convert '" + from->get_name() +
                                         "' to '" + to->get_name() + "' - explicit cast required");
            return false;
        }
        else
        {
            report_error(error_node, "Cannot convert '" + from->get_name() +
                                         "' to '" + to->get_name() + "'");
            return false;
        }
    }

    bool TypeResolver::check_explicit_conversion(TypePtr from, TypePtr to, BaseSyntax *error_node, const std::string &context)
    {
        ConversionKind kind = check_conversion(from, to);

        if (Conversions::IsConversionPossible(kind))
        {
            return true;
        }
        else
        {
            report_error(error_node, "Cannot convert '" + from->get_name() + "' to '" + to->get_name() + "'");
            return false;
        }
    }

    FunctionSymbol *TypeResolver::resolve_overload(const std::vector<FunctionSymbol *> &overloads, const std::vector<TypePtr> &argTypes)
    {
        if (overloads.empty())
            return nullptr;

        // Single overload - no resolution needed
        if (overloads.size() == 1)
        {
            FunctionSymbol *func = overloads[0];
            if (func->parameters().size() == argTypes.size())
            {
                // Check if all arguments can convert to parameters
                for (size_t i = 0; i < argTypes.size(); ++i)
                {
                    auto paramSymbol = func->parameters()[i];
                    if (!paramSymbol)
                        return nullptr;

                    TypePtr paramType = apply_substitution(paramSymbol->type());

                    // IMPORTANT: Handle unresolved parameter types
                    if (paramType->is<UnresolvedType>())
                    {
                        // If parameter type is unresolved, unify it with the argument type
                        // This allows type inference to work during overload resolution
                        unify(paramType, argTypes[i], nullptr, "parameter inference");
                        continue; // Accept this parameter for now
                    }

                    ConversionKind conv = check_conversion(argTypes[i], paramType);

                    // Only allow Identity and ImplicitNumeric for function calls
                    if (!Conversions::IsImplicitConversion(conv))
                        return nullptr;
                }
                return func;
            }
            return nullptr;
        }

        // Multiple overloads - need resolution
        std::vector<FunctionSymbol *> viable;
        std::vector<std::vector<ConversionKind>> viableConversions;

        // Phase 1: Find viable candidates
        for (auto func : overloads)
        {
            // Check parameter count
            if (func->parameters().size() != argTypes.size())
                continue;

            std::vector<ConversionKind> conversions;
            bool is_viable = true;

            // Check each argument
            for (size_t i = 0; i < argTypes.size(); ++i)
            {
                auto paramSymbol = func->parameters()[i];
                if (!paramSymbol)
                {
                    is_viable = false;
                    break;
                }

                TypePtr paramType = apply_substitution(paramSymbol->type());

                // IMPORTANT: Handle unresolved parameter types
                if (paramType->is<UnresolvedType>())
                {
                    // If parameter type is unresolved, treat it as a potential match
                    // and unify it with the argument type
                    unify(paramType, argTypes[i], nullptr, "parameter inference");
                    conversions.push_back(ConversionKind::Identity); // Treat as exact match after unification
                    continue;
                }

                ConversionKind conv = check_conversion(argTypes[i], paramType);

                // IMPORTANT: ExplicitNumeric conversions are NOT allowed for function calls
                // Only Identity and ImplicitNumeric are viable
                if (!Conversions::IsImplicitConversion(conv))
                {
                    is_viable = false;
                    break;
                }

                conversions.push_back(conv);
            }

            if (is_viable)
            {
                viable.push_back(func);
                viableConversions.push_back(conversions);
            }
        }

        if (viable.empty())
            return nullptr;

        if (viable.size() == 1)
            return viable[0];

        // Phase 2: Find best match among viable candidates
        // Prefer functions with more Identity conversions over ImplicitNumeric
        int bestIndex = 0;
        int bestIdentityCount = count_conversions(viableConversions[0], ConversionKind::Identity);

        for (size_t i = 1; i < viable.size(); ++i)
        {
            int identityCount = count_conversions(viableConversions[i], ConversionKind::Identity);

            if (identityCount > bestIdentityCount)
            {
                bestIndex = i;
                bestIdentityCount = identityCount;
            }
            else if (identityCount == bestIdentityCount)
            {
                // Ambiguity - both have same number of exact matches
                // Could report ambiguity error here
                // For now, we could use additional criteria like:
                // - Prefer narrower implicit conversions (i32->i64 over i32->f64)
                // - Or just report ambiguity
            }
        }

        return viable[bestIndex];
    }

    int TypeResolver::count_conversions(const std::vector<ConversionKind> &conversions, ConversionKind kind)
    {
        int count = 0;
        for (auto conv : conversions)
        {
            if (conv == kind)
                count++;
        }
        return count;
    }

    // --- Visitor Implementations ---

    void TypeResolver::visit(LiteralExprSyntax *node)
    {
        std::string type_name = std::string(to_string(node->kind));
        annotate_expression(node, typeSystem.get_primitive_type(type_name));
    }

    void TypeResolver::visit(ArrayLiteralExprSyntax *node)
    {
        // Visit all elements first
        for (auto elem : node->elements)
        {
            if (elem)
                elem->accept(this);
        }

        // Handle empty array
        if (node->elements.empty())
        {
            annotate_expression(node, typeSystem.get_unresolved_type());
            return;
        }

        // Unify element types
        TypePtr elementType = nullptr;
        for (auto elem : node->elements)
        {
            if (!elem)
                continue;

            TypePtr elemType = get_node_type(elem);
            if (!elemType)
            {
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

        // Create and annotate array type
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

    void TypeResolver::visit(BaseNameExprSyntax *node)
    {
        std::string name = node->get_name();
        auto scope = get_containing_scope(node);
        if (!scope)
            return;

        auto symbol = scope->lookup(name);
        if (!symbol)
        {
            report_error(node, "Identifier '" + name + "' not found");
            annotate_expression(node, typeSystem.get_unresolved_type());
            return;
        }

        if (auto typed_symbol = symbol->as<TypedSymbol>())
        {
            // Single call to annotate_expression handles everything!
            annotate_expression(node, typed_symbol->type(), symbol);
        }
        else if (auto func_group = symbol->as<FunctionGroupSymbol>())
        {
            // For function groups, we can't determine the exact type without call context
            // Mark as a special function reference type or handle in CallExprSyntax
            annotate_expression(node, typeSystem.get_unresolved_type(), symbol);
        }
        else
        {
            report_error(node, "'" + name + "' is not a value");
            annotate_expression(node, typeSystem.get_unresolved_type());
        }
    }

    void TypeResolver::visit(BinaryExprSyntax *node)
    {
        // Visit children
        node->left->accept(this);
        node->right->accept(this);

        TypePtr leftType = get_node_type(node->left);
        TypePtr rightType = get_node_type(node->right);

        if (!leftType || !rightType)
        {
            annotate_expression(node, typeSystem.get_unresolved_type());
            return;
        }

        TypePtr canonicalLeft = apply_substitution(leftType);
        TypePtr canonicalRight = apply_substitution(rightType);

        // For comparison operators
        switch (node->op)
        {
        case BinaryOperatorKind::Equals:
        case BinaryOperatorKind::NotEquals:
        case BinaryOperatorKind::LessThan:
        case BinaryOperatorKind::LessThanOrEqual:
        case BinaryOperatorKind::GreaterThan:
        case BinaryOperatorKind::GreaterThanOrEqual:
            // Check if types can be compared (must have implicit conversion in at least one direction)
            if (!canonicalLeft->is<UnresolvedType>() && !canonicalRight->is<UnresolvedType>())
            {
                ConversionKind leftToRight = check_conversion(canonicalLeft, canonicalRight);
                ConversionKind rightToLeft = check_conversion(canonicalRight, canonicalLeft);

                if (!Conversions::IsImplicitConversion(leftToRight) &&
                    !Conversions::IsImplicitConversion(rightToLeft))
                {
                    report_error(node, "Cannot compare '" + canonicalLeft->get_name() + "' and '" + canonicalRight->get_name() + "'");
                }
            }
            else
            {
                // Fall back to unification for unresolved types
                unify(leftType, rightType, node, "comparison");
            }
            annotate_expression(node, typeSystem.get_primitive_type("bool"));
            break;

        default: // Arithmetic operators
            // Check if implicit conversion exists in either direction
            if (!canonicalLeft->is<UnresolvedType>() && !canonicalRight->is<UnresolvedType>())
            {
                ConversionKind leftToRight = check_conversion(canonicalLeft, canonicalRight);
                ConversionKind rightToLeft = check_conversion(canonicalRight, canonicalLeft);

                // Determine result type based on implicit conversions
                TypePtr resultType = nullptr;
                if (Conversions::IsImplicitConversion(rightToLeft))
                {
                    resultType = canonicalLeft; // Left type is "wider"
                }
                else if (Conversions::IsImplicitConversion(leftToRight))
                {
                    resultType = canonicalRight; // Right type is "wider"
                }
                else if (leftToRight == ConversionKind::Identity)
                {
                    resultType = canonicalLeft; // Same type
                }
                else
                {
                    report_error(node, "Cannot apply binary operator to incompatible types '" +
                                           canonicalLeft->get_name() + "' and '" + canonicalRight->get_name() + "'");
                    resultType = typeSystem.get_unresolved_type();
                }
                annotate_expression(node, resultType);
            }
            else
            {
                // Fall back to unification for unresolved types
                unify(leftType, rightType, node, "binary expression");
                annotate_expression(node, apply_substitution(leftType));
            }
            break;
        }
    }

    void TypeResolver::visit(AssignmentExprSyntax *node)
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

        // Check if implicit conversion is possible
        TypePtr canonicalTarget = apply_substitution(targetType);
        TypePtr canonicalValue = apply_substitution(valueType);

        // If both types are resolved, check conversion
        if (!canonicalTarget->is<UnresolvedType>() && !canonicalValue->is<UnresolvedType>())
        {
            check_implicit_conversion(canonicalValue, canonicalTarget, node, "assignment");
        }
        else
        {
            // Fall back to unification for unresolved types
            unify(targetType, valueType, node, "assignment");
        }

        annotate_expression(node, canonicalTarget);
    }

    void TypeResolver::visit(CallExprSyntax *node)
    {
        // Visit children manually
        node->callee->accept(this);
        for (auto arg : node->arguments)
        {
            if (arg)
                arg->accept(this);
        }

        // Collect argument types for overload resolution
        std::vector<TypePtr> argTypes;
        for (auto arg : node->arguments)
        {
            if (arg)
            {
                TypePtr argType = get_node_type(arg);
                if (argType)
                {
                    argTypes.push_back(apply_substitution(argType));
                }
            }
        }

        // Handle simple name function calls (e.g., foo())
        if (auto name = node->callee->as<BaseNameExprSyntax>())
        {
            auto scope = get_containing_scope(name);
            if (!scope)
                return;

            // Collect all visible overloads
            std::vector<FunctionSymbol *> overloads = scope->lookup_functions(name->get_name());

            if (!overloads.empty())
            {
                // Resolve best overload
                FunctionSymbol *best = resolve_overload(overloads, argTypes);
                if (best)
                {
                    annotate_expression(node, best->return_type());
                    node->resolvedCallee = best->handle;
                    return;
                }
                else
                {
                    // Build argument types string for error message
                    std::string argTypesStr = "(";
                    for (size_t i = 0; i < argTypes.size(); ++i)
                    {
                        if (i > 0)
                            argTypesStr += ", ";
                        argTypesStr += argTypes[i]->get_name();
                    }
                    argTypesStr += ")";

                    // Find the closest matching overload for better error message
                    std::string closestMatch = "";
                    if (!overloads.empty())
                    {
                        // Find overload with matching parameter count
                        for (auto func : overloads)
                        {
                            if (func->parameters().size() == argTypes.size())
                            {
                                std::string paramTypesStr = "(";
                                for (size_t i = 0; i < func->parameters().size(); ++i)
                                {
                                    if (i > 0)
                                        paramTypesStr += ", ";
                                    TypePtr paramType = apply_substitution(func->parameters()[i]->type());
                                    paramTypesStr += paramType->get_name();
                                }
                                paramTypesStr += ")";
                                closestMatch = paramTypesStr;
                                break;
                            }
                        }

                        // If no matching parameter count, show first overload
                        if (closestMatch.empty() && !overloads.empty())
                        {
                            auto func = overloads[0];
                            std::string paramTypesStr = "(";
                            for (size_t i = 0; i < func->parameters().size(); ++i)
                            {
                                if (i > 0)
                                    paramTypesStr += ", ";
                                TypePtr paramType = apply_substitution(func->parameters()[i]->type());
                                paramTypesStr += paramType->get_name();
                            }
                            paramTypesStr += ")";
                            closestMatch = paramTypesStr;
                        }
                    }

                    // Only report error if we have resolved argument types
                    // If argument types are still unresolved, don't report an error yet
                    bool hasUnresolvedArgs = false;
                    for (auto argType : argTypes)
                    {
                        if (argType->is<UnresolvedType>())
                        {
                            hasUnresolvedArgs = true;
                            break;
                        }
                    }

                    if (!hasUnresolvedArgs)
                    {
                        std::string errorMsg = "No matching overload for '" + name->get_name() +
                                               "' with argument types " + argTypesStr;
                        if (!closestMatch.empty())
                        {
                            errorMsg += ". Available overload expects: " + closestMatch;
                        }

                        report_error(node, errorMsg);
                    }

                    annotate_expression(node, typeSystem.get_unresolved_type());
                    return;
                }
            }
        }

        // Handle member access function calls (e.g., obj.method())
        else if (auto memberAccess = node->callee->as<QualifiedNameSyntax>())
        {
            TypePtr objectType = get_node_type(memberAccess->left);
            if (objectType)
            {
                TypePtr canonicalType = apply_substitution(objectType);

                // Check if the object type has the called method
                if (auto typeRef = std::get_if<TypeReference>(&canonicalType->value))
                {
                    TypeLikeSymbol *typeSymbol = typeRef->definition;
                    if (auto scope = typeSymbol->as<Scope>())
                    {
                        std::string methodName = memberAccess->right->get_name();

                        // Collect overloads for this method
                        std::vector<FunctionSymbol *> overloads = scope->lookup_functions_local(methodName);

                        if (!overloads.empty())
                        {
                            // Resolve best overload
                            FunctionSymbol *best = resolve_overload(overloads, argTypes);
                            if (best)
                            {
                                annotate_expression(node, best->return_type());
                                node->resolvedCallee = best->handle;
                                memberAccess->resolvedSymbol = best->handle;
                                return;
                            }
                            else
                            {
                                // Only report error if we have resolved argument types
                                bool hasUnresolvedArgs = false;
                                for (auto argType : argTypes)
                                {
                                    if (argType->is<UnresolvedType>())
                                    {
                                        hasUnresolvedArgs = true;
                                        break;
                                    }
                                }

                                if (!hasUnresolvedArgs)
                                {
                                    report_error(node, "No matching overload for method '" +
                                                           methodName + "' with argument types");
                                }
                                annotate_expression(node, typeSystem.get_unresolved_type());
                                return;
                            }
                        }
                    }
                }
            }
        }

        report_error(node, "Expression is not callable");
        annotate_expression(node, typeSystem.get_unresolved_type());
    }

    void TypeResolver::visit(NewExprSyntax *node)
    {
        // Visit constructor arguments
        for (auto arg : node->arguments)
        {
            if (arg)
                arg->accept(this);
        }

        auto scope = get_containing_scope(node);
        if (!scope)
            return;

        annotate_expression(node, resolve_expr_type(node->type, scope));
    }

    void TypeResolver::visit(VariableDeclSyntax *node)
    {
        auto scope = get_containing_scope(node);
        if (!scope)
            return;

        auto symbol = scope->lookup(node->variable->name->get_name());
        auto var_symbol = symbol ? symbol->as<TypedSymbol>() : nullptr;

        if (var_symbol)
        {
            TypePtr varType = var_symbol->type();

            if (node->variable->type)
            {
                TypePtr resolvedType = resolve_expr_type(node->variable->type, scope);
                if (!resolvedType->is<UnresolvedType>())
                {
                    var_symbol->set_type(resolvedType);
                    varType = resolvedType;
                    symbolTable.mark_symbol_resolved(symbol);
                }
            }

            // Handle initializer
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

    void TypeResolver::visit(QualifiedNameSyntax *node)
    {
        // Visit children
        if (node->left)
            node->left->accept(this);
        if (node->right)
            node->right->accept(this);

        TypePtr objectType = get_node_type(node->left);
        if (!objectType)
        {
            annotate_expression(node, typeSystem.get_unresolved_type());
            return;
        }

        TypePtr canonicalType = apply_substitution(objectType);

        // Extract type symbol
        TypeLikeSymbol *typeSymbol = nullptr;
        std::vector<TypePtr> typeArguments;

        if (auto typeRef = std::get_if<TypeReference>(&canonicalType->value))
        {
            typeSymbol = typeRef->definition;
        }
        else if (auto genericType = std::get_if<GenericType>(&canonicalType->value))
        {
            typeSymbol = genericType->genericDefinition;
            typeArguments = genericType->typeArguments;
        }

        if (!typeSymbol)
        {
            report_error(node, "Cannot access members of non-type expression");
            annotate_expression(node, typeSystem.get_unresolved_type());
            return;
        }

        auto scope = typeSymbol->as<Scope>();
        if (!scope)
        {
            report_error(node, "Type is not a scoped type");
            annotate_expression(node, typeSystem.get_unresolved_type());
            return;
        }

        // Look up member
        std::string memberName = node->right->get_name();
        auto member = scope->lookup_local(memberName);

        if (!member)
        {
            report_error(node, "Member '" + memberName + "' not found");
            annotate_expression(node, typeSystem.get_unresolved_type());
            return;
        }

        if (auto typed_member = member->as<TypedSymbol>())
        {
            TypePtr memberType = typed_member->type();

            // Handle generic type parameter substitution
            if (!typeArguments.empty() && memberType->is<TypeParameter>())
            {
                auto &typeParam = memberType->as<TypeParameter>();
                if (typeParam.parameterId >= 0 && typeParam.parameterId < typeArguments.size())
                {
                    memberType = typeArguments[typeParam.parameterId];
                }
            }

            // Single annotation call handles everything!
            annotate_expression(node, memberType, member);
        }
        else if (auto func_group = member->as<FunctionGroupSymbol>())
        {
            // For function groups (methods), we can't determine the exact type without call context
            // Mark with unresolved type but store the symbol for later overload resolution
            annotate_expression(node, typeSystem.get_unresolved_type(), member);
            node->resolvedSymbol = func_group->handle;
        }
        else
        {
            report_error(node, "Member '" + memberName + "' is not a value");
            annotate_expression(node, typeSystem.get_unresolved_type());
        }
    }

    void TypeResolver::visit(UnaryExprSyntax *node)
    {
        // Visit operand
        if (node->operand)
            node->operand->accept(this);

        TypePtr operandType = get_node_type(node->operand);
        if (!operandType)
        {
            annotate_expression(node, typeSystem.get_unresolved_type());
            return;
        }

        TypePtr canonicalType = apply_substitution(operandType);

        switch (node->op)
        {
        case UnaryOperatorKind::Plus:
        case UnaryOperatorKind::Minus:
            annotate_expression(node, canonicalType);
            break;

        case UnaryOperatorKind::Not:
            unify(canonicalType, typeSystem.get_primitive_type("bool"), node, "logical not operand");
            annotate_expression(node, typeSystem.get_primitive_type("bool"));
            break;

        case UnaryOperatorKind::BitwiseNot:
        case UnaryOperatorKind::PreIncrement:
        case UnaryOperatorKind::PreDecrement:
        case UnaryOperatorKind::PostIncrement:
        case UnaryOperatorKind::PostDecrement:
            annotate_expression(node, canonicalType);
            break;

        case UnaryOperatorKind::AddressOf:
            // Create pointer type
            annotate_expression(node, typeSystem.get_pointer_type(canonicalType));
            break;

        case UnaryOperatorKind::Dereference:
            // Check if operand is a pointer
            if (auto ptrType = std::get_if<PointerType>(&canonicalType->value))
            {
                annotate_expression(node, ptrType->pointeeType);
            }
            else
            {
                report_error(node, "Cannot dereference non-pointer type '" + canonicalType->get_name() + "'");
                annotate_expression(node, typeSystem.get_unresolved_type());
            }
            break;

        default:
            report_error(node, "Unknown unary operator");
            annotate_expression(node, typeSystem.get_unresolved_type());
            break;
        }
        // No symbol for unary operations
    }
    void TypeResolver::visit(IndexerExprSyntax *node)
    {
        // Visit children
        if (node->object)
            node->object->accept(this);
        if (node->index)
            node->index->accept(this);

        TypePtr objectType = get_node_type(node->object);
        TypePtr indexType = get_node_type(node->index);

        if (!objectType || !indexType)
        {
            annotate_expression(node, typeSystem.get_unresolved_type());
            return;
        }

        TypePtr canonicalObjectType = apply_substitution(objectType);
        TypePtr canonicalIndexType = apply_substitution(indexType);

        // Check for array type
        if (auto arrayType = std::get_if<ArrayType>(&canonicalObjectType->value))
        {
            // Index should be integer
            unify(canonicalIndexType, typeSystem.get_primitive_type("i32"), node, "array index");
            annotate_expression(node, arrayType->elementType);
        }
        // Check for pointer type (pointer arithmetic)
        else if (auto ptrType = std::get_if<PointerType>(&canonicalObjectType->value))
        {
            unify(canonicalIndexType, typeSystem.get_primitive_type("i32"), node, "pointer index");
            annotate_expression(node, ptrType->pointeeType);
        }
        else
        {
            report_error(node, "Cannot index type '" + canonicalObjectType->get_name() + "'");
            annotate_expression(node, typeSystem.get_unresolved_type());
        }
        // No symbol for indexing
    }

    void TypeResolver::visit(ConditionalExprSyntax *node)
    {
        // Visit children
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
            return;
        }

        // Condition must be bool
        unify(apply_substitution(conditionType), typeSystem.get_primitive_type("bool"),
              node, "conditional expression condition");

        // Branches must have same type
        unify(thenType, elseType, node, "conditional expression branches");

        annotate_expression(node, apply_substitution(thenType));
        // No symbol for conditionals
    }

    void TypeResolver::visit(IfStmtSyntax *node)
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
        unify(canonicalCondition, typeSystem.get_primitive_type("bool"), node, "if expression condition");
    }

    void TypeResolver::visit(CastExprSyntax *node)
    {
        // Visit expression
        if (node->expression)
            node->expression->accept(this);

        auto scope = get_containing_scope(node);
        if (!scope)
            return;

        TypePtr targetType = resolve_expr_type(node->targetType, scope);
        TypePtr sourceType = get_node_type(node->expression);

        if (sourceType && targetType)
        {
            TypePtr canonicalSource = apply_substitution(sourceType);
            TypePtr canonicalTarget = apply_substitution(targetType);

            // Check if the cast is valid
            if (!canonicalSource->is<UnresolvedType>() && !canonicalTarget->is<UnresolvedType>())
            {
                check_explicit_conversion(canonicalSource, canonicalTarget, node, "cast expression");
            }
        }

        annotate_expression(node, targetType);
    }

    void TypeResolver::visit(ThisExprSyntax *node)
    {
        // Find enclosing type
        auto scope = get_containing_scope(node);
        if (!scope)
            return;

        ScopeNode *currentScopeNode = scope->as_scope_node();
        while (currentScopeNode)
        {
            if (auto typeSymbol = currentScopeNode->as<TypeLikeSymbol>())
            {
                TypePtr thisType = typeSystem.get_type_reference(typeSymbol);
                annotate_expression(node, thisType);
                return;
            }
            currentScopeNode = currentScopeNode->parent;
        }

        report_error(node, "'this' is not within a type definition");
        annotate_expression(node, typeSystem.get_unresolved_type());
    }

    void TypeResolver::visit(ReturnStmtSyntax *node)
    {
        // Visit children manually
        if (node->value)
            node->value->accept(this);

        // Find the enclosing function to check return type compatibility
        auto scope = get_containing_scope(node);
        if (!scope)
            return;

        ScopeNode *currentScopeNode = scope->as_scope_node();
        while (currentScopeNode)
        {
            if (auto funcSymbol = currentScopeNode->as<FunctionSymbol>())
            {
                TypePtr expectedReturnType = funcSymbol->return_type();

                if (node->value)
                {
                    // Return with value
                    TypePtr valueType = get_node_type(node->value);
                    if (valueType)
                    {
                        TypePtr canonicalValue = apply_substitution(valueType);

                        // CRITICAL CHECK: Cannot return a void expression
                        if (canonicalValue && !canonicalValue->is<UnresolvedType>() &&
                            canonicalValue->is_void())
                        {
                            report_error(node, "Cannot return void expression");
                            return;
                        }

                        if (expectedReturnType)
                        {
                            TypePtr canonicalExpected = apply_substitution(expectedReturnType);

                            // Check implicit conversion for return
                            if (!canonicalValue->is<UnresolvedType>() &&
                                !canonicalExpected->is<UnresolvedType>())
                            {
                                check_implicit_conversion(canonicalValue, canonicalExpected,
                                                          node, "return statement");
                            }
                            else
                            {
                                // Fall back to unification for unresolved types
                                unify(valueType, expectedReturnType, node, "return statement");
                            }
                        }
                    }
                }
                else
                {
                    // Return without value - should be void
                    TypePtr voidType = typeSystem.get_primitive_type("void");
                    if (expectedReturnType && !expectedReturnType->is<UnresolvedType>())
                    {
                        TypePtr canonicalExpected = apply_substitution(expectedReturnType);
                        if (!canonicalExpected->is_void())
                        {
                            report_error(node, "Cannot return without value from non-void function");
                        }
                    }
                    else
                    {
                        unify(voidType, expectedReturnType, node, "void return statement");
                    }
                }
                return;
            }
            currentScopeNode = currentScopeNode->parent;
        }

        report_error(node, "Return statement not within a function");
    }

    void TypeResolver::visit(ForStmtSyntax *node)
    {
        // Visit children manually
        if (node->initializer)
            node->initializer->accept(this);
        if (node->condition)
            node->condition->accept(this);
        for (auto update : node->updates)
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
                unify(canonicalCondition, typeSystem.get_primitive_type("bool"), node, "for loop condition");
            }
        }
    }

    void TypeResolver::visit(WhileStmtSyntax *node)
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
            unify(canonicalCondition, typeSystem.get_primitive_type("bool"), node, "while loop condition");
        }
    }

    void TypeResolver::visit(FunctionDeclSyntax *node)
    {
        // Visit children manually
        if (node->name)
            node->name->accept(this);

        for (auto param : node->parameters)
        {
            if (param)
                param->accept(this);
        }

        if (node->returnType)
            node->returnType->accept(this);

        if (node->body)
            node->body->accept(this);

        // Look up the function symbol to work with its type
        auto scope = get_containing_scope(node);
        if (!scope)
            return;

        auto symbol = scope->lookup_local(node->name->get_name());
        FunctionSymbol *funcSymbol = nullptr;

        // Handle both direct FunctionSymbol and FunctionGroupSymbol
        if (auto directFunc = symbol ? symbol->as<FunctionSymbol>() : nullptr)
        {
            funcSymbol = directFunc;
        }
        else if (auto funcGroup = symbol ? symbol->as<FunctionGroupSymbol>() : nullptr)
        {
            // Find the exact overload that matches this declaration
            auto overloads = funcGroup->get_overloads();

            for (auto overload : overloads)
            {
                if (overload && overload->parameters().size() == node->parameters.size())
                {
                    // Check if all parameter types match exactly
                    bool exact_match = true;

                    for (size_t i = 0; i < node->parameters.size(); ++i)
                    {
                        auto paramDecl = node->parameters[i];
                        auto paramSymbol = overload->parameters()[i];

                        // Get the type from the declaration
                        TypePtr declType = nullptr;
                        if (paramDecl->param->type)
                        {
                            declType = resolve_expr_type(paramDecl->param->type, scope);
                        }

                        // Get the symbol's parameter type
                        TypePtr symbolType = apply_substitution(paramSymbol->type());

                        // For exact matching: both must be resolved and equal
                        if (declType && !declType->is<UnresolvedType>() &&
                            !symbolType->is<UnresolvedType>())
                        {
                            if (declType->get_name() != symbolType->get_name())
                            {
                                exact_match = false;
                                break;
                            }
                        }
                        // If either is unresolved, we can't determine exact match yet
                        // This overload might still be the right one
                    }

                    if (exact_match)
                    {
                        funcSymbol = overload;
                        break;
                    }
                }
            }
        }

        if (!funcSymbol)
            return;

        TypePtr returnType = funcSymbol->return_type();
        if (returnType && !returnType->is<UnresolvedType>())
        {
            symbolTable.mark_symbol_resolved(funcSymbol);
        }
        else
        {
            auto resolvedType = infer_function_return_type(node->body);
            if (resolvedType && !resolvedType->is<UnresolvedType>())
            {
                funcSymbol->set_return_type(resolvedType);
                symbolTable.mark_symbol_resolved(funcSymbol);
            }
        }

        // Update parameter types after all parameters have been resolved
        std::vector<TypePtr> resolvedParamTypes;
        for (auto paramDecl : node->parameters)
        {
            if (paramDecl && paramDecl->param && paramDecl->param->name)
            {
                std::string paramName(paramDecl->param->name->get_name());
                auto functionScope = static_cast<Scope *>(funcSymbol);
                auto paramSymbol = functionScope->lookup_local(paramName);
                if (auto typedParamSymbol = paramSymbol ? paramSymbol->as<TypedSymbol>() : nullptr)
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

    void TypeResolver::visit(ParameterDeclSyntax *node)
    {
        // Visit children manually
        if (node->param)
            node->param->accept(this);
        if (node->defaultValue)
            node->defaultValue->accept(this);

        // Look up the parameter symbol in the function's scope
        // Parameters are defined in the function scope, not where the ParameterDeclSyntax node is annotated
        auto scope = get_containing_scope(node);
        if (!scope)
            return;

        // Parameters should be in the current scope (function scope)
        auto symbol = scope->lookup_local(node->param->name->get_name());
        if (!symbol)
        {
            // If not found locally, try looking in parent scope (might be in function scope)
            symbol = scope->lookup(node->param->name->get_name());
        }
        auto paramSymbol = symbol ? symbol->as<ParameterSymbol>() : nullptr;
        if (!paramSymbol)
        {
            return;
        }

        // Resolve explicit type annotations that were deferred during symbol building
        TypePtr paramType = paramSymbol->type();

        if (paramType && paramType->is<UnresolvedType>())
        {
            if (node->param->type)
            {
                // Resolve the type expression now that all types are in symbol table
                TypePtr resolvedType = resolve_expr_type(node->param->type, scope);

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

    void TypeResolver::visit(PropertyDeclSyntax *node)
    {
        // Handle property symbol resolution in the property's own scope context
        // Don't visit the nested VariableDeclSyntax - handle the property symbol directly

        auto scope = get_containing_scope(node);
        if (!scope)
            return;

        if (!node->variable || !node->variable->variable || !node->variable->variable->name)
            return;

        std::string prop_name(node->variable->variable->name->get_name());
        auto symbol = scope->lookup(prop_name);
        auto prop_symbol = symbol ? symbol->as<TypedSymbol>() : nullptr;

        if (prop_symbol)
        {
            TypePtr propType = prop_symbol->type();

            // Resolve explicit type annotations that were deferred during symbol building
            if (propType && propType->is<UnresolvedType>())
            {
                if (node->variable->variable->type)
                {
                    TypePtr resolvedType = resolve_expr_type(node->variable->variable->type, scope);

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
                if (auto expr = std::get_if<BaseExprSyntax *>(&node->getter->body))
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

    void TypeResolver::visit(PropertyAccessorSyntax *node)
    {
        // Visit the accessor body to resolve types in expressions
        if (auto expr = std::get_if<BaseExprSyntax *>(&node->body))
        {
            if (*expr)
                (*expr)->accept(this);
        }
        else if (auto block = std::get_if<BlockSyntax *>(&node->body))
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

    void TypeResolver::visit(PointerTypeSyntax *node)
    {
        // Visit the pointee type to ensure proper traversal
        if (node->baseType)
            node->baseType->accept(this);

        // PointerTypeSyntax nodes should always be resolved when visited
        // They only appear in type contexts, so we always resolve them
        auto scope = get_containing_scope(node);
        if (!scope)
            return;

        TypePtr resolvedType = resolve_expr_type(node, scope);
        annotate_expression(node, resolvedType);
    }

    void TypeResolver::visit(TypeParameterDeclSyntax *node)
    {
        // Type parameter declarations don't need type resolution
        // They are handled during generic type definition processing
        if (node->name)
            node->name->accept(this);
    }

    void TypeResolver::visit(TypeDeclSyntax *node)
    {
        // Save current type parameters
        auto savedTypeParameters = currentTypeParameters;

        // Register type parameters for this generic type
        int parameterId = 0;
        for (auto typeParam : node->typeParameters)
        {
            if (typeParam && typeParam->name)
            {
                std::string paramName(typeParam->name->get_name());
                TypePtr paramType = typeSystem.get_type_parameter(paramName, parameterId++);
                currentTypeParameters[paramName] = paramType;

                // Visit the type parameter declaration
                typeParam->accept(this);
            }
        }

        // Visit the type's members with type parameters in scope
        DefaultVisitor::visit(node);

        // Restore previous type parameters
        currentTypeParameters = savedTypeParameters;
    }

}