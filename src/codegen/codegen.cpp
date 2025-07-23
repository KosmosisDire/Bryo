#include "codegen/codegen.hpp"
#include "ast/ast_rtti.hpp"
#include "common/logger.hpp"
#include <iostream>

namespace Mycelium::Scripting::Lang {


CodeGenerator::CodeGenerator(SymbolTable& table) 
    : symbol_table_(table), current_value_(ValueRef::invalid()) {
}



void CodeGenerator::visit(CompilationUnitNode* node) {
    if (!node) return;
    
    LOG_DEBUG("CompilationUnitNode: Found " + std::to_string(node->statements.size) + " statements", LogCategory::CODEGEN);
    
    // Visit all statements in the compilation unit
    for (int i = 0; i < node->statements.size; ++i) {
        auto* stmt = node->statements[i];
        
        LOG_DEBUG("Processing statement " + std::to_string(i) + " of type: " + get_node_type_name(stmt) + " (ID " + std::to_string((int)stmt->typeId) + ")", LogCategory::CODEGEN);
        
        // Process function declarations and type declarations
        if (stmt->is_a<FunctionDeclarationNode>()) {
            LOG_DEBUG("Found FunctionDeclarationNode at index " + std::to_string(i), LogCategory::CODEGEN);
            stmt->accept(this);
        } else if (stmt->is_a<TypeDeclarationNode>()) {
            LOG_DEBUG("Found TypeDeclarationNode at index " + std::to_string(i), LogCategory::CODEGEN);
            stmt->accept(this);
        } else {
            LOG_DEBUG("Processing other statement type at index " + std::to_string(i), LogCategory::CODEGEN);
            stmt->accept(this);
        }
    }
}

void CodeGenerator::visit(LiteralExpressionNode* node) {
    if (!node || !ir_builder_ || !node->token) return;
    
    // Handle different literal types
    switch (node->kind) {
        case LiteralKind::Integer: {
            std::string token_text = std::string(node->token->text);
            int32_t value = std::stoi(token_text);
            current_value_ = ir_builder_->const_i32(value);
            break;
        }
        case LiteralKind::Boolean: {
            std::string token_text = std::string(node->token->text);
            bool value = (token_text == "true");
            current_value_ = ir_builder_->const_bool(value);
            break;
        }
        default:
            std::cerr << "Unsupported literal type" << std::endl;
            current_value_ = ValueRef::invalid();
            break;
    }
}

void CodeGenerator::visit(BinaryExpressionNode* node) {
    if (!node || !ir_builder_) return;
    
    // Visit left operand
    node->left->accept(this);
    ValueRef lhs = current_value_;
    
    // Visit right operand
    node->right->accept(this);
    ValueRef rhs = current_value_;
    
    // Generate the binary operation
    switch (node->opKind) {
        case BinaryOperatorKind::Add:
            current_value_ = ir_builder_->add(lhs, rhs);
            break;
        case BinaryOperatorKind::Subtract:
            current_value_ = ir_builder_->sub(lhs, rhs);
            break;
        case BinaryOperatorKind::Multiply:
            current_value_ = ir_builder_->mul(lhs, rhs);
            break;
        case BinaryOperatorKind::Divide:
            current_value_ = ir_builder_->div(lhs, rhs);
            break;
        case BinaryOperatorKind::LessThan:
            current_value_ = ir_builder_->icmp(ICmpPredicate::Slt, lhs, rhs);
            break;
        case BinaryOperatorKind::LessThanOrEqual:
            current_value_ = ir_builder_->icmp(ICmpPredicate::Sle, lhs, rhs);
            break;
        case BinaryOperatorKind::GreaterThan:
            current_value_ = ir_builder_->icmp(ICmpPredicate::Sgt, lhs, rhs);
            break;
        case BinaryOperatorKind::GreaterThanOrEqual:
            current_value_ = ir_builder_->icmp(ICmpPredicate::Sge, lhs, rhs);
            break;
        case BinaryOperatorKind::Equals:
            current_value_ = ir_builder_->icmp(ICmpPredicate::Eq, lhs, rhs);
            break;
        case BinaryOperatorKind::NotEquals:
            current_value_ = ir_builder_->icmp(ICmpPredicate::Ne, lhs, rhs);
            break;
        case BinaryOperatorKind::LogicalAnd:
            // For logical AND, we can use bitwise AND on booleans
            // TODO: Implement proper short-circuit evaluation
            current_value_ = ir_builder_->logical_and(lhs, rhs);
            break;
        case BinaryOperatorKind::LogicalOr:
            // For logical OR, we can use bitwise OR on booleans
            // TODO: Implement proper short-circuit evaluation
            current_value_ = ir_builder_->logical_or(lhs, rhs);
            break;
        default:
            std::cerr << "Unsupported binary operation: " << static_cast<int>(node->opKind) << std::endl;
            current_value_ = ValueRef::invalid();
            break;
    }
}

void CodeGenerator::visit(UnaryExpressionNode* node) {
    if (!node || !ir_builder_) return;
    
    // Visit the operand
    node->operand->accept(this);
    ValueRef operand = current_value_;
    
    // Generate the unary operation
    switch (node->opKind) {
        case UnaryOperatorKind::Minus:
            // Unary minus: 0 - operand
            current_value_ = ir_builder_->sub(ir_builder_->const_i32(0), operand);
            break;
        case UnaryOperatorKind::Plus:
            // Unary plus: just return the operand
            current_value_ = operand;
            break;
        case UnaryOperatorKind::Not:
            // Logical NOT: for boolean operands
            current_value_ = ir_builder_->logical_not(operand);
            break;
        default:
            std::cerr << "Unsupported unary operation: " << static_cast<int>(node->opKind) << std::endl;
            current_value_ = ValueRef::invalid();
            break;
    }
}

void CodeGenerator::visit(VariableDeclarationNode* node) {
    if (!node || !ir_builder_) return;
    
    // Generate initializer value once if present
    ValueRef init_value = ValueRef::invalid();
    if (node->initializer) {
        node->initializer->accept(this);
        init_value = current_value_;
    }
    
    // Process all variable names in this declaration
    for (int i = 0; i < node->names.size; ++i) {
        auto* name_node = node->names[i];
        if (!name_node) continue;
        
        // Get the variable's type - prefer initializer type over symbol table
        std::string var_name = std::string(name_node->name);
        IRType value_type = IRType::i32(); // Default fallback
        
        if (init_value.is_valid()) {
            // Use the actual type of the initializer expression
            value_type = init_value.type;
        } else {
            // Fall back to symbol table type if no initializer
            auto symbol = symbol_table_.lookup_symbol(var_name);
            if (symbol && symbol->type == SymbolType::VARIABLE) {
                value_type = symbol->data_type;
            }
        }
        
        // Allocate space for the variable with correct type
        ValueRef alloca_ref = ir_builder_->alloca(value_type);
        
        // Store the variable and its type in our local variables map
        local_vars_[var_name] = {alloca_ref, value_type};
        
        // If there's an initializer, store the value to this variable
        if (init_value.is_valid()) {
            ir_builder_->store(init_value, alloca_ref);
        }
    }
}

void CodeGenerator::visit(IdentifierExpressionNode* node) {
    if (!node || !ir_builder_ || !node->identifier) return;
    
    // Look up the variable in our local variables
    std::string var_name = std::string(node->identifier->name);
    auto it = local_vars_.find(var_name);
    if (it != local_vars_.end()) {
        // Use the stored type information
        const VariableInfo& var_info = it->second;
        
        // Load the value from the variable with correct type
        current_value_ = ir_builder_->load(var_info.value_ref, var_info.type);
        return;
    }
    
    // Check if we're in a member function and the identifier refers to a field
    std::string current_scope = symbol_table_.get_current_scope_name();
    if (current_scope.find("::") != std::string::npos) {
        // We're in a member function scope (Type::function)
        std::string type_name = current_scope.substr(0, current_scope.find("::"));
        
        // Check if this identifier is a field of the current type
        int type_scope_id = symbol_table_.find_scope_by_name(type_name);
        if (type_scope_id != -1) {
            auto field_symbol = symbol_table_.lookup_symbol_in_scope(type_scope_id, var_name);
            if (field_symbol && field_symbol->type == SymbolType::VARIABLE) {
                // This is an unqualified field access - generate `this.field`
                LOG_DEBUG("Generating unqualified field access for: " + var_name, LogCategory::CODEGEN);
                
                // Get the 'this' pointer from local variables
                auto this_it = local_vars_.find("this");
                if (this_it == local_vars_.end()) {
                    std::cerr << "Error: Cannot access field '" << var_name << "' - 'this' pointer not found" << std::endl;
                    current_value_ = ValueRef::invalid();
                    return;
                }
                
                ValueRef this_ptr = this_it->second.value_ref;
                
                // Generate member access: this.field
                auto struct_layout = build_struct_layout(type_name);
                if (!struct_layout) {
                    std::cerr << "Error: Could not build struct layout for unqualified field access" << std::endl;
                    current_value_ = ValueRef::invalid();
                    return;
                }
                
                // Find the field index
                int field_index = find_field_index(*struct_layout, var_name);
                if (field_index == -1) {
                    std::cerr << "Error: Field '" << var_name << "' not found in struct layout" << std::endl;
                    current_value_ = ValueRef::invalid();
                    return;
                }
                
                // Load 'this' pointer
                ValueRef this_value = ir_builder_->load(this_ptr, this_it->second.type);
                
                // Generate GEP instruction to get field address
                std::vector<int> indices = {field_index};
                ValueRef field_ptr = ir_builder_->gep(this_value, indices, IRType::ptr_to(field_symbol->data_type));
                
                // Load the field value
                current_value_ = ir_builder_->load(field_ptr, field_symbol->data_type);
                return;
            }
        }
    }
    
    std::cerr << "Unknown variable: " << var_name << std::endl;
    current_value_ = ValueRef::invalid();
}

void CodeGenerator::visit(ReturnStatementNode* node) {
    if (!node || !ir_builder_) return;
    
    if (node->expression) {
        // Generate code for the return expression
        node->expression->accept(this);
        if (current_value_.is_valid()) {
            ir_builder_->ret(current_value_);
        }
    } else {
        // Void return
        ir_builder_->ret_void();
    }
}

void CodeGenerator::visit(FunctionDeclarationNode* node) {
    if (!node || !ir_builder_) {
        LOG_ERROR("FunctionDeclarationNode: null node or null builder", LogCategory::CODEGEN);
        return;
    }
    
    if (!node->name) {
        LOG_ERROR("FunctionDeclarationNode: null name", LogCategory::CODEGEN);
        return;
    }
    
    // Look up function in symbol table
    std::string func_name = std::string(node->name->name);
    auto func_symbol = symbol_table_.lookup_symbol(func_name);
    
    IRType return_type = IRType::void_(); // Default to void
    if (func_symbol && func_symbol->type == SymbolType::FUNCTION) {
        return_type = func_symbol->data_type;
        // Function found with correct return type
    } else {
        LOG_ERROR("Function '" + func_name + "' not found in symbol table", LogCategory::CODEGEN);
    }
    
    // Navigate to the function scope to look up parameters
    int function_scope_id = symbol_table_.find_scope_by_name(func_name);
    if (function_scope_id == -1) {
        LOG_ERROR("Could not find function scope for: " + func_name, LogCategory::CODEGEN);
        return;
    }
    
    // Collect parameter types from symbol table
    std::vector<IRType> param_types;
    for (int i = 0; i < node->parameters.size; ++i) {
        if (auto* param = ast_cast_or_error<ParameterNode>(node->parameters.values[i])) {
            std::string param_name = std::string(param->name->name);
            auto param_symbol = symbol_table_.lookup_symbol_in_scope(function_scope_id, param_name);
            if (param_symbol && param_symbol->type == SymbolType::PARAMETER) {
                param_types.push_back(param_symbol->data_type);
            } else {
                LOG_ERROR("Parameter '" + param_name + "' not found in symbol table", LogCategory::CODEGEN);
                param_types.push_back(IRType::i32()); // Fallback
            }
        }
    }
    
    // Begin the function
    LOG_INFO("Processing function: '" + func_name + "'", LogCategory::CODEGEN);
    ir_builder_->function_begin(func_name, return_type, param_types);
    
    // Navigate to the function scope in the symbol table
    symbol_table_.push_scope(func_name);
    
    // Clear local variables for this function
    local_vars_.clear();
    
    // Process function parameters - allocate space for each parameter
    for (int i = 0; i < node->parameters.size; ++i) {
        if (auto* param = ast_cast_or_error<ParameterNode>(node->parameters.values[i])) {
            std::string param_name = std::string(param->name->name);
            auto param_symbol = symbol_table_.lookup_symbol(param_name);
            
            if (param_symbol && param_symbol->type == SymbolType::PARAMETER) {
                // Allocate space for the parameter
                ValueRef param_alloca = ir_builder_->alloca(param_symbol->data_type);
                local_vars_[param_name] = {param_alloca, param_symbol->data_type};
                
                LOG_DEBUG("Added parameter '" + param_name + "' of type " + param_symbol->type_name, LogCategory::CODEGEN);
            } else {
                LOG_ERROR("Parameter '" + param_name + "' not found in current scope", LogCategory::CODEGEN);
            }
        }
    }
    
    // Process the function body
    if (node->body) {
        LOG_DEBUG("Processing function body for: " + func_name, LogCategory::CODEGEN);
        node->body->accept(this);
    } else {
        LOG_WARN("No body for function: " + func_name, LogCategory::CODEGEN);
    }
    
    // Add automatic void return only for void functions that don't explicitly return
    if (return_type.kind == IRType::Kind::Void) {
        ir_builder_->ret_void();
    }
    // Note: Non-void functions must have explicit return statements
    
    // Pop the function scope from the symbol table
    symbol_table_.pop_scope();
    
    // End the function
    ir_builder_->function_end();
}

void CodeGenerator::visit(TypeDeclarationNode* node) {
    if (!node || !ir_builder_) {
        LOG_ERROR("TypeDeclarationNode: null node or null builder", LogCategory::CODEGEN);
        return;
    }
    
    if (!node->name) {
        LOG_ERROR("TypeDeclarationNode: null name", LogCategory::CODEGEN);
        return;
    }
    
    std::string type_name = std::string(node->name->name);
    LOG_INFO("Processing type declaration: '" + type_name + "'", LogCategory::CODEGEN);
    
    // Navigate to the type scope in symbol table
    int type_scope_id = symbol_table_.find_scope_by_name(type_name);
    if (type_scope_id == -1) {
        LOG_ERROR("Could not find type scope for: " + type_name, LogCategory::CODEGEN);
        return;
    }
    
    symbol_table_.push_scope(type_name);
    
    // Process all member functions in the type
    for (int i = 0; i < node->members.size; ++i) {
        auto* member = node->members[i];
        if (member && member->is_a<FunctionDeclarationNode>()) {
            auto* func_decl = member->as<FunctionDeclarationNode>();
            LOG_DEBUG("Processing member function: " + std::string(func_decl->name->name), LogCategory::CODEGEN);
            
            // Generate member function with implicit 'this' parameter
            visit_member_function(func_decl, type_name);
        }
        // Note: Field declarations are handled during struct layout creation, not here
    }
    
    symbol_table_.pop_scope();
    LOG_DEBUG("Completed processing type: " + type_name, LogCategory::CODEGEN);
}

void CodeGenerator::visit(BlockStatementNode* node) {
    if (!node) return;
    
    // Visit all statements in the block
    for (int i = 0; i < node->statements.size; ++i) {
        node->statements[i]->accept(this);
    }
}

void CodeGenerator::visit(AssignmentExpressionNode* node) {
    if (!node || !ir_builder_) return;
    
    // Generate code for the source expression first
    node->source->accept(this);
    ValueRef source_value = current_value_;
    
    if (!source_value.is_valid()) {
        std::cerr << "Error: Invalid source expression in assignment" << std::endl;
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // Handle different assignment targets
    if (node->target->is_a<IdentifierExpressionNode>()) {
        // Simple variable assignment: var = value
        auto* target_ident = ast_cast_or_error<IdentifierExpressionNode>(node->target);
        if (!target_ident || !target_ident->identifier || target_ident->identifier->name.empty()) {
            std::cerr << "Error: Invalid identifier in assignment target" << std::endl;
            current_value_ = ValueRef::invalid();
            return;
        }
        std::string var_name = std::string(target_ident->identifier->name);
        
        // Look up the variable in local variables first
        auto it = local_vars_.find(var_name);
        if (it != local_vars_.end()) {
            // Store the new value in local variable
            ir_builder_->store(source_value, it->second.value_ref);
        } else {
            // Check if we're in a member function and this is an unqualified field assignment
            std::string current_scope = symbol_table_.get_current_scope_name();
            if (current_scope.find("::") != std::string::npos) {
                // We're in a member function scope (Type::function)
                std::string type_name = current_scope.substr(0, current_scope.find("::"));
                
                // Check if this identifier is a field of the current type
                int type_scope_id = symbol_table_.find_scope_by_name(type_name);
                if (type_scope_id != -1) {
                    auto field_symbol = symbol_table_.lookup_symbol_in_scope(type_scope_id, var_name);
                    if (field_symbol && field_symbol->type == SymbolType::VARIABLE) {
                        // This is an unqualified field assignment - generate `this.field = value`
                        LOG_DEBUG("Generating unqualified field assignment for: " + var_name, LogCategory::CODEGEN);
                        
                        // Get the 'this' pointer from local variables
                        auto this_it = local_vars_.find("this");
                        if (this_it == local_vars_.end()) {
                            std::cerr << "Error: Cannot assign to field '" << var_name << "' - 'this' pointer not found" << std::endl;
                            current_value_ = ValueRef::invalid();
                            return;
                        }
                        
                        // Generate member access: this.field = value
                        auto struct_layout = build_struct_layout(type_name);
                        if (!struct_layout) {
                            std::cerr << "Error: Could not build struct layout for unqualified field assignment" << std::endl;
                            current_value_ = ValueRef::invalid();
                            return;
                        }
                        
                        // Find the field index
                        int field_index = find_field_index(*struct_layout, var_name);
                        if (field_index == -1) {
                            std::cerr << "Error: Field '" << var_name << "' not found in struct layout for assignment" << std::endl;
                            current_value_ = ValueRef::invalid();
                            return;
                        }
                        
                        // Load 'this' pointer
                        ValueRef this_value = ir_builder_->load(this_it->second.value_ref, this_it->second.type);
                        
                        // Generate GEP instruction to get field address
                        std::vector<int> indices = {field_index};
                        ValueRef field_ptr = ir_builder_->gep(this_value, indices, IRType::ptr_to(field_symbol->data_type));
                        
                        // Store the new value in the field
                        ir_builder_->store(source_value, field_ptr);
                    } else {
                        std::cerr << "Error: Unknown variable in assignment: '" << var_name << "'" << std::endl;
                        current_value_ = ValueRef::invalid();
                        return;
                    }
                } else {
                    std::cerr << "Error: Unknown variable in assignment: '" << var_name << "'" << std::endl;
                    current_value_ = ValueRef::invalid();
                    return;
                }
            } else {
                std::cerr << "Error: Unknown variable in assignment: '" << var_name << "'" << std::endl;
                current_value_ = ValueRef::invalid();
                return;
            }
        }
        
    } else if (node->target->is_a<MemberAccessExpressionNode>()) {
        // Member assignment: obj.member = value
        auto* member_access = ast_cast_or_error<MemberAccessExpressionNode>(node->target);
        if (!member_access || !member_access->target || !member_access->member || member_access->member->name.empty()) {
            std::cerr << "Error: Invalid member access node in assignment target" << std::endl;
            current_value_ = ValueRef::invalid();
            return;
        }
        
        // Generate code for the target object to get struct pointer
        member_access->target->accept(this);
        ValueRef struct_ptr = current_value_;
        
        if (!struct_ptr.is_valid() || struct_ptr.type.kind != IRType::Kind::Ptr) {
            std::cerr << "Error: Invalid member access target in assignment" << std::endl;
            current_value_ = ValueRef::invalid();
            return;
        }
        
        // Get the member name
        std::string member_name = std::string(member_access->member->name);
        
        // Get the actual struct type from the pointer's pointee type
        std::string struct_type_name;
        if (struct_ptr.type.pointee_type && struct_ptr.type.pointee_type->kind == IRType::Kind::Struct 
            && struct_ptr.type.pointee_type->struct_layout) {
            struct_type_name = struct_ptr.type.pointee_type->struct_layout->name;
        } else {
            std::cerr << "Error: Cannot determine struct type for member assignment - pointer lacks type information" << std::endl;
            current_value_ = ValueRef::invalid();
            return;
        }
        
        // Build struct layout to find the field
        auto struct_layout = build_struct_layout(struct_type_name);
        if (!struct_layout) {
            std::cerr << "Error: Could not determine struct layout for member assignment" << std::endl;
            current_value_ = ValueRef::invalid();
            return;
        }
        
        // Find the field in the layout
        int field_index = -1;
        IRType field_type = IRType::i32();
        
        for (size_t i = 0; i < struct_layout->fields.size(); ++i) {
            if (struct_layout->fields[i].name == member_name) {
                field_index = static_cast<int>(i);
                field_type = struct_layout->fields[i].type;
                break;
            }
        }
        
        if (field_index == -1) {
            std::cerr << "Error: Field '" << member_name << "' not found in struct for assignment" << std::endl;
            current_value_ = ValueRef::invalid();
            return;
        }
        
        // Generate GEP instruction to get field address
        std::vector<int> indices = {field_index};
        ValueRef field_ptr = ir_builder_->gep(struct_ptr, indices, IRType::ptr_to(field_type));
        
        if (!field_ptr.is_valid()) {
            std::cerr << "Error: Failed to generate field access for assignment" << std::endl;
            current_value_ = ValueRef::invalid();
            return;
        }
        
        // Store the value in the field
        ir_builder_->store(source_value, field_ptr);
        
    } else {
        std::cerr << "Error: Unsupported assignment target type" << std::endl;
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // Assignment expressions return the assigned value
    current_value_ = source_value;
}

void CodeGenerator::visit(IfStatementNode* node) {
    if (!node || !ir_builder_) return;
    
    // Generate proper if-else structure with basic blocks
    // if (condition) { then } else if (...) { ... } else { ... }
    //
    // Generates:
    // entry:
    //   <condition>
    //   br_cond %condition, %then_label, %else_label
    // then_label:
    //   <then_body>
    //   br %end_label
    // else_label:
    //   <else_body>
    //   br %end_label
    // end_label:
    //   <continue>
    
    // Create basic block labels (use global counter for unique names across all functions)
    static int global_if_counter = 0;
    int if_id = global_if_counter++;
    std::string then_label = "if_then_" + std::to_string(if_id);
    std::string else_label = "if_else_" + std::to_string(if_id);
    std::string end_label = "if_end_" + std::to_string(if_id);
    
    // Evaluate the condition
    if (node->condition) {
        node->condition->accept(this);
        if (current_value_.is_valid()) {
            // Branch based on condition
            if (node->elseStatement) {
                ir_builder_->br_cond(current_value_, then_label, else_label);
            } else {
                // No else clause - branch directly to end on false
                ir_builder_->br_cond(current_value_, then_label, end_label);
            }
        } else {
            // Invalid condition - branch directly to end
            ir_builder_->br(end_label);
        }
    } else {
        // No condition - this shouldn't happen, but handle it gracefully
        ir_builder_->br(end_label);
    }
    
    // Then block
    ir_builder_->label(then_label);
    if (node->thenStatement) {
        node->thenStatement->accept(this);
    }
    // Only branch to end if the block doesn't already have a terminator (like return)
    if (!ir_builder_->has_terminator()) {
        ir_builder_->br(end_label);
    }
    
    // Else block (if present)
    if (node->elseStatement) {
        ir_builder_->label(else_label);
        node->elseStatement->accept(this);
        // Only branch to end if the block doesn't already have a terminator
        if (!ir_builder_->has_terminator()) {
            ir_builder_->br(end_label);
        }
    }
    
    // End label
    ir_builder_->label(end_label);
}

void CodeGenerator::visit(WhileStatementNode* node) {
    if (!node || !ir_builder_) return;
    
    // Generate proper while loop structure with basic blocks
    // while (condition) { body }
    //
    // Generates:
    // entry:
    //   br %while_header
    // while_header:
    //   <condition>
    //   br_cond %condition, %while_body, %while_exit
    // while_body:
    //   <body>
    //   br %while_header
    // while_exit:
    //   <continue>
    
    // Create basic block labels (use global counter for unique names across all functions)
    static int global_while_counter = 0;
    int while_id = global_while_counter++;
    std::string header_label = "while_header_" + std::to_string(while_id);
    std::string body_label = "while_body_" + std::to_string(while_id);
    std::string exit_label = "while_exit_" + std::to_string(while_id);
    
    // Branch to while header
    ir_builder_->br(header_label);
    
    // While header: check condition
    ir_builder_->label(header_label);
    if (node->condition) {
        node->condition->accept(this);
        if (current_value_.is_valid()) {
            ir_builder_->br_cond(current_value_, body_label, exit_label);
        } else {
            // Invalid condition - branch directly to exit
            ir_builder_->br(exit_label);
        }
    } else {
        // No condition - infinite loop (branch to body)
        ir_builder_->br(body_label);
    }
    
    // While body
    ir_builder_->label(body_label);
    if (node->body) {
        node->body->accept(this);
    }
    
    // Branch back to header
    ir_builder_->br(header_label);
    
    // While exit
    ir_builder_->label(exit_label);
}

void CodeGenerator::visit(ForStatementNode* node) {
    if (!node || !ir_builder_) return;
    
    // Generate proper loop structure with basic blocks
    // for (init; condition; increment) { body }
    //
    // Generates:
    // entry:
    //   <initializer>
    //   br %loop_header
    // loop_header:
    //   <condition>
    //   br_cond %condition, %loop_body, %loop_exit
    // loop_body:
    //   <body>
    //   <incrementors>
    //   br %loop_header
    // loop_exit:
    //   <continue>
    
    // Process the initializer
    if (node->initializer) {
        node->initializer->accept(this);
    }
    
    // Create basic block labels (use global counter for unique names across all functions)
    static int global_loop_counter = 0;
    int loop_id = global_loop_counter++;
    std::string header_label = "loop_header_" + std::to_string(loop_id);
    std::string body_label = "loop_body_" + std::to_string(loop_id);
    std::string exit_label = "loop_exit_" + std::to_string(loop_id);
    
    // Branch to loop header
    ir_builder_->br(header_label);
    
    // Loop header: check condition
    ir_builder_->label(header_label);
    if (node->condition) {
        node->condition->accept(this);
        if (current_value_.is_valid()) {
            ir_builder_->br_cond(current_value_, body_label, exit_label);
        } else {
            // Invalid condition - branch directly to exit
            ir_builder_->br(exit_label);
        }
    } else {
        // No condition - infinite loop (branch to body)
        ir_builder_->br(body_label);
    }
    
    // Loop body
    ir_builder_->label(body_label);
    if (node->body) {
        node->body->accept(this);
    }
    
    // Execute incrementors
    for (int i = 0; i < node->incrementors.size; ++i) {
        node->incrementors[i]->accept(this);
    }
    
    // Branch back to header
    ir_builder_->br(header_label);
    
    // Loop exit
    ir_builder_->label(exit_label);
}

void CodeGenerator::visit(ExpressionStatementNode* node) {
    if (!node || !ir_builder_) return;
    
    // Just visit the expression - the result will be in current_value_
    if (node->expression) {
        node->expression->accept(this);
    }
}

void CodeGenerator::visit(CallExpressionNode* node) {
    if (!node || !ir_builder_) return;

    // 1. Evaluate all argument expressions and collect their ValueRefs
    std::vector<ValueRef> arg_values;
    LOG_DEBUG("Generating arguments for call expression", LogCategory::CODEGEN);
    for (int i = 0; i < node->arguments.size; ++i) {
        node->arguments[i]->accept(this);
        if (!current_value_.is_valid()) {
            std::cerr << "Error: Invalid expression for argument " << i << " in function call." << std::endl;
            current_value_ = ValueRef::invalid();
            return;
        }
        arg_values.push_back(current_value_);
    }

    // Check if this is a member function call (obj.method()) or a regular function call (func())
    if (auto member_access = node->target->as<MemberAccessExpressionNode>()) {
        // This is a member function call: obj.method(args)
        LOG_DEBUG("Generating member function call", LogCategory::CODEGEN);
        
        // Generate code for the target object to get 'this' pointer
        member_access->target->accept(this);
        ValueRef this_ptr = current_value_;
        
        if (!this_ptr.is_valid() || this_ptr.type.kind != IRType::Kind::Ptr) {
            std::cerr << "Error: Invalid target object for member function call" << std::endl;
            current_value_ = ValueRef::invalid();
            return;
        }
        
        // Get the struct type name from the 'this' pointer
        std::string struct_type_name;
        if (this_ptr.type.pointee_type && this_ptr.type.pointee_type->kind == IRType::Kind::Struct 
            && this_ptr.type.pointee_type->struct_layout) {
            struct_type_name = this_ptr.type.pointee_type->struct_layout->name;
        } else {
            std::cerr << "Error: Cannot determine struct type for member function call" << std::endl;
            current_value_ = ValueRef::invalid();
            return;
        }
        
        // Get the method name
        std::string method_name = std::string(member_access->member->name);
        
        // Create mangled name for the member function
        std::string mangled_name = struct_type_name + "::" + method_name;
        LOG_DEBUG("Calling member function: '" + mangled_name + "'", LogCategory::CODEGEN);
        
        // Look up the member function in the type scope to get return type
        int type_scope_id = symbol_table_.find_scope_by_name(struct_type_name);
        IRType return_type = IRType::void_(); // Default to void
        
        if (type_scope_id != -1) {
            auto method_symbol = symbol_table_.lookup_symbol_in_scope(type_scope_id, method_name);
            if (method_symbol && method_symbol->type == SymbolType::FUNCTION) {
                return_type = method_symbol->data_type;
                LOG_DEBUG("Found member function '" + method_name + "' with return type: " + method_symbol->type_name, LogCategory::CODEGEN);
            } else {
                LOG_ERROR("Member function '" + method_name + "' not found in type '" + struct_type_name + "'", LogCategory::CODEGEN);
            }
        } else {
            LOG_ERROR("Type scope '" + struct_type_name + "' not found for member function call", LogCategory::CODEGEN);
        }
        
        // Prepend 'this' pointer to argument list
        std::vector<ValueRef> member_func_args;
        member_func_args.push_back(this_ptr);
        member_func_args.insert(member_func_args.end(), arg_values.begin(), arg_values.end());
        
        // Call the mangled member function
        // Generate call with proper return type
        current_value_ = ir_builder_->call(mangled_name, return_type, member_func_args);
        
    } else if (auto ident = node->target->as<IdentifierExpressionNode>()) {
        // This is a regular function call: func(args)
        std::string func_name = std::string(ident->identifier->name);
        LOG_DEBUG("Generating call to function: '" + func_name + "'", LogCategory::CODEGEN);

        // Look up the function in the symbol table to get its return type
        IRType return_type = IRType::void_(); // Default to void if not found
        
        auto symbol = symbol_table_.lookup_symbol(func_name);
        if (symbol && symbol->type == SymbolType::FUNCTION) {
            // The data_type field contains the return type for functions
            return_type = symbol->data_type;
            LOG_DEBUG("Found function '" + func_name + "' with return type: " + symbol->type_name, LogCategory::CODEGEN);
        } else {
            LOG_WARN("Function '" + func_name + "' not found in symbol table, assuming void return type", LogCategory::CODEGEN);
        }
        
        current_value_ = ir_builder_->call(func_name, return_type, arg_values);
        
    } else {
        std::cerr << "Error: Unsupported call target type. Only simple function calls and member function calls are supported." << std::endl;
        current_value_ = ValueRef::invalid();
    }
}

void CodeGenerator::visit(MemberAccessExpressionNode* node) {
    if (!node || !ir_builder_ || !node->target || !node->member || node->member->name.empty()) {
        std::cerr << "Error: Invalid member access node structure" << std::endl;
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // Generate code for the target object (should result in a pointer to the struct)
    node->target->accept(this);
    ValueRef struct_ptr = current_value_;
    
    if (!struct_ptr.is_valid()) {
        std::cerr << "Error: Invalid target in member access" << std::endl;
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // Get the member name
    std::string member_name = std::string(node->member->name);
    
    // We need to determine the struct type from the target
    // For now, we'll assume it's a pointer to a struct type
    if (struct_ptr.type.kind != IRType::Kind::Ptr) {
        std::cerr << "Error: Member access target must be a pointer to struct" << std::endl;
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // Get the actual struct type from the pointer's pointee type
    std::string struct_type_name;
    if (struct_ptr.type.pointee_type && struct_ptr.type.pointee_type->kind == IRType::Kind::Struct 
        && struct_ptr.type.pointee_type->struct_layout) {
        struct_type_name = struct_ptr.type.pointee_type->struct_layout->name;
    } else {
        std::cerr << "Error: Cannot determine struct type for member access - pointer lacks type information" << std::endl;
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // First, check if this is a member function - accessing a method without calling it is an error
    int type_scope_id = symbol_table_.find_scope_by_name(struct_type_name);
    if (type_scope_id != -1) {
        auto member_symbol = symbol_table_.lookup_symbol_in_scope(type_scope_id, member_name);
        if (member_symbol && member_symbol->type == SymbolType::FUNCTION) {
            std::cerr << "Error: Cannot access member function '" << member_name << "' without calling it. Use obj." 
                      << member_name << "() to call the method." << std::endl;
            current_value_ = ValueRef::invalid();
            return;
        }
    }
    
    // Build the struct layout to find the field
    auto struct_layout = build_struct_layout(struct_type_name);
    if (!struct_layout) {
        std::cerr << "Error: Could not determine struct layout for member access" << std::endl;
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // Find the field in the layout
    int field_index = -1;
    IRType field_type = IRType::i32(); // Default
    
    for (size_t i = 0; i < struct_layout->fields.size(); ++i) {
        if (struct_layout->fields[i].name == member_name) {
            field_index = static_cast<int>(i);
            field_type = struct_layout->fields[i].type;
            break;
        }
    }
    
    if (field_index == -1) {
        std::cerr << "Error: Field '" << member_name << "' not found in struct '" << struct_type_name << "'" << std::endl;
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // Generate GEP instruction to get field address
    std::vector<int> indices = {field_index};
    ValueRef field_ptr = ir_builder_->gep(struct_ptr, indices, IRType::ptr_to(field_type));
    
    if (!field_ptr.is_valid()) {
        std::cerr << "Error: Failed to generate field access" << std::endl;
        current_value_ = ValueRef::invalid();
        return;
    }
    
    // For struct types, return the pointer. For primitive types, load the value.
    if (field_type.kind == IRType::Kind::Struct) {
        // For struct fields, return the pointer so further member access can work
        current_value_ = field_ptr;
    } else {
        // For primitive fields, load the value
        current_value_ = ir_builder_->load(field_ptr, field_type);
    }
}

void CodeGenerator::visit(IndexerExpressionNode* node) {
    if (!node || !ir_builder_) return;
    
    // For now, array indexing is not supported - provide clear error message
    std::cerr << "Error: Array indexing expressions (arr[index]) are not yet implemented in code generation" << std::endl;
    current_value_ = ValueRef::invalid();
}

void CodeGenerator::visit(NewExpressionNode* node) {
    if (!node || !ir_builder_) return;
    
    // Get the type name from the node
    std::string type_name = "Unknown";
    if (node->type && node->type->identifier) {
        type_name = std::string(node->type->identifier->name);
    }
    
    // Look up the type in the symbol table
    auto type_symbol = symbol_table_.lookup_symbol(type_name);
    if (type_symbol && type_symbol->type == SymbolType::CLASS) {
        // Build struct layout from symbol table
        auto struct_layout = build_struct_layout(type_name);
        if (struct_layout) {
            // Create struct type
            IRType struct_type = IRType::struct_(struct_layout);
            
            // Allocate struct on stack
            ValueRef struct_alloca = ir_builder_->alloca(struct_type);
            
            // Initialize fields with default values
            int struct_scope_id = symbol_table_.find_scope_by_name(type_name);
            if (struct_scope_id != -1) {
                auto field_symbols = symbol_table_.get_all_symbols_in_scope(struct_scope_id);
                
                // Process each field that has an initializer expression
                for (const auto& field_symbol : field_symbols) {
                    if (field_symbol && field_symbol->type == SymbolType::VARIABLE && 
                        field_symbol->initializer_expression) {
                        
                        // Generate code for the initializer expression
                        field_symbol->initializer_expression->accept(this);
                        ValueRef init_value = current_value_;
                        
                        // Find field index in struct layout
                        int field_index = find_field_index(*struct_layout, field_symbol->name);
                        if (field_index >= 0) {
                            // Generate GEP to field location
                            ValueRef field_ptr = ir_builder_->gep(struct_alloca, {field_index}, 
                                                                   IRType::ptr_to(field_symbol->data_type));
                            
                            // For struct types, we need to load the value from the pointer and store the struct value
                            if (field_symbol->data_type.kind == IRType::Kind::Struct) {
                                // Load the struct value from the pointer
                                ValueRef struct_value = ir_builder_->load(init_value, field_symbol->data_type);
                                ir_builder_->store(struct_value, field_ptr);
                            } else {
                                // For primitive types, store the value directly
                                ir_builder_->store(init_value, field_ptr);
                            }
                        } else {
                            std::cerr << "Error: Could not find field index for: " << field_symbol->name << std::endl;
                        }
                    }
                }
            }
            
            // TODO: Call constructor if present
            
            // Return pointer to allocated struct
            current_value_ = struct_alloca;
        } else {
            std::cerr << "Error: Failed to build layout for type: " << type_name << std::endl;
            current_value_ = ValueRef::invalid();
        }
    } else {
        std::cerr << "Error: Unknown type in 'new' expression: " << type_name << std::endl;
        current_value_ = ValueRef::invalid();
    }
}


std::vector<Command> CodeGenerator::generate_code(CompilationUnitNode* root) {
    if (!root) {
        LOG_ERROR("No AST root provided", LogCategory::CODEGEN);
        return {};
    }
    
    LOG_INFO("Starting code generation...", LogCategory::CODEGEN);
    
    // Create the IR builder
    ir_builder_ = std::make_unique<IRBuilder>();
    
    // Pre-generate all struct types to ensure LLVM type definitions exist
    pre_generate_struct_types();
    
    // Visit the compilation unit to generate commands
    root->accept(this);
    
    // Debug: dump the command stream
    // LOG_DEBUG("Generated command stream:", LogCategory::CODEGEN);
    // ir_builder_->dump_commands();
    
    LOG_INFO("Code generation complete", LogCategory::CODEGEN);
    
    // Return the generated commands
    return ir_builder_->commands();
}

std::shared_ptr<StructLayout> CodeGenerator::build_struct_layout(const std::string& struct_name) {
    // Look up the struct/class scope in the symbol table
    int struct_scope_id = symbol_table_.find_scope_by_name(struct_name);
    if (struct_scope_id == -1) {
        std::cerr << "Error: Could not find scope for struct: " << struct_name << std::endl;
        return nullptr;
    }
    
    // Create the struct layout
    auto layout = std::make_shared<StructLayout>();
    layout->name = struct_name;
    
    // Get all symbols in the struct scope
    auto symbols = symbol_table_.get_all_symbols_in_scope(struct_scope_id);
    
    // Process all variable symbols (struct fields)
    for (const auto& symbol : symbols) {
        if (symbol && symbol->type == SymbolType::VARIABLE) {
            StructLayout::Field field;
            field.name = symbol->name;
            field.type = symbol->data_type;
            field.offset = 0; // Will be calculated by calculate_layout()
            
            layout->fields.push_back(field);
        }
    }
    
    // Calculate field offsets and total size
    layout->calculate_layout();
    
    return layout;
}

void CodeGenerator::pre_generate_struct_types() {
    LOG_DEBUG("Pre-generating all struct types...", LogCategory::CODEGEN);
    
    // Get all symbols from the global scope (scope ID 0)
    auto global_symbols = symbol_table_.get_all_symbols_in_scope(0);
    
    // Process all CLASS symbols
    for (const auto& symbol : global_symbols) {
        if (symbol && symbol->type == SymbolType::CLASS) {
            LOG_DEBUG("Pre-generating struct type: " + symbol->name, LogCategory::CODEGEN);
            
            // Build struct layout - this will create the LLVM type definition
            auto struct_layout = build_struct_layout(symbol->name);
            if (struct_layout) {
                // Create the IR type to ensure it's registered in the type system
                IRType struct_type = IRType::struct_(struct_layout);
                LOG_DEBUG("Successfully pre-generated type: " + symbol->name, LogCategory::CODEGEN);
            } else {
                std::cerr << "Error: Failed to pre-generate struct layout for: " << symbol->name << std::endl;
            }
        }
    }
    
    LOG_DEBUG("Struct type pre-generation complete", LogCategory::CODEGEN);
}

int CodeGenerator::find_field_index(const StructLayout& layout, const std::string& field_name) {
    for (size_t i = 0; i < layout.fields.size(); ++i) {
        if (layout.fields[i].name == field_name) {
            return static_cast<int>(i);
        }
    }
    return -1; // Field not found
}

void CodeGenerator::visit_member_function(FunctionDeclarationNode* node, const std::string& owner_type) {
    if (!node || !ir_builder_) {
        LOG_ERROR("visit_member_function: null node or null builder", LogCategory::CODEGEN);
        return;
    }
    
    if (!node->name) {
        LOG_ERROR("visit_member_function: null function name", LogCategory::CODEGEN);
        return;
    }
    
    std::string func_name = std::string(node->name->name);
    
    // Create mangled name for member function to avoid conflicts with global functions
    std::string mangled_name = owner_type + "::" + func_name;
    
    // Look up member function in symbol table using the qualified scope name
    std::string member_func_scope_name = owner_type + "::" + func_name;
    int member_func_scope_id = symbol_table_.find_scope_by_name(member_func_scope_name);
    if (member_func_scope_id == -1) {
        LOG_ERROR("Could not find member function scope for: " + mangled_name, LogCategory::CODEGEN);
        return;
    }
    
    // Get function return type from symbol table
    auto func_symbol = symbol_table_.lookup_symbol_in_scope(symbol_table_.find_scope_by_name(owner_type), func_name);
    IRType return_type = IRType::void_(); // Default to void
    if (func_symbol && func_symbol->type == SymbolType::FUNCTION) {
        return_type = func_symbol->data_type;
        // Member function found with correct return type
    } else {
        LOG_ERROR("Member function '" + func_name + "' not found in type scope", LogCategory::CODEGEN);
    }
    
    // Collect parameter types: 'this' pointer + explicit parameters
    std::vector<IRType> param_types;
    
    // Add implicit 'this' parameter (pointer to owner type)
    IRType this_type = IRType::ptr_to(symbol_table_.string_to_ir_type(owner_type));
    param_types.push_back(this_type);
    
    // Add explicit parameters
    for (int i = 0; i < node->parameters.size; ++i) {
        if (auto* param = ast_cast_or_error<ParameterNode>(node->parameters.values[i])) {
            std::string param_name = std::string(param->name->name);
            auto param_symbol = symbol_table_.lookup_symbol_in_scope(member_func_scope_id, param_name);
            if (param_symbol && param_symbol->type == SymbolType::PARAMETER) {
                param_types.push_back(param_symbol->data_type);
            } else {
                LOG_ERROR("Parameter '" + param_name + "' not found in member function scope", LogCategory::CODEGEN);
                param_types.push_back(IRType::i32()); // Fallback
            }
        }
    }
    
    // Begin the member function with mangled name
    LOG_INFO("Processing member function: '" + mangled_name + "'", LogCategory::CODEGEN);
    ir_builder_->function_begin(mangled_name, return_type, param_types);
    
    // Navigate to the member function scope in the symbol table
    symbol_table_.push_scope(member_func_scope_name);
    
    // Clear local variables for this function
    local_vars_.clear();
    
    // Allocate space for 'this' parameter
    auto this_symbol = symbol_table_.lookup_symbol("this");
    if (this_symbol && this_symbol->type == SymbolType::PARAMETER) {
        ValueRef this_alloca = ir_builder_->alloca(this_symbol->data_type);
        local_vars_["this"] = {this_alloca, this_symbol->data_type};
        LOG_DEBUG("Added 'this' parameter of type " + this_symbol->type_name, LogCategory::CODEGEN);
    }
    
    // Process explicit function parameters - allocate space for each parameter
    for (int i = 0; i < node->parameters.size; ++i) {
        if (auto* param = ast_cast_or_error<ParameterNode>(node->parameters.values[i])) {
            std::string param_name = std::string(param->name->name);
            auto param_symbol = symbol_table_.lookup_symbol(param_name);
            
            if (param_symbol && param_symbol->type == SymbolType::PARAMETER) {
                // Allocate space for the parameter
                ValueRef param_alloca = ir_builder_->alloca(param_symbol->data_type);
                local_vars_[param_name] = {param_alloca, param_symbol->data_type};
                
                LOG_DEBUG("Added parameter '" + param_name + "' of type " + param_symbol->type_name, LogCategory::CODEGEN);
            } else {
                LOG_ERROR("Parameter '" + param_name + "' not found in current scope", LogCategory::CODEGEN);
            }
        }
    }
    
    // Process the function body
    if (node->body) {
        LOG_DEBUG("Processing member function body for: " + mangled_name, LogCategory::CODEGEN);
        node->body->accept(this);
    } else {
        LOG_WARN("No body for member function: " + mangled_name, LogCategory::CODEGEN);
    }
    
    // Add automatic void return only for void functions that don't explicitly return
    if (return_type.kind == IRType::Kind::Void) {
        ir_builder_->ret_void();
    }
    // Note: Non-void functions must have explicit return statements
    
    // Pop the member function scope from the symbol table
    symbol_table_.pop_scope();
    
    // End the function
    ir_builder_->function_end();
}

} // namespace Mycelium::Scripting::Lang