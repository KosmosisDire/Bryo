#include "parser/recursive_parser.hpp"
// Token utilities now in common/token.hpp
#include <algorithm>
#include <cstring> // For memcpy

namespace Mycelium::Scripting::Parser {

// Helper function to create SizedArray from vector
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

RecursiveParser::RecursiveParser(TokenStream& tokens, ParserContext& context, AstAllocator& allocator)
    : ParserBase(tokens, context, allocator), expression_parser_(nullptr) {
}

// Main parsing entry points

ParseResult<CompilationUnitNode*> RecursiveParser::parse_compilation_unit() {
    PARSER_CONTEXT(*this, ParsingContext::Global);
    
    auto unit = create_node<CompilationUnitNode>();
    std::vector<StatementNode*> statements;
    std::vector<ParserDiagnostic> accumulated_errors;
    
    while (!at_end()) {
        auto stmt_result = parse_statement();
        
        if (stmt_result.has_value()) {
            statements.push_back(stmt_result.value());
        }
        
        // Accumulate errors but continue parsing
        if (stmt_result.has_errors()) {
            accumulated_errors.insert(accumulated_errors.end(),
                                    stmt_result.errors().begin(), stmt_result.errors().end());
        }
        
        // If we can't parse a statement, try to recover
        if (!stmt_result.has_value()) {
            synchronize_to_declaration();
            if (!at_end()) {
                advance(); // Skip problematic token
            }
        }
    }
    
    // Convert to SizedArray
    unit->statements = create_sized_array(allocator_, statements);
    
    auto result = ParseResult<CompilationUnitNode*>::success(unit);
    result.add_errors(accumulated_errors);
    return result;
}

ParseResult<StatementNode*> RecursiveParser::parse_statement() {
    // Skip any leading whitespace or comments that might affect parsing
    while (current().kind == TokenKind::EndOfFile) {
        if (at_end()) {
            ParserDiagnostic error(DiagnosticLevel::Error, "Unexpected end of file", current().location);
            return ParseResult<StatementNode*>::error(std::move(error));
        }
    }
    
    switch (current().kind) {
        // Declarations (which are also statements)
        case TokenKind::Fn:
            return parse_function_declaration().cast<StatementNode*>();
        case TokenKind::Type:
            return parse_type_declaration().cast<StatementNode*>();
        case TokenKind::Interface:
            return parse_interface_declaration().cast<StatementNode*>();
        case TokenKind::Enum:
            return parse_enum_declaration().cast<StatementNode*>();
        case TokenKind::Using:
            return parse_using_directive().cast<StatementNode*>();
        case TokenKind::Namespace:
            return parse_namespace_declaration().cast<StatementNode*>();
            
        // Control flow statements
        case TokenKind::If:
            return parse_if_statement().cast<StatementNode*>();
        case TokenKind::While:
            return parse_while_statement().cast<StatementNode*>();
        case TokenKind::For:
            return parse_for_statement().cast<StatementNode*>();
        case TokenKind::Return:
            return parse_return_statement().cast<StatementNode*>();
        case TokenKind::Break:
            return parse_break_statement().cast<StatementNode*>();
        case TokenKind::Continue:
            return parse_continue_statement().cast<StatementNode*>();
            
        // Block statement
        case TokenKind::LeftBrace:
            return parse_block_statement().cast<StatementNode*>();
            
        // Local variable declaration or expression statement
        default:
            if (is_variable_declaration_start()) {
                return parse_local_variable_declaration().cast<StatementNode*>();
            } else if (current().starts_expression()) {
                return parse_expression_statement().cast<StatementNode*>();
            } else {
                ParserDiagnostic error(DiagnosticLevel::Error, 
                                     "Expected statement but found '" + std::string(to_string(current().kind)) + "'",
                                     current().range());
                return ParseResult<StatementNode*>::error(std::move(error));
            }
    }
}

ParseResult<DeclarationNode*> RecursiveParser::parse_declaration() {
    // Parse modifiers first
    auto modifiers_result = parse_modifiers_result();
    std::vector<ModifierKind> modifiers;
    std::vector<ParserDiagnostic> accumulated_errors;
    
    if (modifiers_result.has_value()) {
        modifiers = modifiers_result.value();
    }
    if (modifiers_result.has_errors()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                modifiers_result.errors().begin(), modifiers_result.errors().end());
    }
    
