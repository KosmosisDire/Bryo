#include "sharpie/parser/script_parser.hpp"
#include "sharpie/script_ast.hpp" // For all AST node types
#include <iostream>
#include <vector>
#include <optional>
#include <functional>

namespace Mycelium::Scripting::Lang
{

    // Implementations of specific parsing rule methods (parse_*) from ScriptParser

    std::shared_ptr<CompilationUnitNode> ScriptParser::parse_compilation_unit()
    {
        SourceLocation file_start_loc = {1, 0, 1, 0, std::string(fileName)};
        if (currentTokenInfo.type != TokenType::EndOfFile)
        {
            file_start_loc = currentTokenInfo.location;
        }
        else
        {
            file_start_loc.lineEnd = file_start_loc.lineStart;
            file_start_loc.columnEnd = file_start_loc.columnStart;
        }
        auto unit_node = make_ast_node<CompilationUnitNode>(file_start_loc);
        while (check_token({TokenType::Using, TokenType::Extern}))
        {
            if (check_token(TokenType::Using))
            {
                std::shared_ptr<UsingDirectiveNode> using_directive = parse_using_directive();
                if (using_directive)
                {
                    unit_node->usings.push_back(using_directive);
                }
                else
                {
                    record_error_at_current("Malformed using directive, skipping.");
                    advance_and_lex();
                }
            }
            else if (check_token(TokenType::Extern))
            {
                std::shared_ptr<ExternalMethodDeclarationNode> extern_decl = parse_external_method_declaration();
                if (extern_decl)
                {
                    unit_node->externs.push_back(extern_decl);
                }
                else
                {
                    record_error_at_current("Malformed extern function declaration, skipping.");
                    advance_and_lex();
                }
            }
        }
        while (currentTokenInfo.type != TokenType::EndOfFile)
        {
            if (check_token(TokenType::Namespace) || check_token(TokenType::Class))
            { // Added other type keywords if necessary
                std::shared_ptr<NamespaceMemberDeclarationNode> member_decl = parse_namespace_member_declaration();
                if (member_decl)
                {
                    unit_node->members.push_back(member_decl);
                }
                else
                {
                    SourceLocation error_loc_check = currentTokenInfo.location;
                    advance_and_lex();
                    if (currentTokenInfo.location.lineStart == error_loc_check.lineStart && currentTokenInfo.location.columnStart == error_loc_check.columnStart && currentTokenInfo.type != TokenType::EndOfFile)
                    {
                        record_error_at_current("Parser stuck at top-level member declaration. Breaking.");
                        break;
                    }
                }
            }
            else if (currentTokenInfo.type != TokenType::EndOfFile)
            {
                record_error_at_current("Unexpected token at top level. Expected namespace or type declaration.");
                advance_and_lex();
            }
        }
        if (!unit_node->usings.empty() || !unit_node->externs.empty() || !unit_node->members.empty())
        {
            finalize_node_location(unit_node);
        }
        else if (currentTokenInfo.type == TokenType::EndOfFile && unit_node->location.has_value())
        {
            unit_node->location.value().lineEnd = currentTokenInfo.location.lineStart;
            unit_node->location.value().columnEnd = currentTokenInfo.location.columnStart;
            if (previousTokenInfo.type != TokenType::Error)
            {
                unit_node->location.value().lineEnd = previousTokenInfo.location.lineEnd;
                unit_node->location.value().columnEnd = previousTokenInfo.location.columnEnd;
            }
        }
        return unit_node;
    }

    std::shared_ptr<UsingDirectiveNode> ScriptParser::parse_using_directive()
    {
        SourceLocation directive_start_loc = currentTokenInfo.location;
        auto using_node = make_ast_node<UsingDirectiveNode>(directive_start_loc);
        using_node->usingKeyword = create_token_node(TokenType::Using, currentTokenInfo);
        consume_token(TokenType::Using, "Expected 'using' keyword.");
        std::shared_ptr<TypeNameNode> parsed_name_as_type = parse_type_name();
        if (parsed_name_as_type)
        {
            if (parsed_name_as_type->is_array() || parsed_name_as_type->openAngleBracketToken.has_value())
            {
                record_error("Namespace name in 'using' directive cannot have array specifiers or generic arguments.", parsed_name_as_type->location.value_or(currentTokenInfo.location));
            }
            if (std::holds_alternative<std::shared_ptr<IdentifierNode>>(parsed_name_as_type->name_segment))
            {
                using_node->namespaceName = std::get<std::shared_ptr<IdentifierNode>>(parsed_name_as_type->name_segment);
            }
            else if (std::holds_alternative<std::shared_ptr<QualifiedNameNode>>(parsed_name_as_type->name_segment))
            {
                using_node->namespaceName = std::get<std::shared_ptr<QualifiedNameNode>>(parsed_name_as_type->name_segment);
            }
            else
            {
                record_error_at_current("Invalid name structure for 'using' directive after parsing type name.");
                auto dummy_ident = make_ast_node<IdentifierNode>(currentTokenInfo.location);
                dummy_ident->name = "_ERROR_USING_NAME_";
                finalize_node_location(dummy_ident);
                using_node->namespaceName = dummy_ident;
            }
        }
        else
        {
            record_error_at_current("Expected namespace name after 'using' keyword.");
            auto dummy_ident = make_ast_node<IdentifierNode>(currentTokenInfo.location);
            dummy_ident->name = "_ERROR_USING_NAME_";
            finalize_node_location(dummy_ident);
            using_node->namespaceName = dummy_ident;
        }
        if (check_token(TokenType::Semicolon))
        {
            using_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_previous("Expected ';' after 'using' directive.");
        }
        finalize_node_location(using_node);
        return using_node;
    }

    std::shared_ptr<NamespaceMemberDeclarationNode> ScriptParser::parse_namespace_member_declaration()
    {
        SourceLocation decl_start_loc = currentTokenInfo.location;
        std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers = parse_modifiers();
        if (!modifiers.empty() && modifiers.front().second && modifiers.front().second->location.has_value())
        {
            decl_start_loc = modifiers.front().second->location.value();
        }
        else
        {
            decl_start_loc = currentTokenInfo.location;
        }
        if (check_token(TokenType::Namespace))
        {
            if (!modifiers.empty())
            {
                record_error("Modifiers are not typically allowed on namespace declarations here.", decl_start_loc);
            }
            return parse_namespace_declaration();
        }
        else if (check_token(TokenType::Class))
        { // Add other type keywords like Struct, Enum
            return parse_type_declaration(decl_start_loc, std::move(modifiers));
        }
        else
        {
            if (!modifiers.empty())
            {
                record_error_at_current("Expected 'namespace' or type keyword (e.g., 'class') after modifiers.");
            }
            else
            {
                record_error_at_current("Expected 'namespace' or type declaration keyword (e.g., 'namespace', 'class').");
            }
            return nullptr;
        }
    }

    std::shared_ptr<NamespaceDeclarationNode> ScriptParser::parse_namespace_declaration()
    {
        SourceLocation namespace_start_loc = currentTokenInfo.location;
        auto ns_node = make_ast_node<NamespaceDeclarationNode>(namespace_start_loc);
        ns_node->namespaceKeyword = create_token_node(TokenType::Namespace, currentTokenInfo);
        consume_token(TokenType::Namespace, "Expected 'namespace' keyword.");

        // Helper function to flatten a TypeNameNode into a qualified string
        auto flatten_name = [](std::shared_ptr<TypeNameNode> name_node) -> std::string
        {
            std::string full_name;
            std::function<void(std::shared_ptr<AstNode>)> build_name =
                [&](std::shared_ptr<AstNode> current_node)
            {
                if (auto qn = std::dynamic_pointer_cast<QualifiedNameNode>(current_node))
                {
                    if (auto tn = std::dynamic_pointer_cast<TypeNameNode>(qn->left))
                    {
                        build_name(tn);
                    }
                    if (!full_name.empty())
                        full_name += ".";
                    full_name += qn->right->name;
                }
                else if (auto tn = std::dynamic_pointer_cast<TypeNameNode>(current_node))
                {
                    if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&tn->name_segment))
                    {
                        if (!full_name.empty())
                            full_name += ".";
                        full_name += (*ident)->name;
                    }
                    else if (auto qn_seg = std::get_if<std::shared_ptr<QualifiedNameNode>>(&tn->name_segment))
                    {
                        build_name(*qn_seg);
                    }
                }
            };

