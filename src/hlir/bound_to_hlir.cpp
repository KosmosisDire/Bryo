// hlir_builder.cpp
#include "bound_to_hlir.hpp"
#include <cassert>
#include <iostream>

namespace Bryo::HLIR
{
    #pragma region Public Methods
    
    void BoundToHLIR::build(BoundCompilationUnit* unit) {
        visit(unit);
        resolve_pending_phis();
    }
    
    #pragma region Core Expressions
    
    void BoundToHLIR::visit(BoundLiteralExpression* node) {
        HLIR::Value* result = nullptr;
        
        if (std::holds_alternative<int64_t>(node->constantValue)) {
            auto val = std::get<int64_t>(node->constantValue);
            result = builder.const_int(val, node->type);
        }
        else if (std::holds_alternative<bool>(node->constantValue)) {
            auto val = std::get<bool>(node->constantValue);
            result = builder.const_bool(val, node->type);
        }
        else if (std::holds_alternative<double>(node->constantValue)) {
            auto val = std::get<double>(node->constantValue);
            result = builder.const_float(val, node->type);
        }
        else if (std::holds_alternative<std::string>(node->constantValue)) {
            auto& val = std::get<std::string>(node->constantValue);
            result = builder.const_string(val, node->type);
        }
        
        expression_values[node] = result;
    }
    
    void BoundToHLIR::visit(BoundNameExpression* node) {
        if (!node->symbol) return;
        
        auto value = get_symbol_value(node->symbol);
        expression_values[node] = value;
    }
    
    void BoundToHLIR::visit(BoundBinaryExpression* node) {
        auto left = evaluate_expression(node->left);
        auto right = evaluate_expression(node->right);
        
        auto opcode = get_binary_opcode(node->operatorKind);
        auto result = builder.binary(opcode, left, right);
        
        expression_values[node] = result;
    }
    
    void BoundToHLIR::visit(BoundUnaryExpression* node) {
        auto operand = evaluate_expression(node->operand);
        
        auto opcode = get_unary_opcode(node->operatorKind);
        auto result = builder.unary(opcode, operand);
        
        expression_values[node] = result;
    }
    
