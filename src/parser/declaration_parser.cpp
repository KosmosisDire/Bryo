#include "parser/declaration_parser.h"
#include "parser/parser.h"
#include "parser/statement_parser.h"
#include "parser/expression_parser.h"
#include <vector>
#include <iostream>

namespace Mycelium::Scripting::Lang {

DeclarationParser::DeclarationParser(Parser* parser) : parser_(parser) {
}

// Helper accessors for parser state
ParseContext& DeclarationParser::context() {
    return parser_->get_context();
}

ErrorNode* DeclarationParser::create_error(ErrorKind kind, const char* msg) {
    return parser_->create_error(kind, msg);
}

// Main declaration parsing entry point
ParseResult<DeclarationNode> DeclarationParser::parse_declaration() {
    auto& ctx = context();
    
    // Parse access modifiers and other modifiers first
    std::vector<ModifierKind> modifiers = parse_all_modifiers();
    
    // Determine declaration type by looking at the current token
    // Note: Using directives are handled at top-level, not here
    
    if (ctx.check(TokenKind::Namespace)) {
        return parse_namespace_declaration();
    }
    
    if (ctx.check(TokenKind::Fn)) {
        return parse_function_declaration();
    }
    
    if (ctx.check(TokenKind::Type)) {
        return parse_type_declaration();
    }
    
    if (ctx.check(TokenKind::Enum)) {
        return parse_enum_declaration();
    }
    
    return ParseResult<DeclarationNode>::error(
        create_error(ErrorKind::UnexpectedToken, "Expected declaration"));
}

// Function declaration parsing
ParseResult<DeclarationNode> DeclarationParser::parse_function_declaration() {
    auto& ctx = context();
    
    ctx.advance(); // consume 'fn'
    
    if (!ctx.check(TokenKind::Identifier)) {
        return ParseResult<DeclarationNode>::error(
            create_error(ErrorKind::MissingToken, "Expected function name"));
    }
    
    const Token& name_token = ctx.current();
    ctx.advance();
    
    auto* func_decl = parser_->get_allocator().alloc<FunctionDeclarationNode>();
    func_decl->contains_errors = false;
    
    // Set up name
    auto* name_node = parser_->get_allocator().alloc<IdentifierNode>();
    name_node->name = name_token.text;
    name_node->contains_errors = false;
    func_decl->name = name_node;
    
    // Parse parameter list
    if (!parser_->match(TokenKind::LeftParen)) {
        return ParseResult<DeclarationNode>::error(
            create_error(ErrorKind::MissingToken, "Expected '(' after function name"));
    }
    
    // Parse parameter list and store the parameters
    std::vector<ParameterNode*> params;
    
    // Parse parameter list: param1: Type1, param2: Type2, ...
    while (!ctx.check(TokenKind::RightParen) && !ctx.at_end()) {
        auto param_result = parse_parameter();
        if (param_result.is_success()) {
            params.push_back(param_result.get_node());
        } else {
            // Error recovery: skip to next parameter or end
            parser_->get_recovery().recover_to_safe_point(ctx);
            func_decl->contains_errors = true;
            if (ctx.check(TokenKind::RightParen)) break;
        }
        
        // Check for comma separator
        if (ctx.check(TokenKind::Comma)) {
            ctx.advance();
        } else if (!ctx.check(TokenKind::RightParen)) {
            create_error(ErrorKind::MissingToken, "Expected ',' or ')' in parameter list");
            func_decl->contains_errors = true;
            break;
        }
    }
    
    // Store parameters in the function declaration
    if (!params.empty()) {
        auto* param_array = parser_->get_allocator().alloc_array<AstNode*>(params.size());
        for (size_t i = 0; i < params.size(); ++i) {
            param_array[i] = params[i];
        }
        func_decl->parameters.values = param_array;
        func_decl->parameters.size = static_cast<int>(params.size());
    }
    
    if (!parser_->match(TokenKind::RightParen)) {
        create_error(ErrorKind::MissingToken, "Expected ')' after parameters");
        func_decl->contains_errors = true;
    }
    
    // Parse optional return type: fn test(): i32
    if (parser_->match(TokenKind::Colon)) {
        auto return_type_result = parser_->parse_type_expression();
        if (return_type_result.is_success()) {
            func_decl->returnType = return_type_result.get_node();
        } else {
            func_decl->contains_errors = true;
        }
    }
    
    // Parse function body
    if (ctx.check(TokenKind::LeftBrace)) {
        // Enter function context for parsing body
        auto function_guard = ctx.save_context();
        ctx.set_function_context(true);
        
        auto body_result = parser_->get_statement_parser().parse_block_statement();
        if (body_result.is_success()) {
            func_decl->body = static_cast<BlockStatementNode*>(body_result.get_node());
        } else {
            func_decl->contains_errors = true;
        }
    } else {
        // Missing body - create error but continue
        create_error(ErrorKind::MissingToken, "Expected '{' for function body");
        func_decl->contains_errors = true;
    }
    
    return ParseResult<DeclarationNode>::success(func_decl);
}

// Type declaration parsing
ParseResult<DeclarationNode> DeclarationParser::parse_type_declaration() {
    auto& ctx = context();
    
    // Store the type keyword token
    auto* type_keyword = parser_->get_allocator().alloc<TokenNode>();
    type_keyword->text = ctx.current().text;
    type_keyword->tokenKind = TokenKind::Type;
    type_keyword->contains_errors = false;
    
    ctx.advance(); // consume 'type'
    
    if (!ctx.check(TokenKind::Identifier)) {
        return ParseResult<DeclarationNode>::error(
            create_error(ErrorKind::MissingToken, "Expected type name"));
    }
    
    const Token& name_token = ctx.current();
    ctx.advance();
    
    auto* type_decl = parser_->get_allocator().alloc<TypeDeclarationNode>();
    type_decl->contains_errors = false;
    type_decl->typeKeyword = type_keyword;
    
    // Set up name
    auto* name_node = parser_->get_allocator().alloc<IdentifierNode>();
    name_node->name = name_token.text;
    name_node->contains_errors = false;
    type_decl->name = name_node;
    
    // Parse type body
    if (!parser_->match(TokenKind::LeftBrace)) {
        return ParseResult<DeclarationNode>::error(
            create_error(ErrorKind::MissingToken, "Expected '{' for type body"));
    }
    
    // Store the opening brace
    auto* open_brace = parser_->get_allocator().alloc<TokenNode>();
    open_brace->text = "{";
    open_brace->tokenKind = TokenKind::LeftBrace;
    open_brace->contains_errors = false;
    type_decl->openBrace = open_brace;
    
    std::vector<AstNode*> members;
    
    while (!ctx.check(TokenKind::RightBrace) && !ctx.at_end()) {
        auto member_result = parse_type_member();
        
        if (member_result.is_success()) {
            members.push_back(member_result.get_node());
        } else if (member_result.is_error()) {
            // Add error node and continue
            members.push_back(member_result.get_error());
            type_decl->contains_errors = true;
            
            // Simple recovery: skip to next member or end
            parser_->get_recovery().recover_to_safe_point(ctx);
        } else {
            // Fatal error
            break;
        }
    }
    
    if (!parser_->match(TokenKind::RightBrace)) {
        create_error(ErrorKind::MissingToken, "Expected '}' to close type");
        type_decl->contains_errors = true;
        type_decl->closeBrace = nullptr;
    } else {
        // Store the closing brace
        auto* close_brace = parser_->get_allocator().alloc<TokenNode>();
        close_brace->text = "}";
        close_brace->tokenKind = TokenKind::RightBrace;
        close_brace->contains_errors = false;
        type_decl->closeBrace = close_brace;
    }
    
    // Allocate member array
    if (!members.empty()) {
        auto* member_array = parser_->get_allocator().alloc_array<AstNode*>(members.size());
        for (size_t i = 0; i < members.size(); ++i) {
            member_array[i] = members[i];
        }
        type_decl->members.values = member_array;
        type_decl->members.size = static_cast<int>(members.size());
    }
    
    return ParseResult<DeclarationNode>::success(type_decl);
}

// Enum declaration parsing
ParseResult<DeclarationNode> DeclarationParser::parse_enum_declaration() {
    auto& ctx = context();
    
    ctx.advance(); // consume 'enum'
    
    if (!ctx.check(TokenKind::Identifier)) {
        return ParseResult<DeclarationNode>::error(
            create_error(ErrorKind::MissingToken, "Expected enum name"));
    }
    
    const Token& name_token = ctx.current();
    ctx.advance();
    
    auto* enum_decl = parser_->get_allocator().alloc<EnumDeclarationNode>();
    enum_decl->contains_errors = false;
    
    // Set up name
    auto* name_node = parser_->get_allocator().alloc<IdentifierNode>();
    name_node->name = name_token.text;
    name_node->contains_errors = false;
    enum_decl->name = name_node;
    
    if (!parser_->match(TokenKind::LeftBrace)) {
        return ParseResult<DeclarationNode>::error(
            create_error(ErrorKind::MissingToken, "Expected '{' for enum body"));
    }
    
    std::vector<EnumCaseNode*> cases;
    
    while (!ctx.check(TokenKind::RightBrace) && !ctx.at_end()) {
        auto case_result = parse_enum_case();
        
        if (case_result.is_success()) {
            cases.push_back(case_result.get_node());
        } else {
            enum_decl->contains_errors = true;
            // Simple recovery: skip to next case or end
            parser_->get_recovery().recover_to_safe_point(ctx);
            if (ctx.check(TokenKind::RightBrace)) break;
        }
        
        // Optional comma between cases
        if (ctx.check(TokenKind::Comma)) {
            ctx.advance();
        }
    }
    
    if (!parser_->match(TokenKind::RightBrace)) {
        create_error(ErrorKind::MissingToken, "Expected '}' to close enum");
        enum_decl->contains_errors = true;
    }
    
    // Allocate cases array
    if (!cases.empty()) {
        auto* case_array = parser_->get_allocator().alloc_array<EnumCaseNode*>(cases.size());
        for (size_t i = 0; i < cases.size(); ++i) {
            case_array[i] = cases[i];
        }
        enum_decl->cases.values = case_array;
        enum_decl->cases.size = static_cast<int>(cases.size());
    }
    
    return ParseResult<DeclarationNode>::success(enum_decl);
}

// Using directive parsing
ParseResult<StatementNode> DeclarationParser::parse_using_directive() {
    auto& ctx = context();
    
    ctx.advance(); // consume 'using'
    
    auto type_result = parser_->parse_type_expression();
    if (!type_result.is_success()) {
        return ParseResult<StatementNode>::error(
            create_error(ErrorKind::MissingToken, "Expected type name after 'using'"));
    }
    
    auto* using_stmt = parser_->get_allocator().alloc<UsingDirectiveNode>();
    using_stmt->namespaceName = type_result.get_node();
    using_stmt->contains_errors = ast_has_errors(type_result.get_node());
    
    parser_->expect(TokenKind::Semicolon, "Expected ';' after using directive");
    
    return ParseResult<StatementNode>::success(using_stmt);
}

// Helper method implementations
ParseResult<AstNode> DeclarationParser::parse_parameter_list() {
    std::vector<ParameterNode*> params;
    auto& ctx = context();
    
    // Parse parameter list: param1: Type1, param2: Type2, ...
    while (!ctx.check(TokenKind::RightParen) && !ctx.at_end()) {
        auto param_result = parse_parameter();
        if (param_result.is_success()) {
            params.push_back(param_result.get_node());
        } else {
            // Error recovery: skip to next parameter or end
            parser_->get_recovery().recover_to_safe_point(ctx);
            if (ctx.check(TokenKind::RightParen)) break;
        }
        
        // Check for comma separator
        if (ctx.check(TokenKind::Comma)) {
            ctx.advance();
        } else if (!ctx.check(TokenKind::RightParen)) {
            create_error(ErrorKind::MissingToken, "Expected ',' or ')' in parameter list");
            break;
        }
    }
    
    // We'll return a wrapper node that contains the parameter list
    // For now, just return success to indicate we processed parameters
    return ParseResult<AstNode>::success(nullptr);
}

ParseResult<AstNode> DeclarationParser::parse_type_member() {
    auto& ctx = context();
    
    // Check for access modifiers first
    std::vector<ModifierKind> modifiers = parse_all_modifiers();
    
    // Handle function declarations as type members
    if (ctx.check(TokenKind::Fn)) {
        auto func_result = parse_function_declaration();
        if (func_result.is_success()) {
            return ParseResult<AstNode>::success(func_result.get_node());
        } else {
            return ParseResult<AstNode>::error(func_result.get_error());
        }
    }
    
    // Handle nested type declarations
    if (ctx.check(TokenKind::Type)) {
        auto type_result = parse_type_declaration();
        if (type_result.is_success()) {
            return ParseResult<AstNode>::success(type_result.get_node());
        } else {
            return ParseResult<AstNode>::error(type_result.get_error());
        }
    }
    
    // Handle nested enum declarations
    if (ctx.check(TokenKind::Enum)) {
        auto enum_result = parse_enum_declaration();
        if (enum_result.is_success()) {
            return ParseResult<AstNode>::success(enum_result.get_node());
        } else {
            return ParseResult<AstNode>::error(enum_result.get_error());
        }
    }
    
    // Handle var declarations: var name = value; or public static var name = value;
    if (ctx.check(TokenKind::Var)) {
        return parse_var_field_declaration(modifiers);
    }
    
    // Otherwise, assume explicit type field declaration
    // Myre syntax: Type name; or public Type name1, name2;
    
    // Use the typed variable declaration parser
    auto typed_result = parse_typed_variable_declaration();
    if (!typed_result.is_success()) {
        return ParseResult<AstNode>::error(typed_result.get_error());
    }
    
    auto* var_decl = typed_result.get_node();
    
    // Handle semicolon
    if (context().check(TokenKind::Semicolon)) {
        auto* semicolon = parser_->get_allocator().alloc<TokenNode>();
        semicolon->tokenKind = TokenKind::Semicolon;
        semicolon->text = context().current().text;
        semicolon->contains_errors = false;
        var_decl->semicolon = semicolon;
        context().advance();
    } else {
        create_error(ErrorKind::MissingToken, "Expected ';' after variable declaration");
        var_decl->contains_errors = true;
    }
    
    // Return VariableDeclarationNode directly - no wrapping needed
    return ParseResult<AstNode>::success(var_decl);
}

ParseResult<EnumCaseNode> DeclarationParser::parse_enum_case() {
    auto& ctx = context();
    
    if (!ctx.check(TokenKind::Identifier)) {
        return ParseResult<EnumCaseNode>::error(
            create_error(ErrorKind::MissingToken, "Expected case name"));
    }
    
    const Token& name_token = ctx.current();
    ctx.advance();
    
    auto* case_node = parser_->get_allocator().alloc<EnumCaseNode>();
    case_node->contains_errors = false;
    
    // Set up name
    auto* name_node = parser_->get_allocator().alloc<IdentifierNode>();
    name_node->name = name_token.text;
    name_node->contains_errors = false;
    case_node->name = name_node;
    
    // Check for associated data: Case(param1: Type1, param2: Type2)
    if (ctx.check(TokenKind::LeftParen)) {
        ctx.advance(); // consume '('
        
        std::vector<ParameterNode*> params;
        
        while (!ctx.check(TokenKind::RightParen) && !ctx.at_end()) {
            auto param_result = parse_enum_parameter();
            if (param_result.is_success()) {
                params.push_back(param_result.get_node());
            } else {
                case_node->contains_errors = true;
                break;
            }
            
            if (ctx.check(TokenKind::Comma)) {
                ctx.advance();
            } else if (!ctx.check(TokenKind::RightParen)) {
                create_error(ErrorKind::MissingToken, "Expected ',' or ')' in parameter list");
                case_node->contains_errors = true;
                break;
            }
        }
        
        if (!parser_->match(TokenKind::RightParen)) {
            create_error(ErrorKind::MissingToken, "Expected ')' after parameters");
            case_node->contains_errors = true;
        }
        
        // Allocate parameter array
        if (!params.empty()) {
            auto* param_array = parser_->get_allocator().alloc_array<ParameterNode*>(params.size());
            for (size_t i = 0; i < params.size(); ++i) {
                param_array[i] = params[i];
            }
            case_node->associatedData.values = param_array;
            case_node->associatedData.size = static_cast<int>(params.size());
        }
    }
    
    return ParseResult<EnumCaseNode>::success(case_node);
}

ParseResult<ParameterNode> DeclarationParser::parse_parameter() {
    auto& ctx = context();
    
    // Myre syntax: Type name (not name: Type)
    // First parse the type
    auto type_result = parser_->parse_type_expression();
    if (!type_result.is_success()) {
        return ParseResult<ParameterNode>::error(
            create_error(ErrorKind::MissingToken, "Expected parameter type"));
    }
    
    // Then parse the parameter name
    if (!ctx.check(TokenKind::Identifier)) {
        return ParseResult<ParameterNode>::error(
            create_error(ErrorKind::MissingToken, "Expected parameter name after type"));
    }
    
    const Token& name_token = ctx.current();
    ctx.advance();
    
    auto* param = parser_->get_allocator().alloc<ParameterNode>();
    param->contains_errors = false;
    
    // Set up name
    auto* name_node = parser_->get_allocator().alloc<IdentifierNode>();
    name_node->name = name_token.text;
    name_node->contains_errors = false;
    param->name = name_node;
    
    // Set up type
    param->type = type_result.get_node();
    
    // Check for default value: = value
    if (parser_->match(TokenKind::Assign)) {
        auto default_result = parser_->get_expression_parser().parse_expression();
        if (default_result.is_success()) {
            param->defaultValue = default_result.get_node();
        } else {
            param->contains_errors = true;
        }
    }
    
    return ParseResult<ParameterNode>::success(param);
}

ParseResult<ParameterNode> DeclarationParser::parse_enum_parameter() {
    auto& ctx = context();
    
    // Parse type first
    auto type_result = parser_->parse_type_expression();
    if (!type_result.is_success()) {
        return ParseResult<ParameterNode>::error(
            create_error(ErrorKind::MissingToken, "Expected parameter type"));
    }
    
    auto* param = parser_->get_allocator().alloc<ParameterNode>();
    param->contains_errors = false;
    param->type = type_result.get_node();
    param->defaultValue = nullptr;  // No default values for enum parameters
    
    // Check if there's a parameter name after the type
    if (ctx.check(TokenKind::Identifier)) {
        // This is the "Type name" syntax: i32 priority, string message
        const Token& name_token = ctx.current();
        ctx.advance();
        
        auto* name_node = parser_->get_allocator().alloc<IdentifierNode>();
        name_node->name = name_token.text;
        name_node->contains_errors = false;
        param->name = name_node;
    } else {
        // This is the "Type only" syntax: i32, i32, i32
        param->name = nullptr;
    }
    
    return ParseResult<ParameterNode>::success(param);
}

// Helper method implementations
std::vector<ModifierKind> DeclarationParser::parse_all_modifiers() {
    std::vector<ModifierKind> modifiers;
    auto& ctx = context();
    
    while (true) {
        if (ctx.check(TokenKind::Public)) {
            modifiers.push_back(ModifierKind::Public);
            ctx.advance();
        } else if (ctx.check(TokenKind::Private)) {
            modifiers.push_back(ModifierKind::Private);
            ctx.advance();
        } else if (ctx.check(TokenKind::Protected)) {
            modifiers.push_back(ModifierKind::Protected);
            ctx.advance();
        } else if (ctx.check(TokenKind::Static)) {
            modifiers.push_back(ModifierKind::Static);
            ctx.advance();
        } else if (ctx.check(TokenKind::Ref)) {
            modifiers.push_back(ModifierKind::Ref);
            ctx.advance();
        } else {
            break;
        }
    }
    
    return modifiers;
}

ModifierKind DeclarationParser::parse_access_modifiers() {
    auto& ctx = context();
    
    if (ctx.check(TokenKind::Public)) {
        ctx.advance();
        return ModifierKind::Public;
    } else if (ctx.check(TokenKind::Private)) {
        ctx.advance();
        return ModifierKind::Private;
    } else if (ctx.check(TokenKind::Protected)) {
        ctx.advance();
        return ModifierKind::Protected;
    }
    
    return ModifierKind::Private; // Default
}

// Placeholder implementations for complex features
ParseResult<DeclarationNode> DeclarationParser::parse_namespace_declaration() {
    return ParseResult<DeclarationNode>::error(
        create_error(ErrorKind::UnexpectedToken, "Namespace declarations not implemented yet"));
}

ParseResult<AstNode> DeclarationParser::parse_generic_parameters() {
    return ParseResult<AstNode>::error(
        create_error(ErrorKind::UnexpectedToken, "Generic parameters not implemented yet"));
}

ParseResult<AstNode> DeclarationParser::parse_generic_constraints() {
    return ParseResult<AstNode>::error(
        create_error(ErrorKind::UnexpectedToken, "Generic constraints not implemented yet"));
}

// Unified variable declaration parsing - handles both "var name = value" syntax
ParseResult<VariableDeclarationNode> DeclarationParser::parse_variable_declaration() {
    auto& ctx = context();
    
    // Store the var keyword token
    auto* var_keyword = parser_->get_allocator().alloc<TokenNode>();
    var_keyword->text = ctx.current().text;
    var_keyword->contains_errors = false;
    
    ctx.advance(); // consume 'var'
    
    // Parse variable names (can be multiple: var x, y, z = 0)
    std::vector<IdentifierNode*> names;
    
    if (!ctx.check(TokenKind::Identifier)) {
        return ParseResult<VariableDeclarationNode>::error(
            create_error(ErrorKind::MissingToken, "Expected identifier after 'var'"));
    }
    
    // Parse first name
    const Token& first_name_token = ctx.current();
    ctx.advance();
    
    auto* first_name = parser_->get_allocator().alloc<IdentifierNode>();
    first_name->name = first_name_token.text;
    first_name->contains_errors = false;
    names.push_back(first_name);
    
    // Parse additional names if comma-separated
    while (ctx.check(TokenKind::Comma)) {
        ctx.advance(); // consume comma
        
        if (!ctx.check(TokenKind::Identifier)) {
            return ParseResult<VariableDeclarationNode>::error(
                create_error(ErrorKind::MissingToken, "Expected identifier after ','"));
        }
        
        const Token& name_token = ctx.current();
        ctx.advance();
        
        auto* name_node = parser_->get_allocator().alloc<IdentifierNode>();
        name_node->name = name_token.text;
        name_node->contains_errors = false;
        names.push_back(name_node);
    }
    
    auto* var_decl = parser_->get_allocator().alloc<VariableDeclarationNode>();
    var_decl->contains_errors = false;
    var_decl->varKeyword = var_keyword;
    var_decl->type = nullptr; // var declarations don't have explicit type
    
    // Set both name and names fields
    var_decl->name = first_name; // For single variable case
    
    // Allocate and populate names array
    auto* names_array = parser_->get_allocator().alloc_array<IdentifierNode*>(names.size());
    for (size_t i = 0; i < names.size(); ++i) {
        names_array[i] = names[i];
    }
    var_decl->names.values = names_array;
    var_decl->names.size = static_cast<int>(names.size());
    
    // Parse optional initializer
    var_decl->equalsToken = nullptr;
    var_decl->initializer = nullptr;
    
    if (ctx.check(TokenKind::Assign)) {
        auto* equals_token = parser_->get_allocator().alloc<TokenNode>();
        equals_token->text = ctx.current().text;
        equals_token->contains_errors = false;
        var_decl->equalsToken = equals_token;
        ctx.advance(); // consume '='
        
        auto init_result = parser_->get_expression_parser().parse_expression();
        if (init_result.is_success()) {
            var_decl->initializer = init_result.get_node();
        } else {
            var_decl->contains_errors = true;
        }
    }
    
    // Update error flag
    if (var_decl->initializer && ast_has_errors(var_decl->initializer)) {
        var_decl->contains_errors = true;
    }
    
    // Note: Semicolon handling is done by the caller
    var_decl->semicolon = nullptr;
    
    return ParseResult<VariableDeclarationNode>::success(var_decl);
}

// Parse typed variable declaration: Type name1, name2 = value
ParseResult<VariableDeclarationNode> DeclarationParser::parse_typed_variable_declaration() {
    auto& ctx = context();
    
    // Parse type first
    auto type_result = parser_->parse_type_expression();
    if (!type_result.is_success()) {
        return ParseResult<VariableDeclarationNode>::error(
            create_error(ErrorKind::MissingToken, "Expected type in variable declaration"));
    }
    
    // Parse variable names (can be multiple: i32 x, y, z;)
    std::vector<IdentifierNode*> names;
    
    if (!ctx.check(TokenKind::Identifier)) {
        return ParseResult<VariableDeclarationNode>::error(
            create_error(ErrorKind::MissingToken, "Expected variable name after type"));
    }
    
    // Parse first name
    const Token& first_name_token = ctx.current();
    ctx.advance();
    
    auto* first_name = parser_->get_allocator().alloc<IdentifierNode>();
    first_name->name = first_name_token.text;
    first_name->contains_errors = false;
    names.push_back(first_name);
    
    // Parse additional names if comma-separated
    while (ctx.check(TokenKind::Comma)) {
        ctx.advance(); // consume comma
        
        if (!ctx.check(TokenKind::Identifier)) {
            return ParseResult<VariableDeclarationNode>::error(
                create_error(ErrorKind::MissingToken, "Expected identifier after ','"));
        }
        
        const Token& name_token = ctx.current();
        ctx.advance();
        
        auto* name_node = parser_->get_allocator().alloc<IdentifierNode>();
        name_node->name = name_token.text;
        name_node->contains_errors = false;
        names.push_back(name_node);
    }
    
    auto* var_decl = parser_->get_allocator().alloc<VariableDeclarationNode>();
    var_decl->contains_errors = false;
    var_decl->varKeyword = nullptr; // No var keyword for typed declarations
    var_decl->type = type_result.get_node();
    
    // Set both name and names fields
    var_decl->name = first_name; // For single variable case
    
    // Allocate and populate names array
    auto* names_array = parser_->get_allocator().alloc_array<IdentifierNode*>(names.size());
    for (size_t i = 0; i < names.size(); ++i) {
        names_array[i] = names[i];
    }
    var_decl->names.values = names_array;
    var_decl->names.size = static_cast<int>(names.size());
    
    // Parse optional initializer
    var_decl->equalsToken = nullptr;
    var_decl->initializer = nullptr;
    
    if (ctx.check(TokenKind::Assign)) {
        auto* equals_token = parser_->get_allocator().alloc<TokenNode>();
        equals_token->text = ctx.current().text;
        equals_token->contains_errors = false;
        var_decl->equalsToken = equals_token;
        ctx.advance(); // consume '='
        
        auto init_result = parser_->get_expression_parser().parse_expression();
        if (init_result.is_success()) {
            var_decl->initializer = init_result.get_node();
        } else {
            var_decl->contains_errors = true;
        }
    }
    
    // Update error flag
    bool has_errors = false;
    if (var_decl->type && ast_has_errors(var_decl->type)) has_errors = true;
    if (var_decl->initializer && ast_has_errors(var_decl->initializer)) has_errors = true;
    var_decl->contains_errors = has_errors;
    
    // Note: Semicolon handling is done by the caller
    var_decl->semicolon = nullptr;
    
    return ParseResult<VariableDeclarationNode>::success(var_decl);
}

// Parse var field declarations: var name = value; or static var name = value;
ParseResult<AstNode> DeclarationParser::parse_var_field_declaration(const std::vector<ModifierKind>& modifiers) {
    // Use the unified variable declaration parser
    auto var_result = parse_variable_declaration();
    if (!var_result.is_success()) {
        return ParseResult<AstNode>::error(var_result.get_error());
    }
    
    auto* var_decl = var_result.get_node();
    
    // For field declarations, initializer is required for var declarations
    if (!var_decl->initializer) {
        return ParseResult<AstNode>::error(
            create_error(ErrorKind::MissingToken, "Expected '=' in var field declaration"));
    }
    
    // Handle semicolon
    if (context().check(TokenKind::Semicolon)) {
        auto* semicolon = parser_->get_allocator().alloc<TokenNode>();
        semicolon->text = context().current().text;
        semicolon->contains_errors = false;
        var_decl->semicolon = semicolon;
        context().advance();
    } else {
        create_error(ErrorKind::MissingToken, "Expected ';' after var field declaration");
        var_decl->contains_errors = true;
    }
    
    // Return the VariableDeclarationNode directly - no wrapping needed
    // var declarations have the same syntax whether local or field
    return ParseResult<AstNode>::success(var_decl);
}

} // namespace Mycelium::Scripting::Lang