#include "ast/ast.hpp"
#include "ast/ast_allocator.hpp"
#include "ast/ast_rtti.hpp"
#include "ast/ast_printer.hpp"
#include "semantic/symbol_table.hpp"
#include "codegen/codegen.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <cstring> // For memcpy

// Bring the AST namespace into scope for convenience.
using namespace Mycelium::Scripting::Lang;

// --- Helper Functions for Manual AST Construction ---

template<typename T>
SizedArray<T> create_sized_array(AstAllocator& allocator, const std::vector<T>& items) {
    SizedArray<T> arr;
    arr.size = (int)items.size();
    if (arr.size > 0) {
        arr.values = (T*)allocator.alloc_bytes(sizeof(T) * arr.size, alignof(T));
        memcpy(arr.values, items.data(), sizeof(T) * arr.size);
    } else {
        arr.values = nullptr;
    }
    return arr;
}

TokenNode* create_token(AstAllocator& allocator, TokenKind kind, const std::string& text) {
    auto token = allocator.alloc<TokenNode>();
    token->tokenKind = kind;
    char* text_buffer = (char*)allocator.alloc_bytes(text.length() + 1, 1);
    memcpy(text_buffer, text.c_str(), text.length() + 1);
    token->text = std::string_view(text_buffer, text.length());
    return token;
}

IdentifierNode* create_identifier(AstAllocator& allocator, const std::string& name) {
    auto ident = allocator.alloc<IdentifierNode>();
    char* name_buffer = (char*)allocator.alloc_bytes(name.length() + 1, 1);
    memcpy(name_buffer, name.c_str(), name.length() + 1);
    ident->name = std::string_view(name_buffer, name.length());
    return ident;
}

TypeNameNode* create_type_name(AstAllocator& allocator, const std::string& name) {
    auto type_node = allocator.alloc<TypeNameNode>();
    type_node->identifier = create_identifier(allocator, name);
    return type_node;
}

// --- Main Test Function ---