    void BoundToHLIR::visit(BoundAssignmentExpression* node) {
        auto rhs_value = evaluate_expression(node->value);

        // Desugar compound assignments during lowering (e.g., x += 1 becomes x = x + 1)
        HLIR::Value* final_value = rhs_value;
        if (node->operatorKind != AssignmentOperatorKind::Assign) {
            // Load the current value of the target
            HLIR::Value* current_value = nullptr;

            if (auto name = node->target->as<BoundNameExpression>()) {
                current_value = get_symbol_value(name->symbol);
            }
            else if (auto member = node->target->as<BoundMemberAccessExpression>()) {
                auto obj_val = evaluate_expression(member->object);
                if (obj_val && member->member) {
                    if (auto field = member->member->as<FieldSymbol>()) {
                        size_t field_index = 0;
                        auto addr_result = builder.field_addr(obj_val, field_index, field->type);
                        current_value = builder.load(addr_result, field->type);
                    }
                    else if (auto var = member->member->as<VariableSymbol>()) {
                        size_t field_index = 0;
                        auto addr_result = builder.field_addr(obj_val, field_index, var->type);
                        current_value = builder.load(addr_result, var->type);
                    }
                }
            }

            // Perform the compound operation
            if (current_value) {
                HLIR::Opcode opcode;
                switch (node->operatorKind) {
                    case AssignmentOperatorKind::Add: opcode = HLIR::Opcode::Add; break;
                    case AssignmentOperatorKind::Subtract: opcode = HLIR::Opcode::Sub; break;
                    case AssignmentOperatorKind::Multiply: opcode = HLIR::Opcode::Mul; break;
                    case AssignmentOperatorKind::Divide: opcode = HLIR::Opcode::Div; break;
                    case AssignmentOperatorKind::Modulo: opcode = HLIR::Opcode::Rem; break;
                    case AssignmentOperatorKind::And: opcode = HLIR::Opcode::BitAnd; break;
                    case AssignmentOperatorKind::Or: opcode = HLIR::Opcode::BitOr; break;
                    case AssignmentOperatorKind::Xor: opcode = HLIR::Opcode::BitXor; break;
                    case AssignmentOperatorKind::LeftShift: opcode = HLIR::Opcode::Shl; break;
                    case AssignmentOperatorKind::RightShift: opcode = HLIR::Opcode::Shr; break;
                    default:
                        std::cerr << "Warning: Unsupported compound assignment operator\n";
                        opcode = HLIR::Opcode::Add;
                        break;
                }

                final_value = builder.binary(opcode, current_value, rhs_value);
            }
        }

        // Now perform the actual assignment
        // Handle simple name assignment
        if (auto name = node->target->as<BoundNameExpression>()) {
            if (name->symbol) {
                set_symbol_value(name->symbol, final_value);
            }
        }
        // Handle member field assignment
        else if (auto member = node->target->as<BoundMemberAccessExpression>()) {
            auto obj_val = evaluate_expression(member->object);
            if (obj_val && member->member) {
                // Field assignment
                if (auto field = member->member->as<FieldSymbol>()) {
                    size_t field_index = 0;
                    if (field->parent && field->parent->is<TypeSymbol>()) {
                        // TODO: Proper field indexing - for now use 0
                        field_index = 0;
                    }

                    // Generate field address instruction
                    auto addr_result = builder.field_addr(obj_val, field_index, field->type);
                    builder.store(final_value, addr_result);
                }
                // Variable member assignment
                else if (auto var = member->member->as<VariableSymbol>()) {
                    size_t field_index = 0;
                    if (var->parent && var->parent->is<TypeSymbol>()) {
                        // TODO: Proper field indexing - for now use 0
                        field_index = 0;
                    }

                    // Generate field address instruction
                    auto addr_result = builder.field_addr(obj_val, field_index, var->type);
                    builder.store(final_value, addr_result);
                }
                // Property setter
                else if (auto prop = member->member->as<PropertySymbol>()) {
                    if (prop->has_setter) {
                        // For now, properties can't be assigned until we generate setter functions
                        // TODO: Generate setter functions in HLIR and call them here
                        std::cerr << "Warning: Property setters not yet implemented in HLIR\n";
                    }
                }
            }
        }
        // TODO: Handle index expressions

        expression_values[node] = final_value;
    }
    
    void BoundToHLIR::visit(BoundCallExpression* node) {
        std::vector<HLIR::Value*> args;
        
        // Check if this is a method call through member access
        if (auto member_expr = node->callee->as<BoundMemberAccessExpression>()) {
            // Evaluate the object to get 'this'
            auto this_val = evaluate_expression(member_expr->object);
            if (this_val) {
                args.push_back(this_val);
            }
        }
        
        // Add regular arguments
        for (auto arg : node->arguments) {
            args.push_back(evaluate_expression(arg));
        }
        
        if (node->method && node->method->as<FunctionSymbol>()) {
            auto func_sym = static_cast<FunctionSymbol*>(node->method);
            HLIR::Function* func = module->find_function(func_sym);
            
            if (func) {
                auto result = builder.call(func, args);
                expression_values[node] = result;
            }
        }
    }
    
    #pragma region Stub Expressions
    