    switch (current().kind) {
        case TokenKind::Fn: {
            auto func_result = parse_function_declaration();
            if (func_result.has_value()) {
                auto func = func_result.value();
                func->modifiers = create_sized_array(allocator_, modifiers);
            }
            if (func_result.has_errors()) {
                accumulated_errors.insert(accumulated_errors.end(),
                                        func_result.errors().begin(), func_result.errors().end());
            }
            auto result = func_result.cast<DeclarationNode*>();
            result.add_errors(accumulated_errors);
            return result;
        }
        
        case TokenKind::Type: {
            auto type_result = parse_type_declaration();
            if (type_result.has_value()) {
                auto type_decl = type_result.value();
                type_decl->modifiers = create_sized_array(allocator_, modifiers);
            }
            if (type_result.has_errors()) {
                accumulated_errors.insert(accumulated_errors.end(),
                                        type_result.errors().begin(), type_result.errors().end());
            }
            auto result = type_result.cast<DeclarationNode*>();
            result.add_errors(accumulated_errors);
            return result;
        }
        
        case TokenKind::Interface: {
            auto interface_result = parse_interface_declaration();
            if (interface_result.has_value()) {
                auto interface_decl = interface_result.value();
                interface_decl->modifiers = create_sized_array(allocator_, modifiers);
            }
            if (interface_result.has_errors()) {
                accumulated_errors.insert(accumulated_errors.end(),
                                        interface_result.errors().begin(), interface_result.errors().end());
            }
            auto result = interface_result.cast<DeclarationNode*>();
            result.add_errors(accumulated_errors);
            return result;
        }
        
        case TokenKind::Enum: {
            auto enum_result = parse_enum_declaration();
            if (enum_result.has_value()) {
                auto enum_decl = enum_result.value();
                enum_decl->modifiers = create_sized_array(allocator_, modifiers);
            }
            if (enum_result.has_errors()) {
                accumulated_errors.insert(accumulated_errors.end(),
                                        enum_result.errors().begin(), enum_result.errors().end());
            }
            auto result = enum_result.cast<DeclarationNode*>();
            result.add_errors(accumulated_errors);
            return result;
        }
        
        default: {
            ParserDiagnostic error(DiagnosticLevel::Error,
                                 "Expected declaration but found '" + std::string(to_string(current().kind)) + "'",
                                 current().range());
            accumulated_errors.push_back(std::move(error));
            return ParseResult<DeclarationNode*>::errors(accumulated_errors);
        }
    }
}

// Declaration parsing methods