int main()
{
    // 1. Initialize RTTI
    AstTypeInfo::initialize();
    std::cout << "RTTI Initialized. Total types: " << (AstNode::sTypeInfo.fullDerivedCount + 1) << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 2. Create Allocator
    AstAllocator allocator;

    // 3. Manually construct the AST for the provided code
    
    // --- Console static type ---
    
    // Console fields
    auto console_messageCount_field = allocator.alloc<FieldDeclarationNode>();
    console_messageCount_field->name = create_identifier(allocator, "messageCount");
    console_messageCount_field->type = create_type_name(allocator, "i32");
    console_messageCount_field->modifiers = create_sized_array<ModifierKind>(allocator, {ModifierKind::Public, ModifierKind::Mutable});
    
    auto console_doubleVar2_field = allocator.alloc<FieldDeclarationNode>();
    console_doubleVar2_field->name = create_identifier(allocator, "doubleVar2");
    console_doubleVar2_field->type = create_type_name(allocator, "f64");
    auto double_literal_24 = allocator.alloc<LiteralExpressionNode>();
    double_literal_24->kind = LiteralKind::Double;
    double_literal_24->token = create_token(allocator, TokenKind::FloatLiteral, "2.4");
    console_doubleVar2_field->initializer = double_literal_24;
    
    auto console_lastMessage_field = allocator.alloc<FieldDeclarationNode>();
    console_lastMessage_field->name = create_identifier(allocator, "lastMessage");
    console_lastMessage_field->type = create_type_name(allocator, "string");
    console_lastMessage_field->modifiers = create_sized_array<ModifierKind>(allocator, {ModifierKind::Mutable});
    
    // Console.Log method body
    auto print_call_expr = allocator.alloc<CallExpressionNode>();
    print_call_expr->target = allocator.alloc<IdentifierExpressionNode>();
    ((IdentifierExpressionNode*)print_call_expr->target)->identifier = create_identifier(allocator, "Print");
    auto msg_arg_expr = allocator.alloc<IdentifierExpressionNode>();
    msg_arg_expr->identifier = create_identifier(allocator, "msg");
    print_call_expr->arguments = create_sized_array<ExpressionNode*>(allocator, {msg_arg_expr});
    auto print_call_stmt = allocator.alloc<ExpressionStatementNode>();
    print_call_stmt->expression = print_call_expr;
    
    auto messageCount_increment = allocator.alloc<UnaryExpressionNode>();
    messageCount_increment->opKind = UnaryOperatorKind::PostIncrement;
    messageCount_increment->operand = allocator.alloc<IdentifierExpressionNode>();
    ((IdentifierExpressionNode*)messageCount_increment->operand)->identifier = create_identifier(allocator, "messageCount");
    messageCount_increment->isPostfix = true;
    auto increment_stmt = allocator.alloc<ExpressionStatementNode>();
    increment_stmt->expression = messageCount_increment;
    
    auto lastMessage_assign = allocator.alloc<AssignmentExpressionNode>();
    lastMessage_assign->opKind = AssignmentOperatorKind::Assign;
    lastMessage_assign->target = allocator.alloc<IdentifierExpressionNode>();
    ((IdentifierExpressionNode*)lastMessage_assign->target)->identifier = create_identifier(allocator, "lastMessage");
    lastMessage_assign->source = allocator.alloc<IdentifierExpressionNode>();
    ((IdentifierExpressionNode*)lastMessage_assign->source)->identifier = create_identifier(allocator, "msg");
    auto assign_stmt = allocator.alloc<ExpressionStatementNode>();
    assign_stmt->expression = lastMessage_assign;
    
    auto console_log_body = allocator.alloc<BlockStatementNode>();
    console_log_body->statements = create_sized_array<StatementNode*>(allocator, {print_call_stmt, increment_stmt, assign_stmt});
    
    auto console_log_method = allocator.alloc<FunctionDeclarationNode>();
    console_log_method->name = create_identifier(allocator, "Log");
    console_log_method->returnType = create_type_name(allocator, "void");
    console_log_method->modifiers = create_sized_array<ModifierKind>(allocator, {ModifierKind::Public});
    console_log_method->body = console_log_body;
    auto log_param = allocator.alloc<ParameterNode>();
    log_param->name = create_identifier(allocator, "msg");
    log_param->type = create_type_name(allocator, "string");
    console_log_method->parameters = create_sized_array<ParameterNode*>(allocator, {log_param});
    
    // Console.GetLast method
    auto return_lastMessage = allocator.alloc<ReturnStatementNode>();
    return_lastMessage->expression = allocator.alloc<IdentifierExpressionNode>();
    ((IdentifierExpressionNode*)return_lastMessage->expression)->identifier = create_identifier(allocator, "lastMessage");
    auto console_getLast_body = allocator.alloc<BlockStatementNode>();
    console_getLast_body->statements = create_sized_array<StatementNode*>(allocator, {return_lastMessage});
    
    auto console_getLast_method = allocator.alloc<FunctionDeclarationNode>();
    console_getLast_method->name = create_identifier(allocator, "GetLast");
    console_getLast_method->returnType = create_type_name(allocator, "string");
    console_getLast_method->modifiers = create_sized_array<ModifierKind>(allocator, {ModifierKind::Public});
    console_getLast_method->body = console_getLast_body;
    console_getLast_method->parameters = create_sized_array<ParameterNode*>(allocator, {});
    
    auto console_class = allocator.alloc<ClassDeclarationNode>();
    console_class->name = create_identifier(allocator, "Console");
    console_class->modifiers = create_sized_array<ModifierKind>(allocator, {ModifierKind::Static});
    console_class->members = create_sized_array<MemberDeclarationNode*>(allocator, {
        (MemberDeclarationNode*)console_messageCount_field,
        (MemberDeclarationNode*)console_doubleVar2_field,
        (MemberDeclarationNode*)console_lastMessage_field,
        (MemberDeclarationNode*)console_log_method,
        (MemberDeclarationNode*)console_getLast_method
    });
    
    // --- Vector3 type ---
    
    auto vector3_x_field = allocator.alloc<FieldDeclarationNode>();
    vector3_x_field->name = create_identifier(allocator, "x");
    vector3_x_field->type = create_type_name(allocator, "f32");
    vector3_x_field->modifiers = create_sized_array<ModifierKind>(allocator, {ModifierKind::Public, ModifierKind::Mutable});
    
    auto vector3_y_field = allocator.alloc<FieldDeclarationNode>();
    vector3_y_field->name = create_identifier(allocator, "y");
    vector3_y_field->type = create_type_name(allocator, "f32");
    vector3_y_field->modifiers = create_sized_array<ModifierKind>(allocator, {ModifierKind::Public, ModifierKind::Mutable});
    
    auto vector3_z_field = allocator.alloc<FieldDeclarationNode>();
    vector3_z_field->name = create_identifier(allocator, "z");
    vector3_z_field->type = create_type_name(allocator, "f32");
    vector3_z_field->modifiers = create_sized_array<ModifierKind>(allocator, {ModifierKind::Public, ModifierKind::Mutable});
    
    auto vector3_class = allocator.alloc<ClassDeclarationNode>();
    vector3_class->name = create_identifier(allocator, "Vector3");
    vector3_class->members = create_sized_array<MemberDeclarationNode*>(allocator, {
        (MemberDeclarationNode*)vector3_x_field,
        (MemberDeclarationNode*)vector3_y_field,
        (MemberDeclarationNode*)vector3_z_field
    });
    
    // --- Enemy ref type ---
    
    auto enemy_position_field = allocator.alloc<FieldDeclarationNode>();
    enemy_position_field->name = create_identifier(allocator, "position");
    enemy_position_field->type = create_type_name(allocator, "Vector3");
    enemy_position_field->modifiers = create_sized_array<ModifierKind>(allocator, {ModifierKind::Public, ModifierKind::Mutable});
    
    auto enemy_attack_field = allocator.alloc<FieldDeclarationNode>();
    enemy_attack_field->name = create_identifier(allocator, "attack");
    enemy_attack_field->type = create_type_name(allocator, "i32");
    
    auto enemy_hitChance_field = allocator.alloc<FieldDeclarationNode>();
    enemy_hitChance_field->name = create_identifier(allocator, "hitChance");
    enemy_hitChance_field->type = create_type_name(allocator, "f32");
    auto hitChance_literal = allocator.alloc<LiteralExpressionNode>();
    hitChance_literal->kind = LiteralKind::Float;
    hitChance_literal->token = create_token(allocator, TokenKind::FloatLiteral, "0.5");
    enemy_hitChance_field->initializer = hitChance_literal;
    
    // Enemy constructor
    auto position_assign_ctor = allocator.alloc<AssignmentExpressionNode>();
    position_assign_ctor->opKind = AssignmentOperatorKind::Assign;
    position_assign_ctor->target = allocator.alloc<IdentifierExpressionNode>();
    ((IdentifierExpressionNode*)position_assign_ctor->target)->identifier = create_identifier(allocator, "position");
    position_assign_ctor->source = allocator.alloc<IdentifierExpressionNode>();
    ((IdentifierExpressionNode*)position_assign_ctor->source)->identifier = create_identifier(allocator, "startPos");
    auto position_assign_stmt = allocator.alloc<ExpressionStatementNode>();
    position_assign_stmt->expression = position_assign_ctor;
    
    auto attack_assign_ctor = allocator.alloc<AssignmentExpressionNode>();
    attack_assign_ctor->opKind = AssignmentOperatorKind::Assign;
    attack_assign_ctor->target = allocator.alloc<IdentifierExpressionNode>();
    ((IdentifierExpressionNode*)attack_assign_ctor->target)->identifier = create_identifier(allocator, "attack");
    attack_assign_ctor->source = allocator.alloc<IdentifierExpressionNode>();
    ((IdentifierExpressionNode*)attack_assign_ctor->source)->identifier = create_identifier(allocator, "damage");
    auto attack_assign_stmt = allocator.alloc<ExpressionStatementNode>();
    attack_assign_stmt->expression = attack_assign_ctor;
    
    auto enemy_ctor_body = allocator.alloc<BlockStatementNode>();
    enemy_ctor_body->statements = create_sized_array<StatementNode*>(allocator, {position_assign_stmt, attack_assign_stmt});
    
    auto enemy_ctor = allocator.alloc<FunctionDeclarationNode>();
    enemy_ctor->name = create_identifier(allocator, "constructor");
    enemy_ctor->returnType = create_type_name(allocator, "void"); // Constructor return type
    auto ctor_param1 = allocator.alloc<ParameterNode>();
    ctor_param1->name = create_identifier(allocator, "startPos");
    ctor_param1->type = create_type_name(allocator, "Vector3");
    auto ctor_param2 = allocator.alloc<ParameterNode>();
    ctor_param2->name = create_identifier(allocator, "damage");
    ctor_param2->type = create_type_name(allocator, "i32");
    auto damage_default = allocator.alloc<LiteralExpressionNode>();
    damage_default->kind = LiteralKind::Integer;
    damage_default->token = create_token(allocator, TokenKind::IntegerLiteral, "5");
    ctor_param2->defaultValue = damage_default;
    enemy_ctor->parameters = create_sized_array<ParameterNode*>(allocator, {ctor_param1, ctor_param2});
    enemy_ctor->body = enemy_ctor_body;
    
    // Enemy.GetDamage method
    auto random_chance_call = allocator.alloc<MemberAccessExpressionNode>();
    random_chance_call->target = allocator.alloc<IdentifierExpressionNode>();
    ((IdentifierExpressionNode*)random_chance_call->target)->identifier = create_identifier(allocator, "Random");
    random_chance_call->member = create_identifier(allocator, "Chance");
    auto chance_invocation = allocator.alloc<CallExpressionNode>();
    chance_invocation->target = random_chance_call;
    auto hitChance_arg = allocator.alloc<IdentifierExpressionNode>();
    hitChance_arg->identifier = create_identifier(allocator, "hitChance");
    chance_invocation->arguments = create_sized_array<ExpressionNode*>(allocator, {hitChance_arg});
    
    // Since there's no TernaryExpressionNode, we'll create an if-else structure instead
    auto attack_expr = allocator.alloc<IdentifierExpressionNode>();
    attack_expr->identifier = create_identifier(allocator, "attack");
    auto return_attack = allocator.alloc<ReturnStatementNode>();
    return_attack->expression = attack_expr;
    
    auto zero_expr = allocator.alloc<LiteralExpressionNode>();
    zero_expr->kind = LiteralKind::Integer;
    zero_expr->token = create_token(allocator, TokenKind::IntegerLiteral, "0");
    auto return_zero = allocator.alloc<ReturnStatementNode>();
    return_zero->expression = zero_expr;
    
    auto if_stmt = allocator.alloc<IfStatementNode>();
    if_stmt->condition = chance_invocation;
    if_stmt->thenStatement = return_attack;
    if_stmt->elseStatement = return_zero;
    
    auto getDamage_body = allocator.alloc<BlockStatementNode>();
    getDamage_body->statements = create_sized_array<StatementNode*>(allocator, {if_stmt});
    
    auto enemy_getDamage_method = allocator.alloc<FunctionDeclarationNode>();
    enemy_getDamage_method->name = create_identifier(allocator, "GetDamage");
    enemy_getDamage_method->returnType = create_type_name(allocator, "i32");
    enemy_getDamage_method->modifiers = create_sized_array<ModifierKind>(allocator, {ModifierKind::Public});
    enemy_getDamage_method->body = getDamage_body;
    enemy_getDamage_method->parameters = create_sized_array<ParameterNode*>(allocator, {});
    
    auto enemy_class = allocator.alloc<ClassDeclarationNode>();
    enemy_class->name = create_identifier(allocator, "Enemy");
    enemy_class->modifiers = create_sized_array<ModifierKind>(allocator, {ModifierKind::Reference});
    enemy_class->members = create_sized_array<MemberDeclarationNode*>(allocator, {
        (MemberDeclarationNode*)enemy_position_field,
        (MemberDeclarationNode*)enemy_attack_field,
        (MemberDeclarationNode*)enemy_hitChance_field,
        (MemberDeclarationNode*)enemy_ctor,
        (MemberDeclarationNode*)enemy_getDamage_method
    });
    
    // --- Main function ---
    
    auto running_var_decl = allocator.alloc<VariableDeclarationNode>();
    running_var_decl->name = create_identifier(allocator, "running");
    running_var_decl->type = create_type_name(allocator, "bool");
    running_var_decl->modifiers = create_sized_array<ModifierKind>(allocator, {ModifierKind::Mutable});
    auto true_literal = allocator.alloc<LiteralExpressionNode>();
    true_literal->kind = LiteralKind::Boolean;
    true_literal->token = create_token(allocator, TokenKind::True, "true");
    running_var_decl->initializer = true_literal;
    auto running_decl_stmt = allocator.alloc<LocalVariableDeclarationNode>();
    running_decl_stmt->declarators = create_sized_array<VariableDeclarationNode*>(allocator, {running_var_decl});
    
    auto while_condition = allocator.alloc<IdentifierExpressionNode>();
    while_condition->identifier = create_identifier(allocator, "running");
    auto while_body = allocator.alloc<BlockStatementNode>();
    while_body->statements = create_sized_array<StatementNode*>(allocator, {});
    auto while_stmt = allocator.alloc<WhileStatementNode>();
    while_stmt->condition = while_condition;
    while_stmt->body = while_body;
    
    auto console_log_call = allocator.alloc<MemberAccessExpressionNode>();
    console_log_call->target = allocator.alloc<IdentifierExpressionNode>();
    ((IdentifierExpressionNode*)console_log_call->target)->identifier = create_identifier(allocator, "Console");
    console_log_call->member = create_identifier(allocator, "Log");
    auto log_invocation = allocator.alloc<CallExpressionNode>();
    log_invocation->target = console_log_call;
    auto done_literal = allocator.alloc<LiteralExpressionNode>();
    done_literal->kind = LiteralKind::String;
    done_literal->token = create_token(allocator, TokenKind::StringLiteral, "\"Done\"");
    log_invocation->arguments = create_sized_array<ExpressionNode*>(allocator, {done_literal});
    auto log_call_stmt = allocator.alloc<ExpressionStatementNode>();
    log_call_stmt->expression = log_invocation;
    
    auto main_body = allocator.alloc<BlockStatementNode>();
    main_body->statements = create_sized_array<StatementNode*>(allocator, {running_decl_stmt, while_stmt, log_call_stmt});
    
    auto main_func = allocator.alloc<FunctionDeclarationNode>();
    main_func->name = create_identifier(allocator, "Main");
    main_func->returnType = create_type_name(allocator, "void");
    main_func->body = main_body;
    main_func->parameters = create_sized_array<ParameterNode*>(allocator, {});
    
    // --- Main() function call ---
    
    auto main_call_expr = allocator.alloc<CallExpressionNode>();
    main_call_expr->target = allocator.alloc<IdentifierExpressionNode>();
    ((IdentifierExpressionNode*)main_call_expr->target)->identifier = create_identifier(allocator, "Main");
    main_call_expr->arguments = create_sized_array<ExpressionNode*>(allocator, {});
    auto main_call_stmt = allocator.alloc<ExpressionStatementNode>();
    main_call_stmt->expression = main_call_expr;

    // --- Compilation Unit (Root) ---
    auto compilation_unit = allocator.alloc<CompilationUnitNode>();
    compilation_unit->statements = create_sized_array<StatementNode*>(allocator, {
        (StatementNode*)console_class,
        (StatementNode*)vector3_class,
        (StatementNode*)enemy_class,
        (StatementNode*)main_func,
        (StatementNode*)main_call_stmt
    });

    // 4. Create and run the printer visitor.
    std::cout << "Printing constructed AST:" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    AstPrinterVisitor printer;
    compilation_unit->accept(&printer);
    std::cout << "----------------------------------------\n" << std::endl;

    // 5. Build symbol table from AST
    std::cout << "Building symbol table from AST:" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    SymbolTable symbol_table;
    build_symbol_table(symbol_table, compilation_unit);
    symbol_table.print_symbol_table();
    std::cout << "----------------------------------------\n" << std::endl;

    // 6. Demonstrate symbol table navigation
    std::cout << "Demonstrating symbol table navigation:" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    // Navigate to Console class
    symbol_table.push_scope("Console");
    symbol_table.print_navigation_state();
    
    // Look up a field in Console
    auto messageCount = symbol_table.lookup_symbol("messageCount");
    if (messageCount) {
        std::cout << "Found: " << messageCount->name << " (" << messageCount->data_type << ")" << std::endl;
    }
    
    // Navigate to Enemy class
    symbol_table.reset_navigation();
    symbol_table.push_scope("Enemy");
    symbol_table.print_navigation_state();
    
    // Look up GetDamage function
    auto getDamage = symbol_table.lookup_symbol("GetDamage");
    if (getDamage) {
        std::cout << "Found: " << getDamage->name << " (" << getDamage->data_type << ")" << std::endl;
    }
    
    symbol_table.reset_navigation();
    std::cout << "----------------------------------------\n" << std::endl;

    // 7. Test code generation
    std::cout << "Testing code generation:" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    CodeGenerator code_gen(symbol_table);
    code_gen.generate_code(compilation_unit);
    std::cout << "----------------------------------------\n" << std::endl;

    // 8. Test RTTI on the new AST
    std::cout << "Testing RTTI on new AST:" << std::endl;
    if (main_func->is_a<DeclarationNode>()) {
         std::cout << "OK: FunctionDeclarationNode is a DeclarationNode." << std::endl;
    }
    if (!while_stmt->is_a<ExpressionNode>()) {
        std::cout << "OK: WhileStatementNode is NOT an ExpressionNode." << std::endl;
    }
    auto casted_decl = console_messageCount_field->as<DeclarationNode>();
    if (casted_decl && casted_decl->name) {
        std::cout << "OK: Cast from FieldDeclarationNode to DeclarationNode successful. Name: " << casted_decl->name->name << std::endl;
    } else {
        std::cout << "FAIL: Cast to DeclarationNode failed or name was null." << std::endl;
    }

    return 0;
}