    void BoundToHLIR::visit(BoundMemberAccessExpression* node) {
        // Evaluate the object
        HLIR::Value* obj_val = nullptr;
        if (node->object) {
            obj_val = evaluate_expression(node->object);
        }

        HLIR::Value* result = nullptr;

        if (node->member) {
            // Field access
            if (auto field = node->member->as<FieldSymbol>()) {
                if (obj_val) {
                    // Get the field offset/index
                    size_t field_index = 0;
                    if (field->parent && field->parent->is<TypeSymbol>()) {
                        auto type_sym = field->parent->as<TypeSymbol>();
                        // Find field index by searching members
                        auto field_members = type_sym->get_member(field->name);
                        // Count fields before this one
                        // TODO: Proper field indexing - for now use 0
                        field_index = 0;
                    }
                    
                    // Generate field address instruction
                    auto addr_result = builder.field_addr(obj_val, field_index, field->type);
                    // Load the field value
                    result = builder.load(addr_result, field->type);
                }
            }
            // Variable member (for compatibility)
            else if (auto var = node->member->as<VariableSymbol>()) {
                if (obj_val) {
                    // Treat as field access
                    size_t field_index = 0;
                    if (var->parent && var->parent->is<TypeSymbol>()) {
                        auto type_sym = var->parent->as<TypeSymbol>();
                        // Find field index by searching members
                        auto var_members = type_sym->get_member(var->name);
                        // TODO: Proper field indexing - for now use 0
                        field_index = 0;
                    }
                    
                    // Generate field address instruction
                    auto addr_result = builder.field_addr(obj_val, field_index, var->type);
                    result = builder.load(addr_result, var->type);
                } else {
                    // Static member or error
                    result = get_symbol_value(var);
                }
            }
            // Property access via getter
            else if (auto prop = node->member->as<PropertySymbol>()) {
                if (prop->has_getter) {
                    // For now, properties can't be accessed until we generate getter functions
                    // TODO: Generate getter functions in HLIR and call them here
                    std::cerr << "Warning: Property getters not yet implemented in HLIR\n";
                    result = nullptr;
                }
            }
            // Method reference: no value to produce here, handled at call site
            else if (node->member->as<FunctionSymbol>()) {
                // Store the object for potential method call
                expression_values[node] = obj_val;
                return;
            }
        }

        expression_values[node] = result;
    }
    
    void BoundToHLIR::visit(BoundIndexExpression* node) {
        // Evaluate object and index
        auto obj_val = evaluate_expression(node->object);
        auto index_val = evaluate_expression(node->index);
        
        if (!obj_val || !index_val) {
            expression_values[node] = nullptr;
            return;
        }
        
        // Get element type from array type
        TypePtr element_type = nullptr;
        if (auto array_type = node->object->type->as<ArrayType>()) {
            element_type = array_type->element;
        } else if (auto ptr_type = node->object->type->as<PointerType>()) {
            // Pointer arithmetic - treat as array access
            element_type = ptr_type->pointee;
        } else {
            // Error: Cannot index this type
            expression_values[node] = nullptr;
            return;
        }
        
        // Generate element address and load
        auto addr_result = builder.element_addr(obj_val, index_val, element_type);
        auto result = builder.load(addr_result, element_type);
        
        expression_values[node] = result;
    }
    
    void BoundToHLIR::visit(BoundNewExpression* node) {
        // Get the type being instantiated
        TypePtr obj_type = node->type;
        if (!obj_type) {
            expression_values[node] = nullptr;
            return;
        }
        
        // Allocate memory for the object
        auto alloc_result = builder.alloc(obj_type);
        
        // Call constructor if available
        if (node->constructor) {
            auto ctor_func = module->find_function(node->constructor);
            if (ctor_func) {
                std::vector<HLIR::Value*> args;
                // First argument is 'this' pointer
                args.push_back(alloc_result);
                // Add constructor arguments
                for (auto arg : node->arguments) {
                    args.push_back(evaluate_expression(arg));
                }
                builder.call(ctor_func, args);
            }
        } else {
            // Default initialization
            // For named types with fields, initialize each field to default value
            if (auto named_type = obj_type->as<NamedType>()) {
                if (auto type_sym = named_type->symbol) {
                    // TODO: Iterate through fields and initialize them
                    // For now, just leave uninitialized
                }
            }
        }
        
        expression_values[node] = alloc_result;
    }
    
    void BoundToHLIR::visit(BoundArrayCreationExpression* node) {
        // TODO: Implement array creation
        expression_values[node] = nullptr;
    }
    
    void BoundToHLIR::visit(BoundCastExpression* node) {
        auto expr = evaluate_expression(node->expression);
        // TODO: Implement proper cast based on conversion kind
        auto result = builder.cast(expr, node->type);
        expression_values[node] = result;
    }
    
    void BoundToHLIR::visit(BoundConditionalExpression* node) {
        // TODO: Implement ternary operator with phi nodes
        expression_values[node] = nullptr;
    }
    