ParseResult<FunctionDeclarationNode*> RecursiveParser::parse_function_declaration() {
    PARSER_CONTEXT(*this, ParsingContext::FunctionDeclaration);
    
    auto func = create_node<FunctionDeclarationNode>();
    std::vector<ParserDiagnostic> accumulated_errors;
    
    // Parse 'fn' keyword
    auto fn_result = consume_token(TokenKind::Fn);
    if (!fn_result.has_value()) {
        return ParseResult<FunctionDeclarationNode*>::errors(fn_result.errors());
    }
    func->fnKeyword = create_node<TokenNode>();
    func->fnKeyword->tokenKind = fn_result.value().kind;
    func->fnKeyword->text = fn_result.value().text;
    
    // Parse function name
    auto name_result = parse_identifier_result();
    if (!name_result.has_value()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                name_result.errors().begin(), name_result.errors().end());
        // Try to recover
        if (current().kind == TokenKind::LeftParen) {
            // Create dummy name for recovery
            func->name = create_node<IdentifierNode>();
            func->name->name = "<missing>";
        } else {
            return ParseResult<FunctionDeclarationNode*>::errors(accumulated_errors);
        }
    } else {
        func->name = name_result.value();
        if (name_result.has_errors()) {
            accumulated_errors.insert(accumulated_errors.end(),
                                    name_result.errors().begin(), name_result.errors().end());
        }
    }
    
    // Parse opening parenthesis
    auto open_paren_result = consume_token(TokenKind::LeftParen);
    if (!open_paren_result.has_value()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                open_paren_result.errors().begin(), open_paren_result.errors().end());
        return ParseResult<FunctionDeclarationNode*>::errors(accumulated_errors);
    }
    func->openParen = create_node<TokenNode>();
    func->openParen->tokenKind = open_paren_result.value().kind;
    func->openParen->text = open_paren_result.value().text;
    
    // Parse parameter list (just the parameters, not the parentheses)
    auto params_result = parse_parameter_list_only();
    std::vector<ParameterNode*> params;
    if (params_result.has_value()) {
        params = params_result.value();
    }
    if (params_result.has_errors()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                params_result.errors().begin(), params_result.errors().end());
    }
    func->parameters = create_sized_array(allocator_, params);
    
    // Parse closing parenthesis
    auto close_paren_result = consume_token(TokenKind::RightParen);
    if (!close_paren_result.has_value()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                close_paren_result.errors().begin(), close_paren_result.errors().end());
    } else {
        func->closeParen = create_node<TokenNode>();
        func->closeParen->tokenKind = close_paren_result.value().kind;
        func->closeParen->text = close_paren_result.value().text;
    }
    
    // Parse optional return type (-> Type)
    if (current().kind == TokenKind::Arrow) {
        auto arrow_result = consume_token(TokenKind::Arrow);
        if (arrow_result.has_value()) {
            func->arrow = create_node<TokenNode>();
            func->arrow->tokenKind = arrow_result.value().kind;
            func->arrow->text = arrow_result.value().text;
            
            auto return_type_result = parse_type_name_result();
            if (return_type_result.has_value()) {
                func->returnType = return_type_result.value();
            } else {
                accumulated_errors.insert(accumulated_errors.end(),
                                        return_type_result.errors().begin(), return_type_result.errors().end());
            }
        }
    }
    
    // Parse function body or semicolon for abstract functions
    if (current().kind == TokenKind::LeftBrace) {
        auto body_result = parse_block_statement();
        if (body_result.has_value()) {
            func->body = body_result.value();
        } else {
            accumulated_errors.insert(accumulated_errors.end(),
                                    body_result.errors().begin(), body_result.errors().end());
        }
    } else if (current().kind == TokenKind::Semicolon) {
        auto semicolon_result = consume_token(TokenKind::Semicolon);
        if (semicolon_result.has_value()) {
            func->semicolon = create_node<TokenNode>();
            func->semicolon->tokenKind = semicolon_result.value().kind;
            func->semicolon->text = semicolon_result.value().text;
        }
    } else {
        ParserDiagnostic error(DiagnosticLevel::Error,
                             "Expected function body '{' or semicolon ';' for abstract function",
                             current().range());
        accumulated_errors.push_back(std::move(error));
    }
    
    auto result = ParseResult<FunctionDeclarationNode*>::success(func);
    result.add_errors(accumulated_errors);
    return result;
}

