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

    // Helper: Get field index within a type
    size_t BoundToHLIR::get_field_index(TypeSymbol* type_sym, Symbol* field_sym) {
        if (!type_sym || !field_sym) return 0;

        // Iterate through member_order to find the index of this field
        size_t index = 0;
        for (auto* member : type_sym->member_order) {
            // Only count fields/variables (not functions, properties, etc.)
            if (member->is<FieldSymbol>() || member->is<VariableSymbol>()) {
                if (member == field_sym) {
                    return index;
                }
                index++;
            }
        }

        return 0; // Fallback
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

        // Check if this is a field member being accessed in a member function
        if (node->symbol->is<FieldSymbol>() || node->symbol->is<VariableSymbol>()) {
            auto parent = node->symbol->parent;
            if (parent && parent->is<TypeSymbol>()) {
                // This is a field of a type - we need 'this' pointer
                if (current_function && !current_function->is_static && current_function->params.size() > 0) {
                    // Get 'this' parameter (first parameter of member function)
                    auto this_param = current_function->params[0];

                    // Compute field address
                    if (auto field = node->symbol->as<FieldSymbol>()) {
                        size_t field_index = get_field_index(parent->as<TypeSymbol>(), field);
                        auto field_addr = builder.field_addr(this_param, field_index, field->type);
                        expression_values[node] = field_addr;
                        return;
                    } else if (auto var = node->symbol->as<VariableSymbol>()) {
                        size_t field_index = get_field_index(parent->as<TypeSymbol>(), var);
                        auto field_addr = builder.field_addr(this_param, field_index, var->type);
                        expression_values[node] = field_addr;
                        return;
                    }
                }
            }
        }

        // Otherwise, it's a regular local variable or parameter
        auto value = get_symbol_value(node->symbol);
        expression_values[node] = value;
    }
    
    void BoundToHLIR::visit(BoundBinaryExpression* node) {
        auto left = evaluate_expression(node->left);
        auto right = evaluate_expression(node->right);

        // check for null operands
        if (!left || !right) {
            std::cerr << "Error: Null operand in binary expression at "
                      << node->location.start.to_string() << "\n";
            expression_values[node] = nullptr;
            return;
        }
        
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
                        size_t field_index = get_field_index(field->parent->as<TypeSymbol>(), field);
                        auto addr_result = builder.field_addr(obj_val, field_index, field->type);
                        current_value = builder.load(addr_result, field->type);
                    }
                    else if (auto var = member->member->as<VariableSymbol>()) {
                        size_t field_index = get_field_index(var->parent->as<TypeSymbol>(), var);
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
                // Get the target address
                auto target_addr = get_symbol_value(name->symbol);

                // If both target and value are pointers to value types, we need to load from source and store to target
                if (target_addr && target_addr->type->as<PointerType>() &&
                    final_value && final_value->type->as<PointerType>()) {
                    auto target_pointee = target_addr->type->as<PointerType>()->pointee;
                    auto value_pointee = final_value->type->as<PointerType>()->pointee;

                    // If both point to the same value type, do a value copy
                    if (target_pointee->as<NamedType>() && value_pointee->as<NamedType>() &&
                        target_pointee == value_pointee) {
                        // Load from source, store to target
                        auto loaded_value = builder.load(final_value, value_pointee);
                        builder.store(loaded_value, target_addr);
                        expression_values[node] = loaded_value;
                        return;
                    }
                }

                // For primitive assignments, just store the value
                if (target_addr && target_addr->type->as<PointerType>()) {
                    builder.store(final_value, target_addr);
                } else {
                    set_symbol_value(name->symbol, final_value);
                }
            }
        }
        // Handle member field assignment
        else if (auto member = node->target->as<BoundMemberAccessExpression>()) {
            auto obj_val = evaluate_expression(member->object);
            if (obj_val && member->member) {
                // Field assignment
                if (auto field = member->member->as<FieldSymbol>()) {
                    size_t field_index = get_field_index(field->parent->as<TypeSymbol>(), field);
                    auto addr_result = builder.field_addr(obj_val, field_index, field->type);

                    // If field is a value type and we're assigning a pointer, load the value first
                    auto store_value = final_value;
                    if (field->type->as<NamedType>() && field->type->is_value_type() &&
                        final_value && final_value->type->as<PointerType>()) {
                        auto ptr_type = final_value->type->as<PointerType>();
                        if (ptr_type->pointee == field->type) {
                            store_value = builder.load(final_value, field->type);
                        }
                    }
                    builder.store(store_value, addr_result);
                }
                // Variable member assignment
                else if (auto var = member->member->as<VariableSymbol>()) {
                    size_t field_index = get_field_index(var->parent->as<TypeSymbol>(), var);
                    auto addr_result = builder.field_addr(obj_val, field_index, var->type);

                    // If field is a value type and we're assigning a pointer, load the value first
                    auto store_value = final_value;
                    if (var->type->as<NamedType>() && var->type->is_value_type() &&
                        final_value && final_value->type->as<PointerType>()) {
                        auto ptr_type = final_value->type->as<PointerType>();
                        if (ptr_type->pointee == var->type) {
                            store_value = builder.load(final_value, var->type);
                        }
                    }
                    builder.store(store_value, addr_result);
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
        // Handle array element assignment
        else if (auto index = node->target->as<BoundIndexExpression>()) {
            auto obj_val = evaluate_expression(index->object);
            auto index_val = evaluate_expression(index->index);

            if (obj_val && index_val) {
                // Get element type from array type
                TypePtr element_type = nullptr;
                if (auto array_type = index->object->type->as<ArrayType>()) {
                    element_type = array_type->element;
                } else if (auto ptr_type = index->object->type->as<PointerType>()) {
                    element_type = ptr_type->pointee;
                }

                if (element_type) {
                    // Generate element address and store
                    auto addr_result = builder.element_addr(obj_val, index_val, element_type);
                    builder.store(final_value, addr_result);
                }
            }
        }

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
            auto arg_val = evaluate_expression(arg);
            args.push_back(arg_val);
        }

        if (node->method && node->method->as<FunctionSymbol>()) {
            auto func_sym = static_cast<FunctionSymbol*>(node->method);
            HLIR::Function* func = module->find_function(func_sym);

            if (func) {
                // Convert arguments if needed: if parameter expects value but we have pointer, load it
                bool is_member_func = !func->is_static && func->params.size() > 0;
                size_t param_offset = is_member_func ? 1 : 0; // Offset to skip 'this' parameter

                for (size_t arg_idx = 0; arg_idx < node->arguments.size(); arg_idx++) {
                    size_t param_idx = arg_idx + param_offset;
                    size_t args_idx = arg_idx + (is_member_func ? 1 : 0); // args also has 'this' at index 0 for member functions

                    if (param_idx < func->params.size() && args_idx < args.size()) {
                        auto param_type = func->params[param_idx]->type;
                        auto& arg_val = args[args_idx];

                        // If parameter expects a value type but we're passing a pointer, load the value
                        if (param_type->as<NamedType>() && param_type->is_value_type() &&
                            arg_val && arg_val->type->as<PointerType>()) {
                            auto ptr_type = arg_val->type->as<PointerType>();
                            if (ptr_type->pointee == param_type) {
                                // Load the value from the pointer
                                arg_val = builder.load(arg_val, param_type);
                            }
                        }
                    }
                }

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
                    size_t field_index = get_field_index(field->parent->as<TypeSymbol>(), field);
                    auto addr_result = builder.field_addr(obj_val, field_index, field->type);

                    // For struct fields, return the pointer so nested access works
                    // Only load for primitive types
                    if (field->type->as<NamedType>() && field->type->is_value_type()) {
                        // Struct field - return pointer for chained access
                        result = addr_result;
                    } else {
                        // Primitive field - load the value
                        result = builder.load(addr_result, field->type);
                    }
                }
            }
            // Variable member (for compatibility)
            else if (auto var = node->member->as<VariableSymbol>()) {
                if (obj_val) {
                    size_t field_index = get_field_index(var->parent->as<TypeSymbol>(), var);
                    auto addr_result = builder.field_addr(obj_val, field_index, var->type);

                    // For struct fields, return the pointer so nested access works
                    // Only load for primitive types
                    if (var->type->as<NamedType>() && var->type->is_value_type()) {
                        // Struct field - return pointer for chained access
                        result = addr_result;
                    } else {
                        // Primitive field - load the value
                        result = builder.load(addr_result, var->type);
                    }
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
        // Use stack allocation for value types, heap for reference types
        bool use_stack = obj_type->is_value_type();
        auto alloc_result = builder.alloc(obj_type, use_stack);
        
        // Call constructor
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

                // Convert arguments if needed: if parameter expects value but we have pointer, load it
                // Constructor is a member function, so params[0] is 'this', params[1+] are actual args
                for (size_t arg_idx = 0; arg_idx < node->arguments.size(); arg_idx++) {
                    size_t param_idx = arg_idx + 1; // +1 to skip 'this' parameter
                    size_t args_idx = arg_idx + 1;  // +1 because args[0] is 'this'

                    if (param_idx < ctor_func->params.size() && args_idx < args.size()) {
                        auto param_type = ctor_func->params[param_idx]->type;
                        auto& arg_val = args[args_idx];

                        // If parameter expects a value type but we're passing a pointer, load the value
                        if (param_type->as<NamedType>() && param_type->is_value_type() &&
                            arg_val && arg_val->type->as<PointerType>()) {
                            auto ptr_type = arg_val->type->as<PointerType>();
                            if (ptr_type->pointee == param_type) {
                                // Load the value from the pointer
                                arg_val = builder.load(arg_val, param_type);
                            }
                        }
                    }
                }

                builder.call(ctor_func, args);
            }
        }
        
        expression_values[node] = alloc_result;
    }
    
    void BoundToHLIR::visit(BoundArrayCreationExpression* node) {
        // Allocate stack space for the array (pass true for stack allocation)
        auto alloc_result = builder.alloc(node->type, true);

        // Initialize array elements if initializers are provided
        if (!node->initializers.empty()) {
            // Get element type from array type
            TypePtr element_type = nullptr;
            if (auto array_type = node->type->as<ArrayType>()) {
                element_type = array_type->element;
            }

            // Get i32 type for array indices
            auto i32_type = type_system->get_primitive("i32");

            // Store each initializer value into the array
            for (size_t i = 0; i < node->initializers.size(); i++) {
                auto init_val = evaluate_expression(node->initializers[i]);
                if (init_val && element_type) {
                    // Create constant index with i32 type
                    auto index_val = builder.const_int(static_cast<int64_t>(i), i32_type);
                    // Get element address
                    auto elem_addr = builder.element_addr(alloc_result, index_val, element_type);
                    // Store the value
                    builder.store(init_val, elem_addr);
                }
            }
        }

        expression_values[node] = alloc_result;
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
        // Check if then_block has a terminator (current_block might have changed)
        bool then_terminated = then_block->terminator() != nullptr;
        if (!then_terminated) {
            builder.set_block(then_block);
            builder.br(merge_block);
        }

        // Else branch (if exists)
        bool else_terminated = false;
        if (node->elseStatement) {
            builder.set_block(else_block);
            current_block = else_block;
            node->elseStatement->accept(this);
            // Check if else_block has a terminator (current_block might have changed)
            else_terminated = else_block->terminator() != nullptr;
            if (!else_terminated) {
                builder.set_block(else_block);
                builder.br(merge_block);
            }
        }

        // Check if merge block is reachable
        bool merge_reachable = !then_terminated || (node->elseStatement && !else_terminated) || !node->elseStatement;

        if (merge_reachable) {
            // Merge block is reachable, continue with it
            builder.set_block(merge_block);
            current_block = merge_block;
        } else {
            // Both branches terminated - merge block is dead code
            // Remove it from the function's block list
            auto& blocks = current_function->blocks;
            blocks.erase(
                std::remove_if(blocks.begin(), blocks.end(),
                    [merge_block](const std::unique_ptr<BasicBlock>& blk) {
                        return blk.get() == merge_block;
                    }),
                blocks.end()
            );
            current_block = nullptr;
        }
    }
    
    void BoundToHLIR::visit(BoundWhileStatement* node) {
        auto entry_block = current_block;
        auto header = create_block("while.header");
        auto body = create_block("while.body");
        auto exit = create_block("while.exit");

        // Jump to header
        builder.br(header);
        builder.set_block(header);
        current_block = header;

        // Create phi helper to manage loop-carried variables
        LoopPhiHelper phi_helper(this, entry_block, header);
        phi_helper.create_phis();

        // Set up loop context
        LoopContext ctx;
        ctx.continue_target = header;
        ctx.break_target = exit;
        ctx.loop_entry_values = symbol_values;
        loop_stack.push(ctx);

        // Evaluate condition and branch
        auto cond = evaluate_expression(node->condition);
        builder.cond_br(cond, body, exit);

        // Process loop body
        builder.set_block(body);
        current_block = body;
        node->body->accept(this);

        auto body_end_block = current_block;
        if (body_end_block && !body_end_block->terminator()) {
            builder.br(header);
        }

        // Add back-edge to phi nodes
        phi_helper.add_backedge(body_end_block);

        loop_stack.pop();
        builder.set_block(exit);
        current_block = exit;

        // Finalize phis - variables use phi results after loop
        phi_helper.finalize();
    }
    
    void BoundToHLIR::visit(BoundForStatement* node) {
        // Initialize
        if (node->initializer) {
            node->initializer->accept(this);
        }

        auto entry_block = current_block;
        auto header = create_block("for.header");
        auto body = create_block("for.body");
        auto update = create_block("for.update");
        auto exit = create_block("for.exit");

        builder.br(header);
        builder.set_block(header);
        current_block = header;

        // Create phi helper to manage loop-carried variables
        LoopPhiHelper phi_helper(this, entry_block, header);
        phi_helper.create_phis();

        // Set up loop context
        LoopContext ctx;
        ctx.continue_target = update;
        ctx.break_target = exit;
        ctx.loop_entry_values = symbol_values;
        loop_stack.push(ctx);

        // Evaluate condition and branch
        if (node->condition) {
            auto cond = evaluate_expression(node->condition);
            builder.cond_br(cond, body, exit);
        } else {
            builder.br(body);
        }

        // Process body
        builder.set_block(body);
        current_block = body;
        node->body->accept(this);
        if (!current_block->terminator()) {
            builder.br(update);
        }

        // Update block - increment variables and loop back
        builder.set_block(update);
        current_block = update;
        for (auto inc : node->incrementors) {
            evaluate_expression(inc);
        }
        builder.br(header);

        // Add back-edge to phi nodes from update block
        phi_helper.add_backedge(update);

        loop_stack.pop();
        builder.set_block(exit);
        current_block = exit;

        // Finalize phis - variables use phi results after loop
        phi_helper.finalize();
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

            // If returning a value type but we have a pointer, load the value
            if (current_function && current_function->return_type()) {
                auto return_type = current_function->return_type();
                if (return_type->as<NamedType>() && return_type->is_value_type() &&
                    val && val->type->as<PointerType>()) {
                    auto ptr_type = val->type->as<PointerType>();
                    if (ptr_type->pointee == return_type) {
                        // Load the value from the pointer before returning
                        val = builder.load(val, return_type);
                    }
                }
            }

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

        auto var_sym = node->symbol->as<VariableSymbol>();
        auto var_type = var_sym->type;

        // For value types (structs), allocate on stack
        if (var_type->as<NamedType>() && var_type->is_value_type()) {
            // Allocate stack space for the variable
            auto var_addr = builder.alloc(var_type, true);

            // If there's an initializer, evaluate it and store
            if (node->initializer) {
                auto init_value = evaluate_expression(node->initializer);
                // If init_value is a pointer (from 'new' expression), load it first
                if (init_value && init_value->type->as<PointerType>()) {
                    // Load the struct value from the pointer
                    auto loaded_value = builder.load(init_value, var_type);
                    builder.store(loaded_value, var_addr);
                } else if (init_value) {
                    builder.store(init_value, var_addr);
                }
            }
            // else: uninitialized, just leave the allocated space

            // Store the address for later use
            set_symbol_value(node->symbol, var_addr);
        } else {
            // For primitive types and pointers, use the value directly
            HLIR::Value* init_value = nullptr;

            if (node->initializer) {
                init_value = evaluate_expression(node->initializer);
            } else {
                // Default initialize
                init_value = builder.const_null(var_type);
            }

            if (node->symbol) {
                set_symbol_value(node->symbol, init_value);
            }
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
            auto param_sym = node->parameters[i]->symbol->as<ParameterSymbol>();
            auto param_type = param_sym->type;

            // For value types (structs), we need to allocate stack space and store the parameter there
            // so that we can take their address for member access
            if (param_type->as<NamedType>() && param_type->is_value_type()) {
                // Create parameter as value
                auto param = func->create_value(param_type, node->parameters[i]->name);
                func->params.push_back(param);

                // Allocate stack space for the parameter
                auto param_addr = builder.alloc(param_type, true);
                // Store the parameter value into the stack allocation
                builder.store(param, param_addr);

                // Use the stack address for all subsequent accesses
                set_symbol_value(param_sym, param_addr);
            } else {
                // For primitive types and pointers, use the parameter directly
                auto param = func->create_value(param_type, node->parameters[i]->name);
                func->params.push_back(param);
                set_symbol_value(param_sym, param);
            }
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