    void BoundToHLIR::visit(BoundThisExpression* node) {
        // Check if we're in a member function context
        if (!current_function) {
            throw std::runtime_error("'this' used outside of function context");
        }
        
        // For non-static member functions, 'this' is always the first parameter
        if (!current_function->is_static && current_function->params.size() > 0) {
            expression_values[node] = current_function->params[0];
        } else {
            throw std::runtime_error("'this' used outside of member function context");
        }
    }
    
    void BoundToHLIR::visit(BoundTypeOfExpression* node) {
        // TODO: Implement typeof
        expression_values[node] = nullptr;
    }
    
    void BoundToHLIR::visit(BoundSizeOfExpression* node) {
        // TODO: Implement sizeof
        expression_values[node] = nullptr;
    }
    
    void BoundToHLIR::visit(BoundParenthesizedExpression* node) {
        auto inner = evaluate_expression(node->expression);
        expression_values[node] = inner;
    }
    
    void BoundToHLIR::visit(BoundConversionExpression* node) {
        auto expr = evaluate_expression(node->expression);
        // TODO: Handle different conversion kinds
        expression_values[node] = expr;
    }
    
    void BoundToHLIR::visit(BoundTypeExpression* node) {
        // TODO: Type expressions don't produce runtime values
        expression_values[node] = nullptr;
    }
    
    #pragma region Core Statements
    
    void BoundToHLIR::visit(BoundBlockStatement* node) {
        for (auto stmt : node->statements) {
            stmt->accept(this);
        }
    }
    
    void BoundToHLIR::visit(BoundExpressionStatement* node) {
        evaluate_expression(node->expression);
    }
    
    void BoundToHLIR::visit(BoundIfStatement* node) {
        auto cond = evaluate_expression(node->condition);
        
        auto then_block = create_block("if.then");
        auto merge_block = create_block("if.merge");
        auto else_block = node->elseStatement 
            ? create_block("if.else") 
            : merge_block;
        
        builder.cond_br(cond, then_block, else_block);
        
        // Then branch
        builder.set_block(then_block);
        current_block = then_block;
        node->thenStatement->accept(this);
        if (!current_block->terminator()) {
            builder.br(merge_block);
        }
        
        // Else branch (if exists)
        if (node->elseStatement) {
            builder.set_block(else_block);
            current_block = else_block;
            node->elseStatement->accept(this);
            if (!current_block->terminator()) {
                builder.br(merge_block);
            }
        }
        
        // Continue with merge
        builder.set_block(merge_block);
        current_block = merge_block;
    }
    
    void BoundToHLIR::visit(BoundWhileStatement* node) {
        auto header = create_block("while.header");
        auto body = create_block("while.body");
        auto exit = create_block("while.exit");
        
        // Jump to header
        builder.br(header);
        builder.set_block(header);
        current_block = header;
        
        // Set up loop context
        LoopContext ctx;
        ctx.continue_target = header;
        ctx.break_target = exit;
        ctx.loop_entry_values = symbol_values;
        loop_stack.push(ctx);
        
        // Check condition
        auto cond = evaluate_expression(node->condition);
        builder.cond_br(cond, body, exit);
        
        // Body
        builder.set_block(body);
        current_block = body;
        node->body->accept(this);
        if (!current_block->terminator()) {
            builder.br(header);
        }
        
        loop_stack.pop();
        builder.set_block(exit);
        current_block = exit;
    }
    
    void BoundToHLIR::visit(BoundForStatement* node) {
        // Initialize
        if (node->initializer) {
            node->initializer->accept(this);
        }
        
        auto header = create_block("for.header");
        auto body = create_block("for.body");
        auto update = create_block("for.update");
        auto exit = create_block("for.exit");
        
        builder.br(header);
        builder.set_block(header);
        current_block = header;
        
        // Set up loop context
        LoopContext ctx;
        ctx.continue_target = update;
        ctx.break_target = exit;
        ctx.loop_entry_values = symbol_values;
        loop_stack.push(ctx);
        
        // Condition
        if (node->condition) {
            auto cond = evaluate_expression(node->condition);
            builder.cond_br(cond, body, exit);
        } else {
            builder.br(body);
        }
        
        // Body
        builder.set_block(body);
        current_block = body;
        node->body->accept(this);
        if (!current_block->terminator()) {
            builder.br(update);
        }
        
        // Update
        builder.set_block(update);
        current_block = update;
        for (auto inc : node->incrementors) {
            evaluate_expression(inc);
        }
        builder.br(header);
        
        loop_stack.pop();
        builder.set_block(exit);
        current_block = exit;
    }
    