ParseResult<TypeDeclarationNode*> RecursiveParser::parse_type_declaration() {
    PARSER_CONTEXT(*this, ParsingContext::TypeDeclaration);
    
    auto type_decl = create_node<TypeDeclarationNode>();
    std::vector<ParserDiagnostic> accumulated_errors;
    
    // Parse 'type' keyword
    auto type_result = consume_token(TokenKind::Type);
    if (!type_result.has_value()) {
        return ParseResult<TypeDeclarationNode*>::errors(type_result.errors());
    }
    type_decl->typeKeyword = create_node<TokenNode>();
    type_decl->typeKeyword->tokenKind = type_result.value().kind;
    type_decl->typeKeyword->text = type_result.value().text;
    
    // Parse type name
    auto name_result = parse_identifier_result();
    if (!name_result.has_value()) {
        return ParseResult<TypeDeclarationNode*>::errors(name_result.errors());
    }
    type_decl->name = name_result.value();
    
    // Parse opening brace
    auto open_brace_result = consume_token(TokenKind::LeftBrace);
    if (!open_brace_result.has_value()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                open_brace_result.errors().begin(), open_brace_result.errors().end());
        return ParseResult<TypeDeclarationNode*>::errors(accumulated_errors);
    }
    type_decl->openBrace = create_node<TokenNode>();
    type_decl->openBrace->tokenKind = open_brace_result.value().kind;
    type_decl->openBrace->text = open_brace_result.value().text;
    
    // Parse member declarations
    auto members_result = parse_member_declaration_list();
    if (members_result.has_value()) {
        auto members = members_result.value();
        type_decl->members = create_sized_array(allocator_, members);
    }
    if (members_result.has_errors()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                members_result.errors().begin(), members_result.errors().end());
    }
    
    // Parse closing brace
    auto close_brace_result = consume_token(TokenKind::RightBrace);
    if (!close_brace_result.has_value()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                close_brace_result.errors().begin(), close_brace_result.errors().end());
    } else {
        type_decl->closeBrace = create_node<TokenNode>();
        type_decl->closeBrace->tokenKind = close_brace_result.value().kind;
        type_decl->closeBrace->text = close_brace_result.value().text;
    }
    
    auto result = ParseResult<TypeDeclarationNode*>::success(type_decl);
    result.add_errors(accumulated_errors);
    return result;
}

// Expression parsing delegation

ParseResult<ExpressionNode*> RecursiveParser::parse_expression(int min_precedence) {
    if (!expression_parser_) {
        ParserDiagnostic error(DiagnosticLevel::Error,
                             "Expression parser not set - cannot parse expressions",
                             current().range());
        return ParseResult<ExpressionNode*>::error(std::move(error));
    }
    
    // Delegate to the Pratt parser
    auto expr = expression_parser_->parse_expression(min_precedence);
    if (expr) {
        return ParseResult<ExpressionNode*>::success(expr);
    } else {
        ParserDiagnostic error(DiagnosticLevel::Error,
                             "Failed to parse expression",
                             current().range());
        return ParseResult<ExpressionNode*>::error(std::move(error));
    }
}

// Statement parsing methods

ParseResult<BlockStatementNode*> RecursiveParser::parse_block_statement() {
    PARSER_CONTEXT(*this, ParsingContext::BlockStatement);
    
    auto block = create_node<BlockStatementNode>();
    std::vector<ParserDiagnostic> accumulated_errors;
    
    // Parse opening brace
    auto open_brace_result = consume_token(TokenKind::LeftBrace);
    if (!open_brace_result.has_value()) {
        return ParseResult<BlockStatementNode*>::errors(open_brace_result.errors());
    }
    block->openBrace = create_node<TokenNode>();
    block->openBrace->tokenKind = open_brace_result.value().kind;
    block->openBrace->text = open_brace_result.value().text;
    
    // Parse statements
    auto statements_result = parse_statement_list();
    if (statements_result.has_value()) {
        auto statements = statements_result.value();
        block->statements = create_sized_array(allocator_, statements);
    }
    if (statements_result.has_errors()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                statements_result.errors().begin(), statements_result.errors().end());
    }
    
    // Parse closing brace
    auto close_brace_result = consume_token(TokenKind::RightBrace);
    if (!close_brace_result.has_value()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                close_brace_result.errors().begin(), close_brace_result.errors().end());
    } else {
        block->closeBrace = create_node<TokenNode>();
        block->closeBrace->tokenKind = close_brace_result.value().kind;
        block->closeBrace->text = close_brace_result.value().text;
    }
    
    auto result = ParseResult<BlockStatementNode*>::success(block);
    result.add_errors(accumulated_errors);
    return result;
}