            if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&name_node->name_segment))
            {
                return (*ident)->name;
            }
            else if (auto qn = std::get_if<std::shared_ptr<QualifiedNameNode>>(&name_node->name_segment))
            {
                build_name(*qn);
            }
            return full_name;
        };

        std::shared_ptr<TypeNameNode> parsed_name_holder = parse_type_name(); // TypeNameNode can represent qualified names
        if (parsed_name_holder)
        {
            ns_node->name = make_ast_node<IdentifierNode>(parsed_name_holder->location.value_or(currentTokenInfo.location));
            ns_node->name->name = flatten_name(parsed_name_holder);
            finalize_node_location(ns_node->name);
        }
        else
        {
            record_error_at_current("Expected name for namespace declaration.");
            ns_node->name = make_ast_node<IdentifierNode>(currentTokenInfo.location);
            ns_node->name->name = "_ERROR_NS_NAME_";
            finalize_node_location(ns_node->name);
        }

        if (check_token(TokenType::Semicolon))
        {
            advance_and_lex(); /* File-scoped namespace */
        }
        else if (check_token(TokenType::OpenBrace))
        {
            ns_node->openBraceToken = create_token_node(TokenType::OpenBrace, currentTokenInfo);
            advance_and_lex();
            while (check_token(TokenType::Using))
            {
                std::shared_ptr<UsingDirectiveNode> using_directive = parse_using_directive();
                if (using_directive)
                {
                    ns_node->usings.push_back(using_directive);
                }
                else
                {
                    if (check_token(TokenType::Using))
                    {
                        record_error_at_current("Malformed using directive in namespace, skipping.");
                        advance_and_lex();
                    }
                }
            }
            while (!check_token(TokenType::CloseBrace) && !is_at_end_of_token_stream())
            {
                std::shared_ptr<NamespaceMemberDeclarationNode> member_decl = parse_namespace_member_declaration();
                if (member_decl)
                {
                    ns_node->members.push_back(member_decl);
                }
                else
                {
                    record_error_at_current("Invalid member declaration in namespace. Skipping token.");
                    advance_and_lex();
                }
            }
            if (check_token(TokenType::CloseBrace))
            {
                ns_node->closeBraceToken = create_token_node(TokenType::CloseBrace, currentTokenInfo);
                advance_and_lex();
            }
            else
            {
                record_error_at_previous("Expected '}' to close namespace declaration.");
            }
        }
        else
        {
            record_error_at_current("Expected '{' or ';' after namespace name.");
        }
        finalize_node_location(ns_node);
        return ns_node;
    }

    std::shared_ptr<TypeDeclarationNode> ScriptParser::parse_type_declaration(const SourceLocation &decl_start_loc, std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers)
    {
        if (check_token(TokenType::Class))
        {
            return parse_class_declaration(decl_start_loc, std::move(modifiers));
        }
        // else if (check_token(TokenType::Struct)) { return parse_struct_declaration(...); }
        else
        {
            record_error_at_current("Expected type declaration keyword (e.g., 'class') after modifiers.");
            return nullptr;
        }
    }

    std::shared_ptr<ClassDeclarationNode> ScriptParser::parse_class_declaration(const SourceLocation &decl_start_loc, std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers)
    {
        auto class_node = make_ast_node<ClassDeclarationNode>(decl_start_loc);
        class_node->modifiers = std::move(modifiers);
        std::optional<std::string> previous_class_name_context = m_current_class_name;
        if (check_token(TokenType::Class))
        {
            class_node->typeKeywordToken = create_token_node(TokenType::Class, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected 'class' keyword.");
            finalize_node_location(class_node);
            return class_node;
        }
        if (check_token(TokenType::Identifier))
        {
            class_node->name = create_identifier_node(currentTokenInfo);
            m_current_class_name = class_node->name->name;
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected identifier for class name.");
            class_node->name = make_ast_node<IdentifierNode>(currentTokenInfo.location);
            class_node->name->name = "_ERROR_CLASS_NAME_";
            finalize_node_location(class_node->name);
        }
        // TODO: Parse generic type parameters <T>
        // TODO: Parse base list : BaseClass, IInterface
        if (check_token(TokenType::OpenBrace))
        {
            class_node->openBraceToken = create_token_node(TokenType::OpenBrace, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected '{' to begin class body.");
        }
        while (!check_token(TokenType::CloseBrace) && !is_at_end_of_token_stream())
        {
            std::shared_ptr<MemberDeclarationNode> member = parse_member_declaration();
            if (member)
            {
                class_node->members.push_back(member);
            }
            else
            {
                if (!is_at_end_of_token_stream() && !check_token(TokenType::CloseBrace))
                {
                    record_error_at_current("Invalid or unsupported member declaration in class. Attempting to skip token.");
                    advance_and_lex();
                }
            }
        }
        if (check_token(TokenType::CloseBrace))
        {
            class_node->closeBraceToken = create_token_node(TokenType::CloseBrace, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_previous("Expected '}' to close class declaration.");
        }
        finalize_node_location(class_node);
        m_current_class_name = previous_class_name_context;
        return class_node;
    }

    std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> ScriptParser::parse_modifiers()
    {
        std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> parsed_modifiers;
        bool continue_parsing_modifiers = true;
        while (continue_parsing_modifiers && !is_at_end_of_token_stream())
        {
            ModifierKind kind;
            bool is_modifier_keyword = true;
            if (check_token(TokenType::Public))
            {
                kind = ModifierKind::Public;
            }
            else if (check_token(TokenType::Private))
            {
                kind = ModifierKind::Private;
            }
            else if (check_token(TokenType::Static))
            {
                kind = ModifierKind::Static;
            }
            else if (check_token(TokenType::Readonly))
            {
                kind = ModifierKind::Readonly;
            }
            // Add Protected, Internal etc.
            else
            {
                is_modifier_keyword = false;
                continue_parsing_modifiers = false;
            }
            if (is_modifier_keyword)
            {
                for (const auto &mod_pair : parsed_modifiers)
                {
                    if (mod_pair.first == kind)
                    {
                        record_error_at_current("Duplicate modifier '" + std::string(currentTokenInfo.lexeme) + "'.");
                    }
                }
                parsed_modifiers.push_back({kind, create_token_node(currentTokenInfo.type, currentTokenInfo)});
                advance_and_lex();
            }
        }
        return parsed_modifiers;
    }

    std::shared_ptr<ParameterDeclarationNode> ScriptParser::parse_parameter_declaration()
    {
        SourceLocation param_start_loc = currentTokenInfo.location;
        auto param_node = make_ast_node<ParameterDeclarationNode>(param_start_loc);
        param_node->type = parse_type_name();
        if (!param_node->type)
        {
            record_error_at_current("Expected type name for parameter.");
            auto dummy_type = make_ast_node<TypeNameNode>(currentTokenInfo.location);
            auto dummy_ident_type = make_ast_node<IdentifierNode>(currentTokenInfo.location);
            dummy_ident_type->name = "_ERROR_PARAM_TYPE_";
            finalize_node_location(dummy_ident_type);
            dummy_type->name_segment = dummy_ident_type;
            finalize_node_location(dummy_type);
            param_node->type = dummy_type;
        }
        if (check_token(TokenType::Identifier))
        {
            param_node->name = create_identifier_node(currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected identifier for parameter name.");
            param_node->name = make_ast_node<IdentifierNode>(currentTokenInfo.location);
            param_node->name->name = "_ERROR_PARAM_NAME_";
            finalize_node_location(param_node->name);
        }
        if (check_token(TokenType::Assign))
        {
            param_node->equalsToken = create_token_node(TokenType::Assign, currentTokenInfo);
            advance_and_lex();
            param_node->defaultValue = parse_expression();
            if (!param_node->defaultValue)
            {
                record_error_at_current("Invalid default value expression for parameter.");
            }
        }
        finalize_node_location(param_node);
        return param_node;
    }

    std::optional<std::vector<std::shared_ptr<ParameterDeclarationNode>>> ScriptParser::parse_parameter_list_content(std::vector<std::shared_ptr<TokenNode>> &commas_list)
    {
        std::vector<std::shared_ptr<ParameterDeclarationNode>> parameters;
        commas_list.clear();
        if (check_token(TokenType::CloseParen))
        {
            return parameters;
        }
        std::shared_ptr<ParameterDeclarationNode> first_param = parse_parameter_declaration();
        if (first_param)
        {
            parameters.push_back(first_param);
        }
        else
        {
            record_error_at_current("Failed to parse first parameter in list.");
            return std::nullopt;
        }
        while (check_token(TokenType::Comma))
        {
            commas_list.push_back(create_token_node(TokenType::Comma, currentTokenInfo));
            advance_and_lex();
            if (check_token(TokenType::CloseParen))
            {
                record_error_at_current("Unexpected ')' after comma in parameter list. Expected parameter declaration.");
                return std::nullopt;
            }
            std::shared_ptr<ParameterDeclarationNode> param_decl = parse_parameter_declaration();
            if (param_decl)
            {
                parameters.push_back(param_decl);
            }
            else
            {
                record_error_at_current("Failed to parse parameter declaration after comma.");
                return std::nullopt;
            }
        }
        return parameters;
    }

    void ScriptParser::parse_base_method_declaration_parts(std::shared_ptr<BaseMethodDeclarationNode> method_node, const CurrentTokenInfo &method_name_token_info)
    {
        method_node->name = create_identifier_node(method_name_token_info);
        advance_and_lex();
        // TODO: Parse generic parameters <T>
        if (check_token(TokenType::OpenParen))
        {
            method_node->openParenToken = create_token_node(TokenType::OpenParen, currentTokenInfo);
            advance_and_lex();
            std::vector<std::shared_ptr<TokenNode>> param_commas;
            std::optional<std::vector<std::shared_ptr<ParameterDeclarationNode>>> params_opt = parse_parameter_list_content(param_commas);
            if (params_opt.has_value())
            {
                method_node->parameters = params_opt.value();
                method_node->parameterCommas = param_commas;
            }
            if (check_token(TokenType::CloseParen))
            {
                method_node->closeParenToken = create_token_node(TokenType::CloseParen, currentTokenInfo);
                advance_and_lex();
            }
            else
            {
                record_error_at_current("Expected ')' to close parameter list for " + std::string(method_name_token_info.lexeme) + ".");
            }
        }
        else
        {
            record_error_at_current("Expected '(' for parameter list of " + std::string(method_name_token_info.lexeme) + ".");
        }
    }

    std::shared_ptr<ConstructorDeclarationNode> ScriptParser::parse_constructor_declaration(const SourceLocation &decl_start_loc, std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers, const CurrentTokenInfo &constructor_name_token_info)
    {
        auto ctor_node = make_ast_node<ConstructorDeclarationNode>(decl_start_loc);
        ctor_node->modifiers = std::move(modifiers);
        parse_base_method_declaration_parts(ctor_node, constructor_name_token_info);
        if (check_token(TokenType::OpenBrace))
        {
            ctor_node->body = parse_block_statement();
        }
        else if (check_token(TokenType::Semicolon))
        {
            ctor_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected '{' for constructor body or ';' for declaration.");
        }
        finalize_node_location(ctor_node);
        return ctor_node;
    }

    std::shared_ptr<MethodDeclarationNode> ScriptParser::parse_method_declaration(const SourceLocation &decl_start_loc, std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers, std::shared_ptr<TypeNameNode> return_type, const CurrentTokenInfo &method_name_token_info)
    {
        auto method_node = make_ast_node<MethodDeclarationNode>(decl_start_loc);
        method_node->modifiers = std::move(modifiers);
        method_node->type = return_type;
        parse_base_method_declaration_parts(method_node, method_name_token_info);
        if (check_token(TokenType::OpenBrace))
        {
            method_node->body = parse_block_statement();
        }
        else if (check_token(TokenType::Semicolon))
        {
            method_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected '{' for method body or ';' for declaration.");
        }
        finalize_node_location(method_node);
        return method_node;
    }

    std::shared_ptr<DestructorDeclarationNode> ScriptParser::parse_destructor_declaration(const SourceLocation &decl_start_loc, std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers, const CurrentTokenInfo &tilde_token_info)
    {
        auto dtor_node = make_ast_node<DestructorDeclarationNode>(decl_start_loc);
        dtor_node->modifiers = std::move(modifiers);
        dtor_node->tildeToken = create_token_node(TokenType::Tilde, tilde_token_info);
        consume_token(TokenType::Tilde, "Expected '~' for destructor.");
        if (!m_current_class_name.has_value())
        {
            record_error_at_current("Destructor declared outside of a class context.");
            auto dummy_name = make_ast_node<IdentifierNode>(currentTokenInfo.location);
            dummy_name->name = "_ERROR_DTOR_NOCLASS_";
            finalize_node_location(dummy_name);
            dtor_node->name = dummy_name;
        }
        else
        {
            if (check_token(TokenType::Identifier) && currentTokenInfo.lexeme == m_current_class_name.value())
            {
                dtor_node->name = create_identifier_node(currentTokenInfo);
                advance_and_lex();
            }
            else
            {
                record_error_at_current("Expected destructor name to match class name '" + m_current_class_name.value_or("") + "'.");
                auto dummy_name = make_ast_node<IdentifierNode>(currentTokenInfo.location);
                dummy_name->name = "_ERROR_DTOR_NAME_";
                finalize_node_location(dummy_name);
                dtor_node->name = dummy_name;
            }
        }
        if (check_token(TokenType::OpenParen))
        {
            dtor_node->openParenToken = create_token_node(TokenType::OpenParen, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected '(' for destructor parameter list.");
        }
        if (check_token(TokenType::CloseParen))
        {
            dtor_node->closeParenToken = create_token_node(TokenType::CloseParen, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected ')' to close destructor parameter list.");
        }
        dtor_node->parameters.clear();
        dtor_node->parameterCommas.clear();
        if (check_token(TokenType::OpenBrace))
        {
            dtor_node->body = parse_block_statement();
        }
        else if (check_token(TokenType::Semicolon))
        {
            dtor_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected '{' for destructor body or ';' for declaration.");
        }
        finalize_node_location(dtor_node);
        return dtor_node;
    }

    std::shared_ptr<ExternalMethodDeclarationNode> ScriptParser::parse_external_method_declaration()
    {
        SourceLocation start_loc = currentTokenInfo.location;
        auto extern_method_node = make_ast_node<ExternalMethodDeclarationNode>(start_loc);
        if (check_token(TokenType::Extern))
        {
            extern_method_node->externKeyword = create_token_node(TokenType::Extern, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected 'extern' keyword for external method declaration.");
            return nullptr;
        }
        auto type = parse_type_name();
        if (!type)
        {
            record_error_at_current("Expected type name for external method declaration.");
            return nullptr;
        }
        extern_method_node->type = type;
        if (!check_token(TokenType::Identifier))
        {
            record_error_at_current("Expected identifier for external method name.");
            return nullptr;
        }
        CurrentTokenInfo name_token_info = currentTokenInfo;
        parse_base_method_declaration_parts(extern_method_node, name_token_info);
        if (extern_method_node->body)
        {
            record_error("External method declaration cannot have a body.", extern_method_node->body.value()->location.value());
            extern_method_node->body = std::nullopt;
        }
        if (check_token(TokenType::Semicolon))
        {
            extern_method_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected ';' to end external method declaration.");
        }
        finalize_node_location(extern_method_node);
        return extern_method_node;
    }

    std::shared_ptr<MemberDeclarationNode> ScriptParser::parse_member_declaration()
    {
        SourceLocation member_start_loc = currentTokenInfo.location;
        std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers = parse_modifiers();
        if (!modifiers.empty() && modifiers.front().second && modifiers.front().second->location.has_value())
        {
            member_start_loc = modifiers.front().second->location.value();
        }
        else
        {
            member_start_loc = currentTokenInfo.location;
        }
        if (check_token(TokenType::Tilde))
        {
            size_t t_off = currentCharOffset;
            int t_l = currentLine;
            int t_c = currentColumn;
            size_t t_lso = currentLineStartOffset;
            CurrentTokenInfo t_cti = currentTokenInfo;
            CurrentTokenInfo t_pti = previousTokenInfo;
            advance_and_lex();
            bool is_potential_dtor = m_current_class_name.has_value() && check_token(TokenType::Identifier) && currentTokenInfo.lexeme == m_current_class_name.value();
            if (is_potential_dtor)
            {
                advance_and_lex();
                bool is_fby_paren = check_token(TokenType::OpenParen);
                currentCharOffset = t_off;
                currentLine = t_l;
                currentColumn = t_c;
                currentLineStartOffset = t_lso;
                currentTokenInfo = t_cti;
                previousTokenInfo = t_pti;
                if (is_fby_paren)
                {
                    return parse_destructor_declaration(member_start_loc, std::move(modifiers), currentTokenInfo);
                }
            }
            else
            {
                currentCharOffset = t_off;
                currentLine = t_l;
                currentColumn = t_c;
                currentLineStartOffset = t_lso;
                currentTokenInfo = t_cti;
                previousTokenInfo = t_pti;
            }
        }
        CurrentTokenInfo potential_type_or_name_token = currentTokenInfo;
        bool is_potential_ctor_name = m_current_class_name.has_value() && check_token(TokenType::Identifier) && potential_type_or_name_token.lexeme == m_current_class_name.value();
        size_t o_co_la = currentCharOffset;
        int o_l_la = currentLine;
        int o_c_la = currentColumn;
        size_t o_lso_la = currentLineStartOffset;
        CurrentTokenInfo o_cti_la = currentTokenInfo;
        CurrentTokenInfo o_pti_la = previousTokenInfo;
        if (check_token(TokenType::Identifier) || check_token(TokenType::Void) || check_token({TokenType::Bool, TokenType::Int, TokenType::String, TokenType::Long, TokenType::Double, TokenType::Char, TokenType::Float}))
        {
            advance_and_lex();
        }
        bool is_fby_op = check_token(TokenType::OpenParen);
        bool is_fby_lt = check_token(TokenType::LessThan);
        currentCharOffset = o_co_la;
        currentLine = o_l_la;
        currentColumn = o_c_la;
        currentLineStartOffset = o_lso_la;
        currentTokenInfo = o_cti_la;
        previousTokenInfo = o_pti_la;
        if (is_potential_ctor_name && (is_fby_op || is_fby_lt))
        {
            return parse_constructor_declaration(member_start_loc, std::move(modifiers), currentTokenInfo);
        }
        else
        {
            std::shared_ptr<TypeNameNode> type;
            if (check_token(TokenType::Void) || check_token(TokenType::Identifier) || check_token({TokenType::Bool, TokenType::Int, TokenType::String, TokenType::Long, TokenType::Double, TokenType::Char, TokenType::Float}))
            {
                type = parse_type_name();
            }
            else
            {
                record_error_at_current("Expected type name, 'void', or constructor name at start of member declaration.");
                return nullptr;
            }
            if (!type)
            {
                return nullptr;
            }
            if (check_token(TokenType::Identifier))
            {
                CurrentTokenInfo name_token_info = currentTokenInfo;
                o_co_la = currentCharOffset;
                o_l_la = currentLine;
                o_c_la = currentColumn;
                o_lso_la = currentLineStartOffset;
                o_cti_la = currentTokenInfo;
                o_pti_la = previousTokenInfo;
                advance_and_lex();
                bool is_meth_like_sig = check_token(TokenType::OpenParen) || check_token(TokenType::LessThan);
                currentCharOffset = o_co_la;
                currentLine = o_l_la;
                currentColumn = o_c_la;
                currentLineStartOffset = o_lso_la;
                currentTokenInfo = o_cti_la;
                previousTokenInfo = o_pti_la;
                if (is_meth_like_sig)
                {
                    return parse_method_declaration(member_start_loc, std::move(modifiers), type, name_token_info);
                }
                else
                {
                    return parse_field_declaration(member_start_loc, std::move(modifiers), type);
                }
            }
            else
            {
                record_error_at_current("Expected identifier for member name after type.");
                return nullptr;
            }
        }
    }

    std::shared_ptr<FieldDeclarationNode> ScriptParser::parse_field_declaration(const SourceLocation &decl_start_loc, std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers, std::shared_ptr<TypeNameNode> type)
    {
        auto field_node = make_ast_node<FieldDeclarationNode>(decl_start_loc);
        field_node->modifiers = std::move(modifiers);
        field_node->type = type;
        bool first_declarator = true;
        do
        {
            if (!first_declarator)
            {
                if (check_token(TokenType::Comma))
                {
                    field_node->declaratorCommas.push_back(create_token_node(TokenType::Comma, currentTokenInfo));
                    advance_and_lex();
                }
                else
                {
                    record_error_at_current("Expected ',' or ';' in field declaration with multiple declarators.");
                    break;
                }
            }
            first_declarator = false;
            SourceLocation declarator_start_loc = currentTokenInfo.location;
            auto declarator_node = make_ast_node<VariableDeclaratorNode>(declarator_start_loc);
            if (check_token(TokenType::Identifier))
            {
                declarator_node->name = create_identifier_node(currentTokenInfo);
                advance_and_lex();
            }
            else
            {
                record_error_at_current("Expected identifier for field name.");
                declarator_node->name = make_ast_node<IdentifierNode>(currentTokenInfo.location);
                declarator_node->name->name = "_ERROR_FIELD_NAME_";
                finalize_node_location(declarator_node->name);
                if (!check_token(TokenType::Assign) && !check_token(TokenType::Comma) && !check_token(TokenType::Semicolon))
                {
                    field_node->declarators.push_back(declarator_node);
                    goto end_field_declarators_rules;
                }
            }
            if (check_token(TokenType::Assign))
            {
                declarator_node->equalsToken = create_token_node(TokenType::Assign, currentTokenInfo);
                advance_and_lex();
                declarator_node->initializer = parse_expression();
                if (!declarator_node->initializer)
                {
                    record_error_at_current("Invalid initializer expression for field.");
                }
            }
            finalize_node_location(declarator_node);
            field_node->declarators.push_back(declarator_node);
        } while (check_token(TokenType::Comma));
    end_field_declarators_rules:;
        if (check_token(TokenType::Semicolon))
        {
            field_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            SourceLocation error_loc = previousTokenInfo.location;
            if (!field_node->declarators.empty() && field_node->declarators.back()->location.has_value())
            {
                error_loc.lineStart = field_node->declarators.back()->location.value().lineEnd;
                error_loc.columnStart = field_node->declarators.back()->location.value().columnEnd + 1;
            }
            else if (field_node->type && field_node->type.has_value() && field_node->type.value()->location.has_value())
            {
                error_loc.lineStart = field_node->type.value()->location.value().lineEnd;
                error_loc.columnStart = field_node->type.value()->location.value().columnEnd + 1;
            }
            record_error("Expected ';' after field declaration.", error_loc);
        }
        finalize_node_location(field_node);
        return field_node;
    }

    std::shared_ptr<IfStatementNode> ScriptParser::parse_if_statement()
    {
        SourceLocation statement_start_loc = currentTokenInfo.location;
        auto if_node = make_ast_node<IfStatementNode>(statement_start_loc);
        if_node->ifKeyword = create_token_node(TokenType::If, currentTokenInfo);
        consume_token(TokenType::If, "Expected 'if' keyword.");
        if (check_token(TokenType::OpenParen))
        {
            if_node->openParenToken = create_token_node(TokenType::OpenParen, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected '(' after 'if' keyword.");
        }
        if_node->condition = parse_expression();
        if (!if_node->condition)
        {
            record_error_at_current("Expected condition expression in 'if' statement.");
        }
        if (check_token(TokenType::CloseParen))
        {
            if_node->closeParenToken = create_token_node(TokenType::CloseParen, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected ')' after 'if' condition.");
        }
        if_node->thenStatement = parse_statement();
        if (!if_node->thenStatement)
        {
            record_error_at_current("Expected statement for 'then' branch of 'if' statement.");
        }
        if (match_token(TokenType::Else))
        {
            if_node->elseKeyword = create_token_node(TokenType::Else, previousTokenInfo);
            if_node->elseStatement = parse_statement();
            if (!if_node->elseStatement.has_value() || !if_node->elseStatement.value())
            {
                record_error_at_current("Expected statement for 'else' branch of 'if' statement.");
            }
        }
        finalize_node_location(if_node);
        return if_node;
    }

    std::shared_ptr<WhileStatementNode> ScriptParser::parse_while_statement()
    {
        SourceLocation statement_start_loc = currentTokenInfo.location;
        auto while_node = make_ast_node<WhileStatementNode>(statement_start_loc);
        while_node->whileKeyword = create_token_node(TokenType::While, currentTokenInfo);
        consume_token(TokenType::While, "Expected 'while' keyword.");
        if (check_token(TokenType::OpenParen))
        {
            while_node->openParenToken = create_token_node(TokenType::OpenParen, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected '(' after 'while' keyword.");
        }
        while_node->condition = parse_expression();
        if (!while_node->condition)
        {
            record_error_at_current("Expected condition expression in 'while' statement.");
        }
        if (check_token(TokenType::CloseParen))
        {
            while_node->closeParenToken = create_token_node(TokenType::CloseParen, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected ')' after 'while' condition.");
        }
        while_node->body = parse_statement();
        if (!while_node->body)
        {
            record_error_at_current("Expected statement for 'while' loop body.");
        }
        finalize_node_location(while_node);
        return while_node;
    }

    std::shared_ptr<ForStatementNode> ScriptParser::parse_for_statement()
    {
        SourceLocation statement_start_loc = currentTokenInfo.location;
        auto for_node = make_ast_node<ForStatementNode>(statement_start_loc);
        for_node->forKeyword = create_token_node(TokenType::For, currentTokenInfo);
        consume_token(TokenType::For, "Expected 'for' keyword.");
        if (check_token(TokenType::OpenParen))
        {
            for_node->openParenToken = create_token_node(TokenType::OpenParen, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected '(' after 'for' keyword.");
        }
        if (!check_token(TokenType::Semicolon))
        {
            if (check_token(TokenType::Var) || check_token(TokenType::Bool) || check_token(TokenType::Int) || check_token(TokenType::String) || check_token(TokenType::Long) || check_token(TokenType::Double) || check_token(TokenType::Char) || check_token(TokenType::Float))
            {
                auto local_var_decl = parse_local_variable_declaration_statement();
                for_node->initializers = local_var_decl;
                if (local_var_decl && local_var_decl->semicolonToken)
                {
                    for_node->firstSemicolonToken = local_var_decl->semicolonToken;
                }
                else
                {
                    if (check_token(TokenType::Semicolon))
                    {
                        for_node->firstSemicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
                        advance_and_lex();
                    }
                    else
                    {
                        record_error_at_current("Expected ';' after 'for' loop initializer declaration.");
                    }
                }
            }
            else
            {
                std::vector<std::shared_ptr<ExpressionNode>> init_expressions;
                std::vector<std::shared_ptr<TokenNode>> init_commas;
                if (!check_token(TokenType::Semicolon))
                {
                    do
                    {
                        init_expressions.push_back(parse_expression());
                        if (check_token(TokenType::Comma))
                        {
                            init_commas.push_back(create_token_node(TokenType::Comma, currentTokenInfo));
                            advance_and_lex();
                        }
                        else
                        {
                            break;
                        }
                    } while (!check_token(TokenType::Semicolon) && !is_at_end_of_token_stream());
                }
                for_node->initializers = init_expressions;
                for_node->initializerCommas = init_commas;
                if (check_token(TokenType::Semicolon))
                {
                    for_node->firstSemicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
                    advance_and_lex();
                }
                else
                {
                    record_error_at_current("Expected ';' after 'for' loop initializer expressions.");
                }
            }
        }
        else
        {
            for_node->firstSemicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
        }
        if (!check_token(TokenType::Semicolon))
        {
            for_node->condition = parse_expression();
        }
        if (check_token(TokenType::Semicolon))
        {
            for_node->secondSemicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected ';' after 'for' loop condition.");
        }
        if (!check_token(TokenType::CloseParen))
        {
            std::vector<std::shared_ptr<ExpressionNode>> incr_expressions;
            std::vector<std::shared_ptr<TokenNode>> incr_commas;
            do
            {
                incr_expressions.push_back(parse_expression());
                if (check_token(TokenType::Comma))
                {
                    incr_commas.push_back(create_token_node(TokenType::Comma, currentTokenInfo));
                    advance_and_lex();
                }
                else
                {
                    break;
                }
            } while (!check_token(TokenType::CloseParen) && !is_at_end_of_token_stream());
            for_node->incrementors = incr_expressions;
            for_node->incrementorCommas = incr_commas;
        }
        if (check_token(TokenType::CloseParen))
        {
            for_node->closeParenToken = create_token_node(TokenType::CloseParen, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected ')' after 'for' loop clauses.");
        }
        for_node->body = parse_statement();
        if (!for_node->body)
        {
            record_error_at_current("Expected statement for 'for' loop body.");
        }
        finalize_node_location(for_node);
        return for_node;
    }

    std::shared_ptr<ExpressionNode> ScriptParser::parse_assignment_expression()
    {
        SourceLocation expression_start_loc = currentTokenInfo.location;
        std::shared_ptr<ExpressionNode> left_target = parse_conditional_expression();
        if (check_token(TokenType::Assign) || check_token(TokenType::PlusAssign) || check_token(TokenType::MinusAssign) || check_token(TokenType::AsteriskAssign) || check_token(TokenType::SlashAssign) || check_token(TokenType::PercentAssign))
        {
            if (left_target && left_target->location.has_value())
            {
                expression_start_loc = left_target->location.value();
            }
            auto assignment_node = make_ast_node<AssignmentExpressionNode>(expression_start_loc);
            assignment_node->target = left_target;
            assignment_node->operatorToken = create_token_node(currentTokenInfo.type, currentTokenInfo);
            TokenType op_type = currentTokenInfo.type;
            advance_and_lex();
            switch (op_type)
            {
            case TokenType::Assign:
                assignment_node->opKind = AssignmentOperatorKind::Assign;
                break;
            case TokenType::PlusAssign:
                assignment_node->opKind = AssignmentOperatorKind::AddAssign;
                break;
            case TokenType::MinusAssign:
                assignment_node->opKind = AssignmentOperatorKind::SubtractAssign;
                break;
            case TokenType::AsteriskAssign:
                assignment_node->opKind = AssignmentOperatorKind::MultiplyAssign;
                break;
            case TokenType::SlashAssign:
                assignment_node->opKind = AssignmentOperatorKind::DivideAssign;
                break;
            case TokenType::PercentAssign:
                assignment_node->opKind = AssignmentOperatorKind::ModuloAssign;
                break;
            default:
                record_error_at_previous("Internal parser error: Unexpected assignment operator.");
                assignment_node->opKind = AssignmentOperatorKind::Assign;
                break;
            }
            assignment_node->source = parse_assignment_expression();
            finalize_node_location(assignment_node);
            return assignment_node;
        }
        return left_target;
    }

    std::shared_ptr<ExpressionNode> ScriptParser::parse_conditional_expression() { return parse_logical_or_expression(); }
    std::shared_ptr<ExpressionNode> ScriptParser::parse_logical_or_expression()
    {
        SourceLocation expression_start_loc = currentTokenInfo.location;
        std::shared_ptr<ExpressionNode> left_operand = parse_logical_and_expression();
        while (check_token(TokenType::LogicalOr))
        {
            if (left_operand && left_operand->location.has_value())
            {
                expression_start_loc = left_operand->location.value();
            }
            auto binary_expr_node = make_ast_node<BinaryExpressionNode>(expression_start_loc);
            binary_expr_node->left = left_operand;
            binary_expr_node->operatorToken = create_token_node(currentTokenInfo.type, currentTokenInfo);
            binary_expr_node->opKind = BinaryOperatorKind::LogicalOr;
            advance_and_lex();
            binary_expr_node->right = parse_logical_and_expression();
            finalize_node_location(binary_expr_node);
            left_operand = binary_expr_node;
        }
        return left_operand;
    }
    std::shared_ptr<ExpressionNode> ScriptParser::parse_logical_and_expression()
    {
        SourceLocation expression_start_loc = currentTokenInfo.location;
        std::shared_ptr<ExpressionNode> left_operand = parse_equality_expression();
        while (check_token(TokenType::LogicalAnd))
        {
            if (left_operand && left_operand->location.has_value())
            {
                expression_start_loc = left_operand->location.value();
            }
            auto binary_expr_node = make_ast_node<BinaryExpressionNode>(expression_start_loc);
            binary_expr_node->left = left_operand;
            binary_expr_node->operatorToken = create_token_node(currentTokenInfo.type, currentTokenInfo);
            binary_expr_node->opKind = BinaryOperatorKind::LogicalAnd;
            advance_and_lex();
            binary_expr_node->right = parse_equality_expression();
            finalize_node_location(binary_expr_node);
            left_operand = binary_expr_node;
        }
        return left_operand;
    }
    std::shared_ptr<ExpressionNode> ScriptParser::parse_equality_expression()
    {
        SourceLocation expression_start_loc = currentTokenInfo.location;
        std::shared_ptr<ExpressionNode> left_operand = parse_relational_expression();
        while (check_token(TokenType::EqualsEquals) || check_token(TokenType::NotEquals))
        {
            if (left_operand && left_operand->location.has_value())
            {
                expression_start_loc = left_operand->location.value();
            }
            auto binary_expr_node = make_ast_node<BinaryExpressionNode>(expression_start_loc);
            binary_expr_node->left = left_operand;
            binary_expr_node->operatorToken = create_token_node(currentTokenInfo.type, currentTokenInfo);
            if (check_token(TokenType::EqualsEquals))
            {
                binary_expr_node->opKind = BinaryOperatorKind::Equals;
            }
            else if (check_token(TokenType::NotEquals))
            {
                binary_expr_node->opKind = BinaryOperatorKind::NotEquals;
            }
            advance_and_lex();
            binary_expr_node->right = parse_relational_expression();
            finalize_node_location(binary_expr_node);
            left_operand = binary_expr_node;
        }
        return left_operand;
    }
    std::shared_ptr<ExpressionNode> ScriptParser::parse_relational_expression()
    {
        SourceLocation expression_start_loc = currentTokenInfo.location;
        std::shared_ptr<ExpressionNode> left_operand = parse_additive_expression();
        while (check_token(TokenType::LessThan) || check_token(TokenType::GreaterThan) || check_token(TokenType::LessThanOrEqual) || check_token(TokenType::GreaterThanOrEqual))
        {
            if (left_operand && left_operand->location.has_value())
            {
                expression_start_loc = left_operand->location.value();
            }
            auto binary_expr_node = make_ast_node<BinaryExpressionNode>(expression_start_loc);
            binary_expr_node->left = left_operand;
            binary_expr_node->operatorToken = create_token_node(currentTokenInfo.type, currentTokenInfo);
            if (check_token(TokenType::LessThan))
            {
                binary_expr_node->opKind = BinaryOperatorKind::LessThan;
            }
            else if (check_token(TokenType::GreaterThan))
            {
                binary_expr_node->opKind = BinaryOperatorKind::GreaterThan;
            }
            else if (check_token(TokenType::LessThanOrEqual))
            {
                binary_expr_node->opKind = BinaryOperatorKind::LessThanOrEqual;
            }
            else if (check_token(TokenType::GreaterThanOrEqual))
            {
                binary_expr_node->opKind = BinaryOperatorKind::GreaterThanOrEqual;
            }
            advance_and_lex();
            binary_expr_node->right = parse_additive_expression();
            finalize_node_location(binary_expr_node);
            left_operand = binary_expr_node;
        }
        return left_operand;
    }
    std::shared_ptr<ExpressionNode> ScriptParser::parse_additive_expression()
    {
        SourceLocation expression_start_loc = currentTokenInfo.location;
        std::shared_ptr<ExpressionNode> left_operand = parse_multiplicative_expression();
        while (check_token(TokenType::Plus) || check_token(TokenType::Minus))
        {
            if (left_operand && left_operand->location.has_value())
            {
                expression_start_loc = left_operand->location.value();
            }
            auto binary_expr_node = make_ast_node<BinaryExpressionNode>(expression_start_loc);
            binary_expr_node->left = left_operand;
            binary_expr_node->operatorToken = create_token_node(currentTokenInfo.type, currentTokenInfo);
            if (check_token(TokenType::Plus))
            {
                binary_expr_node->opKind = BinaryOperatorKind::Add;
            }
            else if (check_token(TokenType::Minus))
            {
                binary_expr_node->opKind = BinaryOperatorKind::Subtract;
            }
            advance_and_lex();
            binary_expr_node->right = parse_multiplicative_expression();
            finalize_node_location(binary_expr_node);
            left_operand = binary_expr_node;
        }
        return left_operand;
    }
    std::shared_ptr<ExpressionNode> ScriptParser::parse_multiplicative_expression()
    {
        SourceLocation expression_start_loc = currentTokenInfo.location;
        std::shared_ptr<ExpressionNode> left_operand = parse_unary_expression();
        while (check_token(TokenType::Asterisk) || check_token(TokenType::Slash) || check_token(TokenType::Percent))
        {
            if (left_operand && left_operand->location.has_value())
            {
                expression_start_loc = left_operand->location.value();
            }
            auto binary_expr_node = make_ast_node<BinaryExpressionNode>(expression_start_loc);
            binary_expr_node->left = left_operand;
            binary_expr_node->operatorToken = create_token_node(currentTokenInfo.type, currentTokenInfo);
            if (check_token(TokenType::Asterisk))
            {
                binary_expr_node->opKind = BinaryOperatorKind::Multiply;
            }
            else if (check_token(TokenType::Slash))
            {
                binary_expr_node->opKind = BinaryOperatorKind::Divide;
            }
            else if (check_token(TokenType::Percent))
            {
                binary_expr_node->opKind = BinaryOperatorKind::Modulo;
            }
            advance_and_lex();
            binary_expr_node->right = parse_unary_expression();
            finalize_node_location(binary_expr_node);
            left_operand = binary_expr_node;
        }
        return left_operand;
    }

    std::shared_ptr<ExpressionNode> ScriptParser::parse_unary_expression()
    {
        SourceLocation unary_start_loc = currentTokenInfo.location;
        if (check_token(TokenType::OpenParen))
        {
            size_t o_co = currentCharOffset;
            int o_l = currentLine;
            int o_c = currentColumn;
            size_t o_lso = currentLineStartOffset;
            CurrentTokenInfo o_cti = currentTokenInfo;
            CurrentTokenInfo o_pti = previousTokenInfo;
            std::vector<ParseError> o_err = errors;
            bool might_be_cast = false;
            SourceLocation open_paren_loc = currentTokenInfo.location;
            advance_and_lex();
            if (check_token(TokenType::Identifier) || check_token({TokenType::Bool, TokenType::Int, TokenType::String, TokenType::Long, TokenType::Double, TokenType::Char, TokenType::Void, TokenType::Float}))
            {
                std::shared_ptr<TypeNameNode> potential_type_name = parse_type_name();
                if (potential_type_name && check_token(TokenType::CloseParen))
                {
                    might_be_cast = true;
                    auto cast_node = make_ast_node<CastExpressionNode>(open_paren_loc);
                    cast_node->openParenToken = create_token_node(TokenType::OpenParen, o_cti);
                    cast_node->targetType = potential_type_name;
                    cast_node->closeParenToken = create_token_node(TokenType::CloseParen, currentTokenInfo);
                    advance_and_lex();
                    cast_node->expression = parse_unary_expression();
                    finalize_node_location(cast_node);
                    return cast_node;
                }
            }
            currentCharOffset = o_co;
            currentLine = o_l;
            currentColumn = o_c;
            currentLineStartOffset = o_lso;
            currentTokenInfo = o_cti;
            previousTokenInfo = o_pti;
            errors = o_err;
        }
        if (check_token(TokenType::LogicalNot) || check_token(TokenType::Plus) || check_token(TokenType::Minus) || check_token(TokenType::Increment) || check_token(TokenType::Decrement))
        {
            auto unary_node = make_ast_node<UnaryExpressionNode>(unary_start_loc);
            unary_node->operatorToken = create_token_node(currentTokenInfo.type, currentTokenInfo);
            unary_node->isPostfix = false;
            TokenType op_token_type = currentTokenInfo.type;
            advance_and_lex();
            switch (op_token_type)
            {
            case TokenType::LogicalNot:
                unary_node->opKind = UnaryOperatorKind::LogicalNot;
                break;
            case TokenType::Plus:
                unary_node->opKind = UnaryOperatorKind::UnaryPlus;
                break;
            case TokenType::Minus:
                unary_node->opKind = UnaryOperatorKind::UnaryMinus;
                break;
            case TokenType::Increment:
                unary_node->opKind = UnaryOperatorKind::PreIncrement;
                break;
            case TokenType::Decrement:
                unary_node->opKind = UnaryOperatorKind::PreDecrement;
                break;
            default:
                record_error_at_previous("Internal parser error: Unexpected unary operator token.");
                return parse_postfix_expression();
            }
            unary_node->operand = parse_unary_expression();
            finalize_node_location(unary_node);
            return unary_node;
        }
        return parse_postfix_expression();
    }

    std::shared_ptr<ExpressionNode> ScriptParser::parse_primary_expression()
    {
        SourceLocation primary_start_loc = currentTokenInfo.location;
        switch (currentTokenInfo.type)
        {
        case TokenType::BooleanLiteral:
        case TokenType::IntegerLiteral:
        case TokenType::LongLiteral:
        case TokenType::FloatLiteral:
        case TokenType::DoubleLiteral:
        case TokenType::CharLiteral:
        case TokenType::StringLiteral:
        case TokenType::NullLiteral:
        {
            auto literal_node = make_ast_node<LiteralExpressionNode>(primary_start_loc);
            literal_node->token = create_token_node(currentTokenInfo.type, currentTokenInfo);
            if (currentTokenInfo.type == TokenType::BooleanLiteral)
            {
                literal_node->kind = LiteralKind::Boolean;
                literal_node->valueText = std::string(currentTokenInfo.lexeme);
            }
            else if (currentTokenInfo.type == TokenType::IntegerLiteral)
            {
                literal_node->kind = LiteralKind::Integer;
                literal_node->valueText = std::string(currentTokenInfo.lexeme);
            }
            else if (currentTokenInfo.type == TokenType::LongLiteral)
            {
                literal_node->kind = LiteralKind::Long;
                literal_node->valueText = std::string(currentTokenInfo.lexeme);
            }
            else if (currentTokenInfo.type == TokenType::FloatLiteral)
            {
                literal_node->kind = LiteralKind::Float;
                literal_node->valueText = std::string(currentTokenInfo.lexeme);
            }
            else if (currentTokenInfo.type == TokenType::DoubleLiteral)
            {
                literal_node->kind = LiteralKind::Double;
                literal_node->valueText = std::string(currentTokenInfo.lexeme);
            }
            else if (currentTokenInfo.type == TokenType::StringLiteral)
            {
                literal_node->kind = LiteralKind::String;
                literal_node->valueText = std::get<std::string>(currentTokenInfo.literalValue);
            }
            else if (currentTokenInfo.type == TokenType::CharLiteral)
            {
                literal_node->kind = LiteralKind::Char;
                literal_node->valueText = std::string(1, std::get<char>(currentTokenInfo.literalValue));
            }
            else if (currentTokenInfo.type == TokenType::NullLiteral)
            {
                literal_node->kind = LiteralKind::Null;
                literal_node->valueText = "null";
            }
            advance_and_lex();
            finalize_node_location(literal_node);
            return literal_node;
        }
        case TokenType::Identifier:
        {
            auto ident_node_for_expr = make_ast_node<IdentifierExpressionNode>(primary_start_loc);
            ident_node_for_expr->identifier = create_identifier_node(currentTokenInfo);
            advance_and_lex();
            finalize_node_location(ident_node_for_expr);
            return ident_node_for_expr;
        }
        case TokenType::This:
        {
            auto this_node = make_ast_node<ThisExpressionNode>(primary_start_loc);
            this_node->thisKeyword = create_token_node(TokenType::This, currentTokenInfo);
            advance_and_lex();
            finalize_node_location(this_node);
            return this_node;
        }
        case TokenType::OpenParen:
        {
            auto paren_expr_node = make_ast_node<ParenthesizedExpressionNode>(primary_start_loc);
            paren_expr_node->openParenToken = create_token_node(TokenType::OpenParen, currentTokenInfo);
            advance_and_lex();
            paren_expr_node->expression = parse_expression();
            paren_expr_node->closeParenToken = create_token_node(TokenType::CloseParen, currentTokenInfo);
            consume_token(TokenType::CloseParen, "Expected ')' after expression in parentheses.");
            finalize_node_location(paren_expr_node);
            return paren_expr_node;
        }
        case TokenType::New:
        {
            return parse_object_creation_expression();
        }
        case TokenType::Bool:
        case TokenType::Int:
        case TokenType::String:
        case TokenType::Long:
        case TokenType::Double:
        case TokenType::Char:
        case TokenType::Float:
        {
            CurrentTokenInfo type_keyword_token_data = currentTokenInfo;
            advance_and_lex();
            auto ident_for_type_name = make_ast_node<IdentifierNode>(type_keyword_token_data.location);
            ident_for_type_name->name = std::string(type_keyword_token_data.lexeme);
            finalize_node_location(ident_for_type_name);
            auto type_as_expr = make_ast_node<IdentifierExpressionNode>(type_keyword_token_data.location);
            type_as_expr->identifier = ident_for_type_name;
            finalize_node_location(type_as_expr);
            return type_as_expr;
        }
        default:
            record_error_at_current("Unexpected token '" + std::string(currentTokenInfo.lexeme) + "' when expecting a primary expression.");
            auto error_expr_node = make_ast_node<LiteralExpressionNode>(primary_start_loc);
            error_expr_node->kind = LiteralKind::Null;
            error_expr_node->valueText = "_ERROR_EXPR_";
            error_expr_node->token = create_token_node(TokenType::Error, currentTokenInfo);
            advance_and_lex();
            finalize_node_location(error_expr_node);
            return error_expr_node;
        }
    }

    std::shared_ptr<ObjectCreationExpressionNode> ScriptParser::parse_object_creation_expression()
    {
        SourceLocation start_loc = currentTokenInfo.location;
        auto new_keyword_token_node = create_token_node(TokenType::New, currentTokenInfo);
        consume_token(TokenType::New, "Expected 'new' keyword for object creation.");
        auto node = make_ast_node<ObjectCreationExpressionNode>(start_loc);
        node->newKeyword = new_keyword_token_node;
        node->type = parse_type_name();
        if (check_token(TokenType::OpenParen))
        {
            node->argumentList = parse_argument_list();
        }
        else
        {
            node->argumentList = std::nullopt;
        }
        finalize_node_location(node);
        return node;
    }

    std::shared_ptr<TypeNameNode> ScriptParser::parse_type_name()
    {
        SourceLocation type_name_start_loc = currentTokenInfo.location;
        auto node = make_ast_node<TypeNameNode>(type_name_start_loc);
        if (check_token(TokenType::Identifier) || check_token({TokenType::Bool, TokenType::Int, TokenType::String, TokenType::Long, TokenType::Double, TokenType::Char, TokenType::Void, TokenType::Float}))
        {
            auto ident_for_segment = create_identifier_node(currentTokenInfo);
            advance_and_lex();
            node->name_segment = ident_for_segment;
            while (check_token(TokenType::Dot))
            {
                auto qualified_node = make_ast_node<QualifiedNameNode>(type_name_start_loc);
                qualified_node->left = node; // This was the issue, left should be TypeNameNode
                qualified_node->dotToken = create_token_node(TokenType::Dot, currentTokenInfo);
                advance_and_lex();
                if (check_token(TokenType::Identifier))
                {
                    qualified_node->right = create_identifier_node(currentTokenInfo);
                    advance_and_lex();
                }
                else
                {
                    record_error_at_current("Expected identifier after '.' in qualified name.");
                    auto dummy_ident = make_ast_node<IdentifierNode>(currentTokenInfo.location);
                    dummy_ident->name = "_ERROR_QUALIFIER_";
                    finalize_node_location(dummy_ident);
                    qualified_node->right = dummy_ident;
                }
                finalize_node_location(qualified_node);
                // Create a new TypeNameNode to wrap the QualifiedNameNode
                auto new_wrapper_node = make_ast_node<TypeNameNode>(type_name_start_loc); // Start loc of whole type
                new_wrapper_node->name_segment = qualified_node;
                // Transfer array/generic from 'node' to 'new_wrapper_node' if they were parsed on the leftmost segment
                new_wrapper_node->openAngleBracketToken = node->openAngleBracketToken;
                new_wrapper_node->typeArguments = node->typeArguments;
                new_wrapper_node->typeArgumentCommas = node->typeArgumentCommas;
                new_wrapper_node->closeAngleBracketToken = node->closeAngleBracketToken;
                new_wrapper_node->openSquareBracketToken = node->openSquareBracketToken;
                new_wrapper_node->closeSquareBracketToken = node->closeSquareBracketToken;
                node = new_wrapper_node;
            }
        }
        else
        {
            record_error_at_current("Expected identifier or primitive type keyword for type name.");
            auto dummy_ident = make_ast_node<IdentifierNode>(currentTokenInfo.location);
            dummy_ident->name = "_ERROR_TYPE_";
            finalize_node_location(dummy_ident);
            node->name_segment = dummy_ident;
            if (currentTokenInfo.type != TokenType::EndOfFile && currentTokenInfo.type != TokenType::Error)
                advance_and_lex();
        }
        if (check_token(TokenType::LessThan))
        {
            node->openAngleBracketToken = create_token_node(TokenType::LessThan, currentTokenInfo);
            advance_and_lex();
            if (!check_token(TokenType::GreaterThan))
            {
                do
                {
                    node->typeArguments.push_back(parse_type_name());
                    if (check_token(TokenType::Comma))
                    {
                        node->typeArgumentCommas.push_back(create_token_node(TokenType::Comma, currentTokenInfo));
                        advance_and_lex();
                    }
                    else
                    {
                        break;
                    }
                } while (!check_token(TokenType::GreaterThan) && !is_at_end_of_token_stream());
            }
            node->closeAngleBracketToken = create_token_node(TokenType::GreaterThan, currentTokenInfo);
            consume_token(TokenType::GreaterThan, "Expected '>' to close generic type argument list for type name.");
        }
        if (check_token(TokenType::OpenBracket))
        {
            node->openSquareBracketToken = create_token_node(TokenType::OpenBracket, currentTokenInfo);
            advance_and_lex();
            node->closeSquareBracketToken = create_token_node(TokenType::CloseBracket, currentTokenInfo);
            consume_token(TokenType::CloseBracket, "Expected ']' for array type specifier.");
        }
        finalize_node_location(node);
        return node;
    }

    std::optional<std::shared_ptr<ArgumentListNode>> ScriptParser::parse_argument_list()
    {
        SourceLocation start_loc = currentTokenInfo.location;
        if (!check_token(TokenType::OpenParen))
        {
            return std::nullopt;
        }
        auto arg_list_node = make_ast_node<ArgumentListNode>(start_loc);
        arg_list_node->openParenToken = create_token_node(TokenType::OpenParen, currentTokenInfo);
        advance_and_lex();
        bool first_argument = true;
        if (!check_token(TokenType::CloseParen))
        {
            do
            {
                if (!first_argument)
                {
                    if (check_token(TokenType::Comma))
                    {
                        arg_list_node->commas.push_back(create_token_node(TokenType::Comma, currentTokenInfo));
                        advance_and_lex();
                    }
                    else
                    {
                        record_error_at_current("Expected ',' or ')' in argument list.");
                        break;
                    }
                }
                first_argument = false;
                SourceLocation argument_start_loc = currentTokenInfo.location;
                auto argument_node = make_ast_node<ArgumentNode>(argument_start_loc);
                std::shared_ptr<ExpressionNode> expr = parse_expression();
                if (expr)
                {
                    argument_node->expression = expr;
                    finalize_node_location(argument_node);
                    arg_list_node->arguments.push_back(argument_node);
                }
                else
                {
                    if (errors.back().location.lineStart == argument_start_loc.lineStart && errors.back().location.columnStart == argument_start_loc.columnStart)
                    {
                        break;
                    }
                }
            } while (!check_token(TokenType::CloseParen) && !is_at_end_of_token_stream());
        }
        if (check_token(TokenType::CloseParen))
        {
            arg_list_node->closeParenToken = create_token_node(TokenType::CloseParen, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected ')' to close argument list.");
        }
        finalize_node_location(arg_list_node);
        return arg_list_node;
    }

    std::shared_ptr<BlockStatementNode> ScriptParser::parse_block_statement()
    {
        SourceLocation block_start_loc = currentTokenInfo.location;
        auto block_node = make_ast_node<BlockStatementNode>(block_start_loc);
        if (check_token(TokenType::OpenBrace))
        {
            block_node->openBraceToken = create_token_node(TokenType::OpenBrace, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected '{' to start a block statement.");
        }
        while (!check_token(TokenType::CloseBrace) && !is_at_end_of_token_stream())
        {
            std::shared_ptr<StatementNode> statement = parse_statement();
            if (statement)
            {
                block_node->statements.push_back(statement);
            }
            else
            {
                if (!is_at_end_of_token_stream() && currentTokenInfo.location.columnStart == previousTokenInfo.location.columnStart && currentTokenInfo.location.lineStart == previousTokenInfo.location.lineStart)
                {
                    record_error_at_current("Parser stuck in block statement. Advancing token.");
                    advance_and_lex();
                }
                if (errors.size() > 10 && errors.back().location.fileName == block_start_loc.fileName)
                {
                    record_error_at_current("Too many errors in block, skipping to '}'.");
                    while (!check_token(TokenType::CloseBrace) && !is_at_end_of_token_stream())
                    {
                        advance_and_lex();
                    }
                    break;
                }
            }
        }
        if (check_token(TokenType::CloseBrace))
        {
            block_node->closeBraceToken = create_token_node(TokenType::CloseBrace, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            SourceLocation error_loc = previousTokenInfo.location;
            if (!block_node->statements.empty() && block_node->statements.back()->location.has_value())
            {
                error_loc = block_node->statements.back()->location.value();
                error_loc.columnStart = error_loc.columnEnd + 1;
            }
            else if (block_node->openBraceToken && block_node->openBraceToken->location.has_value())
            {
                error_loc = block_node->openBraceToken->location.value();
                error_loc.columnStart = error_loc.columnEnd + 1;
            }
            else
            {
                error_loc = currentTokenInfo.location;
            }
            record_error("Expected '}' to close block statement. Found " + token_type_to_string(currentTokenInfo.type) + " instead.", error_loc);
        }
        finalize_node_location(block_node);
        return block_node;
    }

    std::shared_ptr<LocalVariableDeclarationStatementNode> ScriptParser::parse_local_variable_declaration_statement()
    {
        SourceLocation statement_start_loc = currentTokenInfo.location;
        auto var_decl_node = make_ast_node<LocalVariableDeclarationStatementNode>(statement_start_loc);
        if (check_token(TokenType::Var))
        {
            var_decl_node->varKeywordToken = create_token_node(TokenType::Var, currentTokenInfo);
            advance_and_lex();
            auto var_type_name_node = make_ast_node<TypeNameNode>(var_decl_node->varKeywordToken.value()->location.value());
            auto var_ident_node = make_ast_node<IdentifierNode>(var_decl_node->varKeywordToken.value()->location.value());
            var_ident_node->name = "var";
            finalize_node_location(var_ident_node);
            var_type_name_node->name_segment = var_ident_node;
            finalize_node_location(var_type_name_node);
            var_decl_node->type = var_type_name_node;
        }
        else
        {
            std::shared_ptr<TypeNameNode> type_node = parse_type_name();
            if (type_node)
            {
                var_decl_node->type = type_node;
            }
            else
            {
                record_error_at_current("Expected type name or 'var' for local variable declaration.");
                auto dummy_type_name = make_ast_node<TypeNameNode>(currentTokenInfo.location);
                auto dummy_ident = make_ast_node<IdentifierNode>(currentTokenInfo.location);
                dummy_ident->name = "_ERROR_TYPE_";
                finalize_node_location(dummy_ident);
                dummy_type_name->name_segment = dummy_ident;
                finalize_node_location(dummy_type_name);
                var_decl_node->type = dummy_type_name;
            }
        }
        bool first_declarator = true;
        do
        {
            if (!first_declarator)
            {
                if (check_token(TokenType::Comma))
                {
                    var_decl_node->declaratorCommas.push_back(create_token_node(TokenType::Comma, currentTokenInfo));
                    advance_and_lex();
                }
                else
                {
                    record_error_at_current("Expected ',' or ';' in variable declaration.");
                    break;
                }
            }
            first_declarator = false;
            SourceLocation declarator_start_loc = currentTokenInfo.location;
            auto declarator_node = make_ast_node<VariableDeclaratorNode>(declarator_start_loc);
            if (check_token(TokenType::Identifier))
            {
                declarator_node->name = create_identifier_node(currentTokenInfo);
                advance_and_lex();
            }
            else
            {
                record_error_at_current("Expected identifier for variable name.");
                auto dummy_ident = make_ast_node<IdentifierNode>(currentTokenInfo.location);
                dummy_ident->name = "_ERROR_VAR_NAME_";
                finalize_node_location(dummy_ident);
                declarator_node->name = dummy_ident;
                if (!check_token(TokenType::Assign) && !check_token(TokenType::Comma) && !check_token(TokenType::Semicolon))
                {
                    break;
                }
            }
            if (check_token(TokenType::Assign))
            {
                declarator_node->equalsToken = create_token_node(TokenType::Assign, currentTokenInfo);
                advance_and_lex();
                std::shared_ptr<ExpressionNode> initializer_expr = parse_expression();
                if (initializer_expr)
                {
                    declarator_node->initializer = initializer_expr;
                }
                else
                {
                    record_error_at_current("Invalid initializer expression for variable.");
                }
            }
            finalize_node_location(declarator_node);
            var_decl_node->declarators.push_back(declarator_node);
        } while (check_token(TokenType::Comma));
        if (check_token(TokenType::Semicolon))
        {
            var_decl_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            SourceLocation error_loc = previousTokenInfo.location;
            if (!var_decl_node->declarators.empty() && var_decl_node->declarators.back()->location.has_value())
            {
                error_loc.lineStart = var_decl_node->declarators.back()->location.value().lineEnd;
                error_loc.columnStart = var_decl_node->declarators.back()->location.value().columnEnd + 1;
            }
            else if (var_decl_node->type && var_decl_node->type->location.has_value())
            {
                error_loc.lineStart = var_decl_node->type->location.value().lineEnd;
                error_loc.columnStart = var_decl_node->type->location.value().columnEnd + 1;
            }
            record_error("Expected ';' after variable declaration.", error_loc);
        }
        finalize_node_location(var_decl_node);
        return var_decl_node;
    }

    std::shared_ptr<ReturnStatementNode> ScriptParser::parse_return_statement()
    {
        SourceLocation statement_start_loc = currentTokenInfo.location;
        auto return_node = make_ast_node<ReturnStatementNode>(statement_start_loc);
        if (check_token(TokenType::Return))
        {
            return_node->returnKeyword = create_token_node(TokenType::Return, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Internal Parser Error: parse_return_statement called without 'return' token.");
        }
        if (!check_token(TokenType::Semicolon) && !is_at_end_of_token_stream())
        {
            std::shared_ptr<ExpressionNode> expression = parse_expression();
            if (expression)
            {
                return_node->expression = expression;
            }
            else
            {
                record_error_at_current("Invalid expression for return statement.");
            }
        }
        if (check_token(TokenType::Semicolon))
        {
            return_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            SourceLocation error_loc = previousTokenInfo.location;
            if (return_node->expression.has_value() && return_node->expression.value()->location.has_value())
            {
                error_loc.lineStart = return_node->expression.value()->location.value().lineEnd;
                error_loc.columnStart = return_node->expression.value()->location.value().columnEnd + 1;
            }
            else if (return_node->returnKeyword && return_node->returnKeyword->location.has_value())
            {
                error_loc.lineStart = return_node->returnKeyword->location.value().lineEnd;
                error_loc.columnStart = return_node->returnKeyword->location.value().columnEnd + 1;
            }
            record_error("Expected ';' after return statement.", error_loc);
        }
        finalize_node_location(return_node);
        return return_node;
    }

    std::shared_ptr<BreakStatementNode> ScriptParser::parse_break_statement()
    {
        SourceLocation statement_start_loc = currentTokenInfo.location;
        auto break_node = make_ast_node<BreakStatementNode>(statement_start_loc);
        if (check_token(TokenType::Break))
        {
            break_node->breakKeyword = create_token_node(TokenType::Break, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Internal Parser Error: parse_break_statement called without 'break' token.");
        }
        if (check_token(TokenType::Semicolon))
        {
            break_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            SourceLocation error_loc = previousTokenInfo.location;
            if (break_node->breakKeyword && break_node->breakKeyword->location.has_value())
            {
                error_loc.lineStart = break_node->breakKeyword->location.value().lineEnd;
                error_loc.columnStart = break_node->breakKeyword->location.value().columnEnd + 1;
            }
            record_error("Expected ';' after break statement.", error_loc);
        }
        finalize_node_location(break_node);
        return break_node;
    }

    std::shared_ptr<ContinueStatementNode> ScriptParser::parse_continue_statement()
    {
        SourceLocation statement_start_loc = currentTokenInfo.location;
        auto continue_node = make_ast_node<ContinueStatementNode>(statement_start_loc);
        if (check_token(TokenType::Continue))
        {
            continue_node->continueKeyword = create_token_node(TokenType::Continue, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Internal Parser Error: parse_continue_statement called without 'continue' token.");
        }
        if (check_token(TokenType::Semicolon))
        {
            continue_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            SourceLocation error_loc = previousTokenInfo.location;
            if (continue_node->continueKeyword && continue_node->continueKeyword->location.has_value())
            {
                error_loc.lineStart = continue_node->continueKeyword->location.value().lineEnd;
                error_loc.columnStart = continue_node->continueKeyword->location.value().columnEnd + 1;
            }
            record_error("Expected ';' after continue statement.", error_loc);
        }
        finalize_node_location(continue_node);
        return continue_node;
    }

    std::shared_ptr<ExpressionStatementNode> ScriptParser::parse_expression_statement()
    {
        SourceLocation statement_start_loc = currentTokenInfo.location;
        auto expr_stmt_node = make_ast_node<ExpressionStatementNode>(statement_start_loc);
        std::shared_ptr<ExpressionNode> expression = parse_expression();
        if (expression)
        {
            expr_stmt_node->expression = expression;
        }
        else
        {
            if (errors.empty() || !(errors.back().location.lineStart == statement_start_loc.lineStart && errors.back().location.columnStart == statement_start_loc.columnStart))
            {
                record_error_at_current("Invalid expression for expression statement.");
            }
        }
        if (check_token(TokenType::Semicolon))
        {
            expr_stmt_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
        }
        else
        {
            SourceLocation error_loc = expr_stmt_node->expression ? previousTokenInfo.location : currentTokenInfo.location;
            if (expr_stmt_node->expression && expr_stmt_node->expression->location.has_value())
            {
                error_loc.lineStart = expr_stmt_node->expression->location.value().lineEnd;
                error_loc.columnStart = expr_stmt_node->expression->location.value().columnEnd + 1;
            }
            record_error("Expected ';' after expression statement.", error_loc);
        }
        finalize_node_location(expr_stmt_node);
        return expr_stmt_node;
    }

    std::shared_ptr<StatementNode> ScriptParser::parse_statement()
    {
        if (is_at_end_of_token_stream())
        {
            return nullptr;
        }
        if (check_token(TokenType::OpenBrace))
        {
            return parse_block_statement();
        }
        else if (check_token(TokenType::Return))
        {
            return parse_return_statement();
        }
        else if (check_token(TokenType::Break))
        {
            return parse_break_statement();
        }
        else if (check_token(TokenType::Continue))
        {
            return parse_continue_statement();
        }
        else if (check_token(TokenType::If))
        {
            return parse_if_statement();
        }
        else if (check_token(TokenType::While))
        {
            return parse_while_statement();
        }
        else if (check_token(TokenType::For))
        {
            return parse_for_statement();
        }
        else if (check_token(TokenType::Var) || check_token(TokenType::Bool) || check_token(TokenType::Int) || check_token(TokenType::String) || check_token(TokenType::Long) || check_token(TokenType::Double) || check_token(TokenType::Char) || check_token(TokenType::Float))
        {
            return parse_local_variable_declaration_statement();
        }
        else if (check_token(TokenType::Identifier))
        {
            size_t o_co_la = currentCharOffset;
            int o_l_la = currentLine;
            int o_c_la = currentColumn;
            size_t o_lso_la = currentLineStartOffset;
            CurrentTokenInfo o_cti_la = currentTokenInfo;
            CurrentTokenInfo o_pti_la = previousTokenInfo;
            advance_and_lex();
            bool is_fby_id = check_token(TokenType::Identifier);
            currentCharOffset = o_co_la;
            currentLine = o_l_la;
            currentColumn = o_c_la;
            currentLineStartOffset = o_lso_la;
            currentTokenInfo = o_cti_la;
            previousTokenInfo = o_pti_la;
            if (is_fby_id)
            {
                return parse_local_variable_declaration_statement();
            }
        }
        else if (check_token(TokenType::Semicolon))
        {
            auto empty_stmt_node = make_ast_node<ExpressionStatementNode>(currentTokenInfo.location);
            empty_stmt_node->expression = nullptr;
            empty_stmt_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
            finalize_node_location(empty_stmt_node);
            return empty_stmt_node;
        }
        if (currentTokenInfo.type != TokenType::EndOfFile && currentTokenInfo.type != TokenType::CloseBrace)
        {
            return parse_expression_statement();
        }
        if (!is_at_end_of_token_stream() && currentTokenInfo.type != TokenType::CloseBrace)
        {
            record_error_at_current("Unexpected token '" + std::string(currentTokenInfo.lexeme) + "' when expecting a statement.");
            advance_and_lex();
        }
        return nullptr;
    }

    std::shared_ptr<ExpressionNode> ScriptParser::parse_expression() { return parse_assignment_expression(); }

    std::shared_ptr<ExpressionNode> ScriptParser::parse_postfix_expression()
    {
        std::shared_ptr<ExpressionNode> left_expr = parse_primary_expression();
        if (!left_expr)
        {
            return left_expr;
        }
        SourceLocation overall_start_loc = left_expr->location.value_or(previousTokenInfo.location);
        while (true)
        {
            if (check_token(TokenType::Dot))
            {
                auto member_access_node = make_ast_node<MemberAccessExpressionNode>(overall_start_loc);
                member_access_node->target = left_expr;
                member_access_node->dotToken = create_token_node(TokenType::Dot, currentTokenInfo);
                advance_and_lex();
                if (check_token(TokenType::Identifier))
                {
                    member_access_node->memberName = create_identifier_node(currentTokenInfo);
                    advance_and_lex();
                }
                else
                {
                    record_error_at_current("Expected identifier for member name after '.'.");
                    auto dummy_ident = make_ast_node<IdentifierNode>(currentTokenInfo.location);
                    dummy_ident->name = "_ERROR_MEMBER_";
                    finalize_node_location(dummy_ident);
                    member_access_node->memberName = dummy_ident;
                }
                finalize_node_location(member_access_node);
                left_expr = member_access_node;
                overall_start_loc = left_expr->location.value_or(overall_start_loc);
            }
            else if (check_token(TokenType::OpenBracket))
            {
                auto indexer_node = make_ast_node<IndexerExpressionNode>(overall_start_loc);
                indexer_node->target = left_expr;
                indexer_node->openBracketToken = create_token_node(TokenType::OpenBracket, currentTokenInfo);
                advance_and_lex();
                indexer_node->indexExpression = parse_expression();
                if (check_token(TokenType::CloseBracket))
                {
                    indexer_node->closeBracketToken = create_token_node(TokenType::CloseBracket, currentTokenInfo);
                    advance_and_lex();
                }
                else
                {
                    record_error_at_current("Expected ']' to close indexer expression.");
                }
                finalize_node_location(indexer_node);
                left_expr = indexer_node;
                overall_start_loc = left_expr->location.value_or(overall_start_loc);
            }
            else if (check_token(TokenType::LessThan) || check_token(TokenType::OpenParen))
            {
                bool actually_parse_as_generic_call = check_token(TokenType::LessThan) && can_parse_as_generic_arguments_followed_by_call();
                if (actually_parse_as_generic_call || check_token(TokenType::OpenParen))
                {
                    auto call_node = make_ast_node<MethodCallExpressionNode>(overall_start_loc);
                    call_node->target = left_expr;
                    if (actually_parse_as_generic_call)
                    {
                        call_node->genericOpenAngleBracketToken = create_token_node(TokenType::LessThan, currentTokenInfo);
                        advance_and_lex();
                        std::vector<std::shared_ptr<TypeNameNode>> type_args_vec;
                        std::vector<std::shared_ptr<TokenNode>> type_arg_commas_vec;
                        if (!check_token(TokenType::GreaterThan))
                        {
                            do
                            {
                                type_args_vec.push_back(parse_type_name());
                                if (check_token(TokenType::Comma))
                                {
                                    type_arg_commas_vec.push_back(create_token_node(TokenType::Comma, currentTokenInfo));
                                    advance_and_lex();
                                }
                                else
                                {
                                    break;
                                }
                            } while (!check_token(TokenType::GreaterThan) && !is_at_end_of_token_stream());
                        }
                        call_node->typeArguments = type_args_vec;
                        call_node->typeArgumentCommas = type_arg_commas_vec;
                        if (check_token(TokenType::GreaterThan))
                        {
                            call_node->genericCloseAngleBracketToken = create_token_node(TokenType::GreaterThan, currentTokenInfo);
                            advance_and_lex();
                        }
                        else
                        {
                            record_error_at_current("Expected '>' to close generic type argument list for method call.");
                        }
                    }
                    if (check_token(TokenType::OpenParen))
                    {
                        auto arg_list_opt = parse_argument_list();
                        if (arg_list_opt)
                        {
                            call_node->argumentList = arg_list_opt.value();
                        }
                        else
                        { /* error handling for arg list */
                        }
                    }
                    else if (call_node->genericOpenAngleBracketToken)
                    {
                        record_error_at_current("Expected '(' for arguments after generic type arguments in method call."); /* Create dummy arg list */
                    }
                    else
                    {
                        break;
                    }
                    finalize_node_location(call_node);
                    left_expr = call_node;
                    overall_start_loc = left_expr->location.value_or(overall_start_loc);
                }
                else
                {
                    break;
                }
            }
            else if (check_token(TokenType::Increment) || check_token(TokenType::Decrement))
            {
                auto unary_node = make_ast_node<UnaryExpressionNode>(overall_start_loc);
                unary_node->operand = left_expr;
                unary_node->operatorToken = create_token_node(currentTokenInfo.type, currentTokenInfo);
                unary_node->isPostfix = true;
                if (currentTokenInfo.type == TokenType::Increment)
                {
                    unary_node->opKind = UnaryOperatorKind::PostIncrement;
                }
                else
                {
                    unary_node->opKind = UnaryOperatorKind::PostDecrement;
                }
                advance_and_lex();
                finalize_node_location(unary_node);
                left_expr = unary_node;
                overall_start_loc = left_expr->location.value_or(overall_start_loc);
            }
            else
            {
                break;
            }
        }
        return left_expr;
    }

} // namespace Mycelium::Scripting::Lang