    void BoundToHLIR::visit(BoundBreakStatement* node) {
        if (!loop_stack.empty()) {
            builder.br(loop_stack.top().break_target);
        }
    }
    
    void BoundToHLIR::visit(BoundContinueStatement* node) {
        if (!loop_stack.empty()) {
            builder.br(loop_stack.top().continue_target);
        }
    }
    
    void BoundToHLIR::visit(BoundReturnStatement* node) {
        if (node->value) {
            auto val = evaluate_expression(node->value);
            builder.ret(val);
        } else {
            builder.ret(nullptr);
        }
    }
    
    void BoundToHLIR::visit(BoundUsingStatement* node) {
        // TODO: Using statements don't generate code
    }
    
    #pragma region Declarations
    
    void BoundToHLIR::visit(BoundVariableDeclaration* node) {
        // Skip member variables - they're handled by type declarations
        if (node->symbol && node->symbol->is<FieldSymbol>()) {
            return;
        }
        
        HLIR::Value* init_value = nullptr;
        
        if (node->initializer) {
            init_value = evaluate_expression(node->initializer);
        } else {
            // Default initialize
            init_value = builder.const_null(node->symbol->as<VariableSymbol>()->type);
        }
        
        if (node->symbol) {
            set_symbol_value(node->symbol, init_value);
        }
    }
    
    void BoundToHLIR::visit(BoundFunctionDeclaration* node) {
        auto func_sym = node->symbol->as<FunctionSymbol>();
        if (!func_sym) return;
        
        // Look up the pre-created function
        auto func = module->find_function(func_sym);
        if (!func) return; // Function should already exist
        
        current_function = func;
        auto entry = func->create_block("entry");
        func->entry = entry;
        current_block = entry;  // Set current_block
        builder.set_function(func);
        builder.set_block(entry);
        
        // Check if this is a member function (has a parent TypeSymbol)
        bool is_member_function = func_sym->parent && func_sym->parent->is<TypeSymbol>();
        
        // Add implicit 'this' parameter for member functions
        if (is_member_function && !func_sym->isStatic) {
            auto parent_type = func_sym->parent->as<TypeSymbol>();
            // 'this' should be a pointer to the parent type
            auto this_ptr_type = type_system->get_pointer(parent_type->type);
            auto this_param = func->create_value(
                this_ptr_type,
                "this"
            );
            func->params.push_back(this_param);
        }
        
        // Create parameter values
        for (size_t i = 0; i < node->parameters.size(); i++) {
            auto param = func->create_value(
                node->parameters[i]->symbol->as<ParameterSymbol>()->type,
                node->parameters[i]->name
            );
            func->params.push_back(param);
            set_symbol_value(node->parameters[i]->symbol, param);
        }
        
        // Process body
        if (node->body) {
            node->body->accept(this);
        }
        
        // Add implicit return if needed
        if (current_block && !current_block->terminator()) {
            builder.ret(nullptr);
        }
        
        current_function = nullptr;
        current_block = nullptr;  // Clear current_block
    }
    
    void BoundToHLIR::visit(BoundPropertyDeclaration* node) {
        auto prop_sym = node->symbol->as<PropertySymbol>();
        if (!prop_sym) return;
        
        // Generate getter function if present
        if (node->getter && prop_sym->has_getter) {
            generate_property_getter(node, node->getter);
        }
        
        // Generate setter function if present
        if (node->setter && prop_sym->has_setter) {
            generate_property_setter(node, node->setter);
        }
    }
    