ParseResult<IfStatementNode*> RecursiveParser::parse_if_statement() {
    PARSER_CONTEXT(*this, ParsingContext::IfStatement);
    
    auto if_stmt = create_node<IfStatementNode>();
    std::vector<ParserDiagnostic> accumulated_errors;
    
    // Parse 'if' keyword
    auto if_result = consume_token(TokenKind::If);
    if (!if_result.has_value()) {
        return ParseResult<IfStatementNode*>::errors(if_result.errors());
    }
    if_stmt->ifKeyword = create_node<TokenNode>();
    if_stmt->ifKeyword->tokenKind = if_result.value().kind;
    if_stmt->ifKeyword->text = if_result.value().text;
    
    // Parse opening parenthesis
    auto open_paren_result = consume_token(TokenKind::LeftParen);
    if (!open_paren_result.has_value()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                open_paren_result.errors().begin(), open_paren_result.errors().end());
    } else {
        if_stmt->openParen = create_node<TokenNode>();
        if_stmt->openParen->tokenKind = open_paren_result.value().kind;
        if_stmt->openParen->text = open_paren_result.value().text;
    }
    
    // Parse condition expression
    auto condition_result = parse_expression();
    if (!condition_result.has_value()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                condition_result.errors().begin(), condition_result.errors().end());
    } else {
        if_stmt->condition = condition_result.value();
        if (condition_result.has_errors()) {
            accumulated_errors.insert(accumulated_errors.end(),
                                    condition_result.errors().begin(), condition_result.errors().end());
        }
    }
    
    // Parse closing parenthesis
    auto close_paren_result = consume_token(TokenKind::RightParen);
    if (!close_paren_result.has_value()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                close_paren_result.errors().begin(), close_paren_result.errors().end());
    } else {
        if_stmt->closeParen = create_node<TokenNode>();
        if_stmt->closeParen->tokenKind = close_paren_result.value().kind;
        if_stmt->closeParen->text = close_paren_result.value().text;
    }
    
    // Parse then statement
    auto then_result = parse_statement();
    if (!then_result.has_value()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                then_result.errors().begin(), then_result.errors().end());
    } else {
        if_stmt->thenStatement = then_result.value();
        if (then_result.has_errors()) {
            accumulated_errors.insert(accumulated_errors.end(),
                                    then_result.errors().begin(), then_result.errors().end());
        }
    }
    
    // Parse optional else clause
    if (current().kind == TokenKind::Else) {
        auto else_result = consume_token(TokenKind::Else);
        if (else_result.has_value()) {
            if_stmt->elseKeyword = create_node<TokenNode>();
            if_stmt->elseKeyword->tokenKind = else_result.value().kind;
            if_stmt->elseKeyword->text = else_result.value().text;
            
            auto else_stmt_result = parse_statement();
            if (else_stmt_result.has_value()) {
                if_stmt->elseStatement = else_stmt_result.value();
            } else {
                accumulated_errors.insert(accumulated_errors.end(),
                                        else_stmt_result.errors().begin(), else_stmt_result.errors().end());
            }
        }
    }
    
    auto result = ParseResult<IfStatementNode*>::success(if_stmt);
    result.add_errors(accumulated_errors);
    return result;
}

// Utility methods

bool RecursiveParser::is_variable_declaration_start() const {
    // Check for patterns like:
    // identifier ':'  => x: int = 5
    // identifier '='  => x = 5  (type inference)
    if (current().kind == TokenKind::Identifier) {
        Token next = peek(1);
        return next.kind == TokenKind::Colon || next.kind == TokenKind::Assign;
    }
    return false;
}

bool RecursiveParser::is_function_declaration_start() const {
    return current().kind == TokenKind::Fn;
}

bool RecursiveParser::is_type_declaration_start() const {
    return current().kind == TokenKind::Type;
}

bool RecursiveParser::is_property_declaration_start() const {
    // Check for pattern: identifier ':' 'prop'
    if (current().kind == TokenKind::Identifier) {
        Token next1 = peek(1);
        Token next2 = peek(2);
        return next1.kind == TokenKind::Colon && next2.kind == TokenKind::Prop;
    }
    return false;
}

// Helper methods

ParseResult<std::vector<ModifierKind>> RecursiveParser::parse_modifiers_result() {
    std::vector<ModifierKind> modifiers;
    std::vector<ParserDiagnostic> accumulated_errors;
    
    while (current().is_modifier()) {
        ModifierKind modifier = current().to_modifier_kind();
        modifiers.push_back(modifier);
        advance();
    }
    
    auto result = ParseResult<std::vector<ModifierKind>>::success(std::move(modifiers));
    result.add_errors(accumulated_errors);
    return result;
}

// Error recovery helpers

void RecursiveParser::synchronize_to_declaration() {
    synchronize_to({
        TokenKind::Fn,
        TokenKind::Type,
        TokenKind::Interface,
        TokenKind::Enum,
        TokenKind::Using,
        TokenKind::Namespace,
        TokenKind::RightBrace,
        TokenKind::EndOfFile
    });
}

void RecursiveParser::synchronize_to_statement() {
    synchronize_to({
        TokenKind::If,
        TokenKind::While,
        TokenKind::For,
        TokenKind::Return,
        TokenKind::Break,
        TokenKind::Continue,
        TokenKind::LeftBrace,
        TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::EndOfFile
    });
}

// Placeholder implementations for remaining methods
// These will be implemented in subsequent iterations

ParseResult<InterfaceDeclarationNode*> RecursiveParser::parse_interface_declaration() {
    ParserDiagnostic error(DiagnosticLevel::Error, "Interface declarations not yet implemented", current().range());
    return ParseResult<InterfaceDeclarationNode*>::error(std::move(error));
}

ParseResult<EnumDeclarationNode*> RecursiveParser::parse_enum_declaration() {
    ParserDiagnostic error(DiagnosticLevel::Error, "Enum declarations not yet implemented", current().range());
    return ParseResult<EnumDeclarationNode*>::error(std::move(error));
}

ParseResult<UsingDirectiveNode*> RecursiveParser::parse_using_directive() {
    ParserDiagnostic error(DiagnosticLevel::Error, "Using directives not yet implemented", current().range());
    return ParseResult<UsingDirectiveNode*>::error(std::move(error));
}

ParseResult<NamespaceDeclarationNode*> RecursiveParser::parse_namespace_declaration() {
    ParserDiagnostic error(DiagnosticLevel::Error, "Namespace declarations not yet implemented", current().range());
    return ParseResult<NamespaceDeclarationNode*>::error(std::move(error));
}

ParseResult<WhileStatementNode*> RecursiveParser::parse_while_statement() {
    ParserDiagnostic error(DiagnosticLevel::Error, "While statements not yet implemented", current().range());
    return ParseResult<WhileStatementNode*>::error(std::move(error));
}

ParseResult<ForStatementNode*> RecursiveParser::parse_for_statement() {
    ParserDiagnostic error(DiagnosticLevel::Error, "For statements not yet implemented", current().range());
    return ParseResult<ForStatementNode*>::error(std::move(error));
}

ParseResult<ReturnStatementNode*> RecursiveParser::parse_return_statement() {
    ParserDiagnostic error(DiagnosticLevel::Error, "Return statements not yet implemented", current().range());
    return ParseResult<ReturnStatementNode*>::error(std::move(error));
}

ParseResult<BreakStatementNode*> RecursiveParser::parse_break_statement() {
    ParserDiagnostic error(DiagnosticLevel::Error, "Break statements not yet implemented", current().range());
    return ParseResult<BreakStatementNode*>::error(std::move(error));
}