    void BoundToHLIR::visit(BoundTypeDeclaration* node)
    {
        // Types are already defined in the module, just process member functions
        auto type_sym = node->symbol->as<TypeSymbol>();
        if (!type_sym) return;
        
        // Process member functions if any
        for (auto member : node->members) {
            member->accept(this);
        }
    }
    
    void BoundToHLIR::visit(BoundNamespaceDeclaration* node) {
        // Process namespace members
        for (auto member : node->members) {
            member->accept(this);
        }
    }
    
    void BoundToHLIR::visit(BoundCompilationUnit* node) {
        for (auto stmt : node->statements) {
            stmt->accept(this);
        }
    }
    
    #pragma region Helper Methods
    
    HLIR::Value* BoundToHLIR::evaluate_expression(BoundExpression* expr) {
        expr->accept(this);
        return expression_values[expr];
    }
    
    HLIR::Value* BoundToHLIR::get_symbol_value(Symbol* sym) {
        auto it = symbol_values.find(sym);
        if (it != symbol_values.end()) {
            return it->second;
        }
        // TODO: Handle undefined variable
        return nullptr;
    }
    
    void BoundToHLIR::set_symbol_value(Symbol* sym, HLIR::Value* val) {
        symbol_values[sym] = val;
    }
    
    HLIR::BasicBlock* BoundToHLIR::create_block(const std::string& name) {
        auto block = current_function->create_block(name);
        current_block = block;
        return block;
    }
    
    void BoundToHLIR::resolve_pending_phis() {
        // TODO: Complete phi nodes with proper values
        for (auto& pending : pending_phis) {
            // Find value for symbol at each predecessor
        }
        pending_phis.clear();
    }
    
    HLIR::Opcode BoundToHLIR::get_binary_opcode(BinaryOperatorKind kind) {
        switch (kind) {
            case BinaryOperatorKind::Add: return HLIR::Opcode::Add;
            case BinaryOperatorKind::Subtract: return HLIR::Opcode::Sub;
            case BinaryOperatorKind::Multiply: return HLIR::Opcode::Mul;
            case BinaryOperatorKind::Divide: return HLIR::Opcode::Div;
            case BinaryOperatorKind::Modulo: return HLIR::Opcode::Rem;
            case BinaryOperatorKind::Equals: return HLIR::Opcode::Eq;
            case BinaryOperatorKind::NotEquals: return HLIR::Opcode::Ne;
            case BinaryOperatorKind::LessThan: return HLIR::Opcode::Lt;
            case BinaryOperatorKind::LessThanOrEqual: return HLIR::Opcode::Le;
            case BinaryOperatorKind::GreaterThan: return HLIR::Opcode::Gt;
            case BinaryOperatorKind::GreaterThanOrEqual: return HLIR::Opcode::Ge;
            case BinaryOperatorKind::LogicalAnd: return HLIR::Opcode::And;
            case BinaryOperatorKind::LogicalOr: return HLIR::Opcode::Or;
            case BinaryOperatorKind::BitwiseAnd: return HLIR::Opcode::BitAnd;
            case BinaryOperatorKind::BitwiseOr: return HLIR::Opcode::BitOr;
            case BinaryOperatorKind::BitwiseXor: return HLIR::Opcode::BitXor;
            case BinaryOperatorKind::LeftShift: return HLIR::Opcode::Shl;
            case BinaryOperatorKind::RightShift: return HLIR::Opcode::Shr;
            default: return HLIR::Opcode::Add; // TODO: Error handling
        }
    }
    
    HLIR::Opcode BoundToHLIR::get_unary_opcode(UnaryOperatorKind kind) {
        switch (kind) {
            case UnaryOperatorKind::Minus: return HLIR::Opcode::Neg;
            case UnaryOperatorKind::Not: return HLIR::Opcode::Not;
            case UnaryOperatorKind::BitwiseNot: return HLIR::Opcode::BitNot;
            default: return HLIR::Opcode::Neg; // TODO: Handle inc/dec and others
        }
    }
    