ParseResult<ContinueStatementNode*> RecursiveParser::parse_continue_statement() {
    ParserDiagnostic error(DiagnosticLevel::Error, "Continue statements not yet implemented", current().range());
    return ParseResult<ContinueStatementNode*>::error(std::move(error));
}

ParseResult<ExpressionStatementNode*> RecursiveParser::parse_expression_statement() {
    ParserDiagnostic error(DiagnosticLevel::Error, "Expression statements not yet implemented", current().range());
    return ParseResult<ExpressionStatementNode*>::error(std::move(error));
}

ParseResult<LocalVariableDeclarationNode*> RecursiveParser::parse_local_variable_declaration() {
    ParserDiagnostic error(DiagnosticLevel::Error, "Local variable declarations not yet implemented", current().range());
    return ParseResult<LocalVariableDeclarationNode*>::error(std::move(error));
}

ParseResult<ParameterNode*> RecursiveParser::parse_parameter() {
    auto param = create_node<ParameterNode>();
    std::vector<ParserDiagnostic> accumulated_errors;
    
    // Parse parameter name (identifier)
    auto name_result = parse_identifier_result();
    if (!name_result.has_value()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                name_result.errors().begin(), name_result.errors().end());
        return ParseResult<ParameterNode*>::errors(accumulated_errors);
    }
    param->name = name_result.value();
    
    // Parse colon
    auto colon_result = consume_token(TokenKind::Colon);
    if (!colon_result.has_value()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                colon_result.errors().begin(), colon_result.errors().end());
        return ParseResult<ParameterNode*>::errors(accumulated_errors);
    }
    
    // Parse parameter type
    auto type_result = parse_type_name_result();
    if (!type_result.has_value()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                type_result.errors().begin(), type_result.errors().end());
        return ParseResult<ParameterNode*>::errors(accumulated_errors);
    }
    param->type = type_result.value();
    
    // Parse optional default value (= expression)
    if (current().kind == TokenKind::Assign) {
        auto equals_result = consume_token(TokenKind::Assign);
        if (equals_result.has_value()) {
            param->equalsToken = create_node<TokenNode>();
            param->equalsToken->tokenKind = equals_result.value().kind;
            param->equalsToken->text = equals_result.value().text;
            
            auto default_value_result = parse_expression();
            if (default_value_result.has_value()) {
                param->defaultValue = default_value_result.value();
            } else {
                accumulated_errors.insert(accumulated_errors.end(),
                                        default_value_result.errors().begin(), default_value_result.errors().end());
            }
        }
    }
    
    auto result = ParseResult<ParameterNode*>::success(param);
    result.add_errors(accumulated_errors);
    return result;
}

ParseResult<std::vector<ParameterNode*>> RecursiveParser::parse_parameter_list_only() {
    std::vector<ParameterNode*> params;
    std::vector<ParserDiagnostic> accumulated_errors;
    
    // If we immediately see closing paren, return empty parameter list
    if (current().kind == TokenKind::RightParen) {
        return ParseResult<std::vector<ParameterNode*>>::success(std::move(params));
    }
    
    // Parse first parameter
    auto first_param_result = parse_parameter();
    if (first_param_result.has_value()) {
        params.push_back(first_param_result.value());
    } else {
        accumulated_errors.insert(accumulated_errors.end(),
                                first_param_result.errors().begin(), first_param_result.errors().end());
    }
    
    // Parse additional parameters (comma-separated)
    while (current().kind == TokenKind::Comma) {
        auto comma_result = consume_token(TokenKind::Comma);
        if (comma_result.has_errors()) {
            accumulated_errors.insert(accumulated_errors.end(),
                                    comma_result.errors().begin(), comma_result.errors().end());
        }
        
        auto param_result = parse_parameter();
        if (param_result.has_value()) {
            params.push_back(param_result.value());
        } else {
            accumulated_errors.insert(accumulated_errors.end(),
                                    param_result.errors().begin(), param_result.errors().end());
            // Try to recover by continuing to next comma or closing paren
            synchronize_to({TokenKind::Comma, TokenKind::RightParen});
        }
    }
    
    auto result = ParseResult<std::vector<ParameterNode*>>::success(std::move(params));
    result.add_errors(accumulated_errors);
    return result;
}