    void BoundToHLIR::generate_property_getter(BoundPropertyDeclaration* prop_decl, BoundPropertyAccessor* getter) {
        auto prop_sym = prop_decl->symbol->as<PropertySymbol>();
        if (!prop_sym || !getter->function_symbol) return;
        
        std::cout << "INFO: Generating getter function: " << getter->function_symbol->get_qualified_name() << std::endl;
        
        // Find the HLIR function that was created from the function symbol
        auto getter_func = module->find_function(getter->function_symbol);
        if (!getter_func) {
            std::cerr << "ERROR: Could not find HLIR function for getter symbol" << std::endl;
            return;
        }
        
        // Add 'this' parameter for instance property
        if (prop_sym->parent && prop_sym->parent->is<TypeSymbol>()) {
            auto parent_type = prop_sym->parent->as<TypeSymbol>();
            // 'this' should be a pointer to the parent type
            auto this_ptr_type = type_system->get_pointer(parent_type->type);
            auto this_param = getter_func->create_value(this_ptr_type, "this");
            getter_func->params.push_back(this_param);
        }
        
        // Create function body
        auto entry_block = getter_func->create_block("entry");
        getter_func->entry = entry_block;
        
        // Set up builder context
        auto prev_function = current_function;
        auto prev_block = current_block;
        current_function = getter_func;
        current_block = entry_block;
        builder.set_function(getter_func);
        builder.set_block(entry_block);
        
        // Generate getter body
        if (getter->expression) {
            // Arrow property: => expression
            auto result = evaluate_expression(getter->expression);
            builder.ret(result);
        } else if (getter->body) {
            // Block property: { ... }
            getter->body->accept(this);
            // Add implicit return if no explicit return
            if (!current_block->terminator()) {
                builder.ret(nullptr);
            }
        } else {
            // Empty getter - return default value
            auto default_val = builder.const_null(prop_sym->type);
            builder.ret(default_val);
        }
        
        // Restore context
        current_function = prev_function;
        current_block = prev_block;
        if (prev_function) {
            builder.set_function(prev_function);
            if (prev_block) {
                builder.set_block(prev_block);
            }
        }
    }
    
    void BoundToHLIR::generate_property_setter(BoundPropertyDeclaration* prop_decl, BoundPropertyAccessor* setter) {
        auto prop_sym = prop_decl->symbol->as<PropertySymbol>();
        if (!prop_sym || !setter->function_symbol) return;
        
        std::cout << "INFO: Generating setter function: " << setter->function_symbol->get_qualified_name() << std::endl;
        
        // Find the HLIR function that was created from the function symbol
        auto setter_func = module->find_function(setter->function_symbol);
        if (!setter_func) {
            std::cerr << "ERROR: Could not find HLIR function for setter symbol" << std::endl;
            return;
        }
        
        // Add 'this' parameter for instance property
        if (prop_sym->parent && prop_sym->parent->is<TypeSymbol>()) {
            auto parent_type = prop_sym->parent->as<TypeSymbol>();
            // 'this' should be a pointer to the parent type
            auto this_ptr_type = type_system->get_pointer(parent_type->type);
            auto this_param = setter_func->create_value(this_ptr_type, "this");
            setter_func->params.push_back(this_param);
        }
        
        // Add 'value' parameter
        auto value_param = setter_func->create_value(prop_sym->type, "value");
        setter_func->params.push_back(value_param);
        
        // Create function body
        auto entry_block = setter_func->create_block("entry");
        setter_func->entry = entry_block;
        
        // Set up builder context
        auto prev_function = current_function;
        auto prev_block = current_block;
        current_function = setter_func;
        current_block = entry_block;
        builder.set_function(setter_func);
        builder.set_block(entry_block);
        
        // Generate setter body
        if (setter->expression) {
            // Arrow setter: => expression (assign to expression)
            evaluate_expression(setter->expression);
        } else if (setter->body) {
            // Block setter: { ... }
            setter->body->accept(this);
        }
        
        // Add implicit void return if no explicit return
        if (!current_block->terminator()) {
            builder.ret(nullptr);
        }
        
        // Restore context
        current_function = prev_function;
        current_block = prev_block;
        if (prev_function) {
            builder.set_function(prev_function);
            if (prev_block) {
                builder.set_block(prev_block);
            }
        }
    }
}