ParseResult<std::vector<ParameterNode*>> RecursiveParser::parse_parameter_list_result() {
    std::vector<ParameterNode*> params;
    std::vector<ParserDiagnostic> accumulated_errors;
    
    // Parse opening parenthesis
    auto open_paren_result = consume_token(TokenKind::LeftParen);
    if (!open_paren_result.has_value()) {
        return ParseResult<std::vector<ParameterNode*>>::errors(open_paren_result.errors());
    }
    
    // If we immediately see closing paren, return empty parameter list
    if (current().kind == TokenKind::RightParen) {
        auto close_paren_result = consume_token(TokenKind::RightParen);
        if (!close_paren_result.has_value()) {
            accumulated_errors.insert(accumulated_errors.end(),
                                    close_paren_result.errors().begin(), close_paren_result.errors().end());
        }
        
        auto result = ParseResult<std::vector<ParameterNode*>>::success(std::move(params));
        result.add_errors(accumulated_errors);
        return result;
    }
    
    // Parse first parameter
    auto first_param_result = parse_parameter();
    if (first_param_result.has_value()) {
        params.push_back(first_param_result.value());
    } else {
        accumulated_errors.insert(accumulated_errors.end(),
                                first_param_result.errors().begin(), first_param_result.errors().end());
    }
    
    // Parse additional parameters (comma-separated)
    while (current().kind == TokenKind::Comma) {
        auto comma_result = consume_token(TokenKind::Comma);
        if (comma_result.has_errors()) {
            accumulated_errors.insert(accumulated_errors.end(),
                                    comma_result.errors().begin(), comma_result.errors().end());
        }
        
        auto param_result = parse_parameter();
        if (param_result.has_value()) {
            params.push_back(param_result.value());
        } else {
            accumulated_errors.insert(accumulated_errors.end(),
                                    param_result.errors().begin(), param_result.errors().end());
            // Try to recover by continuing to next comma or closing paren
            synchronize_to({TokenKind::Comma, TokenKind::RightParen});
        }
    }
    
    // Parse closing parenthesis
    auto close_paren_result = consume_token(TokenKind::RightParen);
    if (!close_paren_result.has_value()) {
        accumulated_errors.insert(accumulated_errors.end(),
                                close_paren_result.errors().begin(), close_paren_result.errors().end());
    }
    
    auto result = ParseResult<std::vector<ParameterNode*>>::success(std::move(params));
    result.add_errors(accumulated_errors);
    return result;
}

ParseResult<std::vector<MemberDeclarationNode*>> RecursiveParser::parse_member_declaration_list() {
    std::vector<MemberDeclarationNode*> members;
    return ParseResult<std::vector<MemberDeclarationNode*>>::success(std::move(members));
}

ParseResult<std::vector<StatementNode*>> RecursiveParser::parse_statement_list() {
    std::vector<StatementNode*> statements;
    std::vector<ParserDiagnostic> accumulated_errors;
    
    while (!at_end() && current().kind != TokenKind::RightBrace) {
        auto stmt_result = parse_statement();
        
        if (stmt_result.has_value()) {
            statements.push_back(stmt_result.value());
        }
        
        if (stmt_result.has_errors()) {
            accumulated_errors.insert(accumulated_errors.end(),
                                    stmt_result.errors().begin(), stmt_result.errors().end());
        }
        
        // If we can't parse a statement, try to recover
        if (!stmt_result.has_value()) {
            synchronize_to_statement();
            if (!at_end() && current().kind != TokenKind::RightBrace) {
                advance(); // Skip problematic token
            }
        }
    }
    
    auto result = ParseResult<std::vector<StatementNode*>>::success(std::move(statements));
    result.add_errors(accumulated_errors);
    return result;
}

} // namespace Mycelium::Scripting::Parser