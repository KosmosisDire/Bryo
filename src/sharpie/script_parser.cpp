#include "script_parser.hpp"
#include "script_ast.hpp"
#include <iostream>
#include <cctype>

namespace Mycelium::Scripting::Lang
{

    ScriptParser::ScriptParser(std::string_view source_code, std::string_view fileName)
        : sourceCode(source_code),
          fileName(fileName),
          currentCharOffset(0),
          currentLine(1),
          currentColumn(1),
          currentLineStartOffset(0)
    {
        // Initialize currentTokenInfo to something representing "before start" or an initial Error state
        currentTokenInfo.type = TokenType::Error; // Or a special "Start" token
        currentTokenInfo.location.fileName = std::string(fileName);
        previousTokenInfo = currentTokenInfo;
    }

    std::pair<std::shared_ptr<Mycelium::Scripting::Lang::CompilationUnitNode>, std::vector<Mycelium::Scripting::Lang::ParseError>> ScriptParser::parse()
    {
        errors.clear();
        currentCharOffset = 0;
        currentLine = 1;
        currentColumn = 1;
        currentLineStartOffset = 0;

        advance_and_lex(); // Prime the first token

        std::shared_ptr<CompilationUnitNode> compilation_unit_node = parse_compilation_unit();

        // Finalize the location of the compilation unit node if it was created
        if (compilation_unit_node && compilation_unit_node->location.has_value())
        {
            // The end location of the CU is the end location of the last token processed
            // or the start if the file was empty.
            // parse_compilation_unit should ideally handle its own end location based on what it consumed.
            // If it doesn't consume anything (empty file and EOF is first token), previousTokenInfo might be initial state.
            if (previousTokenInfo.type != TokenType::Error && currentTokenInfo.type == TokenType::EndOfFile)
            {
                compilation_unit_node->location->lineEnd = previousTokenInfo.location.lineEnd;
                compilation_unit_node->location->columnEnd = previousTokenInfo.location.columnEnd;
            }
            else if (currentTokenInfo.type == TokenType::EndOfFile)
            { // EOF was first token
                compilation_unit_node->location->lineEnd = currentTokenInfo.location.lineStart;
                compilation_unit_node->location->columnEnd = currentTokenInfo.location.columnStart;
            }
            // If parse_compilation_unit consumed tokens, its finalize_node_location should handle it.
            // This is a fallback if parse_compilation_unit doesn't set its own end.
        }

        return {compilation_unit_node, errors};
    }

#pragma region "Parsing"

    std::shared_ptr<CompilationUnitNode> ScriptParser::parse_compilation_unit()
    {
        SourceLocation file_start_loc = {1, 0, 1, 0, std::string(fileName)};
        if (currentTokenInfo.type != TokenType::EndOfFile)
        { // If not an empty file
            file_start_loc = currentTokenInfo.location;
        }
        else
        { // Empty file, location is just the start.
            file_start_loc.lineEnd = file_start_loc.lineStart;
            file_start_loc.columnEnd = file_start_loc.columnStart;
        }

        auto unit_node = make_ast_node<CompilationUnitNode>(file_start_loc);

        // Parse Using Directives
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

        // TODO: Handle file-scoped namespace: namespace MyNamespace; (later)
        // if (check_token(TokenType::Namespace) && /* peek for semicolon */ ) { ... }

        // Parse Namespace Member Declarations (namespaces or types)
        while (currentTokenInfo.type != TokenType::EndOfFile)
        {
            // Look for namespace or type keywords
            if (check_token(TokenType::Namespace) ||
                check_token(TokenType::Class) /* || check_token(TokenType::Struct) etc. */)
            {
                std::shared_ptr<NamespaceMemberDeclarationNode> member_decl = parse_namespace_member_declaration();
                if (member_decl)
                {
                    unit_node->members.push_back(member_decl);
                }
                else
                {
                    // Error in member declaration, parse_namespace_member_declaration should record and advance.
                    // If it didn't advance, break to avoid infinite loop.
                    SourceLocation error_loc_check = currentTokenInfo.location;
                    advance_and_lex(); // Consume problematic token
                    if (currentTokenInfo.location.lineStart == error_loc_check.lineStart &&
                        currentTokenInfo.location.columnStart == error_loc_check.columnStart &&
                        currentTokenInfo.type != TokenType::EndOfFile)
                    {
                        record_error_at_current("Parser stuck at top-level member declaration. Breaking.");
                        break;
                    }
                    // continue; // Try to parse next member
                }
            }
            else if (currentTokenInfo.type != TokenType::EndOfFile)
            {
                record_error_at_current("Unexpected token at top level. Expected namespace or type declaration.");
                advance_and_lex(); // Consume unexpected token
            }
        }

        // Finalize the location of the compilation unit.
        // If it's empty, start_loc is already set.
        // If not empty, previousTokenInfo holds the last successfully consumed token.
        if (!unit_node->usings.empty() || !unit_node->members.empty())
        {
            finalize_node_location(unit_node);
        }
        else if (currentTokenInfo.type == TokenType::EndOfFile && unit_node->location.has_value())
        {
            // If file was not empty but contained no valid top-level elements,
            // or only comments, set end to EOF location.
            unit_node->location.value().lineEnd = currentTokenInfo.location.lineStart;
            unit_node->location.value().columnEnd = currentTokenInfo.location.columnStart;
            if (previousTokenInfo.type != TokenType::Error)
            { // If previousTokenInfo is valid
                unit_node->location.value().lineEnd = previousTokenInfo.location.lineEnd;
                unit_node->location.value().columnEnd = previousTokenInfo.location.columnEnd;
            }
        }
        // Else: empty file, initial location is fine.

        return unit_node;
    }

    std::shared_ptr<UsingDirectiveNode> ScriptParser::parse_using_directive()
    {
        SourceLocation directive_start_loc = currentTokenInfo.location; // Location of 'using'
        auto using_node = make_ast_node<UsingDirectiveNode>(directive_start_loc);

        using_node->usingKeyword = create_token_node(TokenType::Using, currentTokenInfo);
        consume_token(TokenType::Using, "Expected 'using' keyword."); // Consumes 'using'

        // The 'namespaceName' in UsingDirectiveNode is a variant of IdentifierNode or QualifiedNameNode.
        // parse_type_name() returns a TypeNameNode. We need to extract the name part.
        // A TypeNameNode can hold an IdentifierNode or a QualifiedNameNode in its name_segment.
        // It shouldn't have array specifiers or generic args for a 'using Namespace;' directive.

        std::shared_ptr<TypeNameNode> parsed_name_as_type = parse_type_name();

        if (parsed_name_as_type)
        {
            if (parsed_name_as_type->is_array() || parsed_name_as_type->openAngleBracketToken.has_value())
            {
                record_error("Namespace name in 'using' directive cannot have array specifiers or generic arguments.",
                             parsed_name_as_type->location.value_or(currentTokenInfo.location));
            }
            // Assign to the variant in UsingDirectiveNode
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
                // Create a dummy identifier if necessary for AST structure
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
            advance_and_lex(); // Consume ';'
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
            decl_start_loc = currentTokenInfo.location; // No modifiers, start is current token
        }

        // Now check for Namespace or Class keyword AFTER modifiers
        if (check_token(TokenType::Namespace))
        {
            // Namespaces typically don't have modifiers like public/private in C# at this level
            // (though internal might be possible depending on language spec).
            // If modifiers were parsed, it's likely an error for a namespace here.
            if (!modifiers.empty())
            {
                record_error("Modifiers are not typically allowed on namespace declarations here.", decl_start_loc);
            }
            // parse_namespace_declaration should be called without passing modifiers,
            // or it needs to handle them (e.g., for future features).
            // For now, let's assume parse_namespace_declaration doesn't expect them from here.
            return parse_namespace_declaration(); // This will re-evaluate current token
        }
        else if (check_token(TokenType::Class) /* || check_token(TokenType::Struct) || ... */)
        {
            // Pass the already parsed decl_start_loc (which accounts for modifiers)
            // and the modifiers themselves to a modified parse_type_declaration or directly
            // to parse_class_declaration if it's adapted.
            // Let's have parse_type_declaration take them.
            return parse_type_declaration(decl_start_loc, std::move(modifiers));
        }
        else
        {
            // If modifiers were parsed but no valid decl keyword follows
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

        std::shared_ptr<TypeNameNode> parsed_name_holder = parse_type_name();
        if (parsed_name_holder)
        {
            if (std::holds_alternative<std::shared_ptr<IdentifierNode>>(parsed_name_holder->name_segment))
            {
                ns_node->name = std::get<std::shared_ptr<IdentifierNode>>(parsed_name_holder->name_segment);
            }
            else if (std::holds_alternative<std::shared_ptr<QualifiedNameNode>>(parsed_name_holder->name_segment))
            {
                std::shared_ptr<QualifiedNameNode> qn = std::get<std::shared_ptr<QualifiedNameNode>>(parsed_name_holder->name_segment);

                std::shared_ptr<IdentifierNode> current_ident = nullptr;
                AstNode *current_segment_owner = qn.get();

                // For now, assuming NamespaceDecl.name is just the first identifier it sees for this declaration:
                if (auto *ident_ptr = std::get_if<std::shared_ptr<IdentifierNode>>(&parsed_name_holder->name_segment))
                {
                    ns_node->name = *ident_ptr;
                }
                else if (auto *qn_ptr = std::get_if<std::shared_ptr<QualifiedNameNode>>(&parsed_name_holder->name_segment))
                {
                    // If it's a qualified name, take the leftmost simple name for this node's direct name
                    // and subsequent parsing inside will handle the rest if grammar allows `namespace A.B {}`
                    // to mean `namespace A { namespace B {} }`.
                    // Or, if `namespace A.B.C;` (file scoped), then the QN is the name.
                    // This part needs to align with how `parse_type_name` handles qualified names and what
                    // the AST expects for `NamespaceDeclarationNode::name`.
                    // Your `parse_type_name` builds a right-associative QN.
                    // For `namespace Foo.Bar;`, `parsed_name_holder`'s `name_segment` would be a QN where `left` is `TypeName(Foo)` and `right` is `Bar`.
                    // Let's assume for NamespaceDeclarationNode, the `name` field should store the *entire* qualified name if one is parsed.
                    // This means `NamespaceDeclarationNode::name` should ideally be a `std::variant<Ident, QN>` or you extract it carefully.
                    // Given `DeclarationNode::name` is `IdentifierNode`, this is a current limitation.
                    // For now, taking rightmost as a quick fix for the immediate problem, but this needs revisiting.
                    AstNode *current_seg_node_ptr = (*qn_ptr).get();
                    while (auto current_qn_as_qn = dynamic_cast<QualifiedNameNode *>(current_seg_node_ptr))
                    {
                        if (auto ident_right = std::dynamic_pointer_cast<IdentifierNode>(current_qn_as_qn->right))
                        {
                            ns_node->name = ident_right;
                            // This is still picking the rightmost. If name is "A.B.C", name will be "C".
                            // This is probably not what we want for the `name` field of NamespaceDeclarationNode
                            // if it's meant to be the full name.
                            // Let's assume for this pass, it should be the first identifier.
                            // To get the first identifier of a qualified name from parse_type_name:
                            // We need to traverse left.
                            std::shared_ptr<TypeNameNode> leftmost_type_name_node = parsed_name_holder;
                            while (std::holds_alternative<std::shared_ptr<QualifiedNameNode>>(leftmost_type_name_node->name_segment))
                            {
                                leftmost_type_name_node = std::get<std::shared_ptr<QualifiedNameNode>>(leftmost_type_name_node->name_segment)->left;
                            }
                            if (std::holds_alternative<std::shared_ptr<IdentifierNode>>(leftmost_type_name_node->name_segment))
                            {
                                ns_node->name = std::get<std::shared_ptr<IdentifierNode>>(leftmost_type_name_node->name_segment);
                            }
                            else
                            { /* Should not happen if TypeName is well-formed */
                            }
                            break;
                        }
                        // This part of the QN traversal seems flawed for getting a single ident.
                        // Let's simplify the assumption for now based on existing AST
                        ns_node->name = (*qn_ptr)->right; // Takes the rightmost part of the *first* QN
                        break;
                    }
                }

                if (!ns_node->name)
                {
                    record_error("Could not extract identifier for namespace name.", parsed_name_holder->location.value_or(currentTokenInfo.location));
                    ns_node->name = make_ast_node<IdentifierNode>(parsed_name_holder->location.value_or(currentTokenInfo.location));
                    ns_node->name->name = "_ERROR_NS_QUAL_";
                }
            }
            else
            {
                record_error_at_current("Invalid name for namespace declaration.");
                ns_node->name = make_ast_node<IdentifierNode>(currentTokenInfo.location);
                ns_node->name->name = "_ERROR_NS_NAME_";
            }
            if (ns_node->name)
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
            // File-scoped namespace like "namespace MyNamespace;"
            // The CompilationUnitNode should have a specific way to handle this,
            // e.g. a dedicated field for fileScopedNamespaceName.
            // For now, this ns_node is created, but its members list will be empty.
            // The 'file-scoped' nature is identified by the semicolon.
            // For CompilationUnitNode structure:
            // unit_node->fileScopedNamespaceKeyword = ns_node->namespaceKeyword; (conceptually)
            // unit_node->fileScopedNamespaceName = ns_node->name; (if name can be QN)
            // unit_node->fileScopeNamespaceSemicolon = create_token_node(...);
            // This ns_node itself might not be added to unit_node->members if CU handles it specially.
            // For now, we just create it and consume the semicolon.
            ns_node->openBraceToken = nullptr;
            ns_node->closeBraceToken = nullptr;
            // Note: This ns_node won't hold the semicolon token itself.
            // If the AST requires NamespaceDeclarationNode to hold this, it needs a field.
            // CompilationUnitNode has fileScopeNamespaceSemicolon.
            advance_and_lex(); // Consume ';'
        }
        else if (check_token(TokenType::OpenBrace))
        {
            ns_node->openBraceToken = create_token_node(TokenType::OpenBrace, currentTokenInfo);
            advance_and_lex(); // Consume '{'

            // --- ADDED: Parse Using Directives within the namespace ---
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
            // --- END ADDED SECTION ---

            while (!check_token(TokenType::CloseBrace) && !is_at_end_of_token_stream())
            {
                // Now, parse_namespace_member_declaration will be called for namespaces or types
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
                advance_and_lex(); // Consume '}'
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
    std::shared_ptr<TypeDeclarationNode> ScriptParser::parse_type_declaration(
        const SourceLocation &decl_start_loc,
        std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers)
    {
        if (check_token(TokenType::Class))
        {
            return parse_class_declaration(decl_start_loc, std::move(modifiers));
        }
        /* else if (struct, enum, etc.) */
        else
        {
            record_error_at_current("Expected type declaration keyword (e.g., 'class') after modifiers.");
            return nullptr;
        }
    }

    std::shared_ptr<ClassDeclarationNode> ScriptParser::parse_class_declaration(
        const SourceLocation &decl_start_loc, // This is the true start (first modifier or 'class')
        std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers)
    {
        auto class_node = make_ast_node<ClassDeclarationNode>(decl_start_loc);
        class_node->modifiers = std::move(modifiers); // Use passed-in modifiers

        // Store class name for constructor parsing context
        std::optional<std::string> previous_class_name_context = m_current_class_name;

        // 'class' keyword is expected next
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
            m_current_class_name = class_node->name->name; // Set current class name
            advance_and_lex();
        }
        else
        {
            record_error_at_current("Expected identifier for class name.");
            class_node->name = make_ast_node<IdentifierNode>(currentTokenInfo.location);
            class_node->name->name = "_ERROR_CLASS_NAME_";
            finalize_node_location(class_node->name);
        }

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
        m_current_class_name = previous_class_name_context; // Restore previous class name context
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

            // TODO: Add other modifier keywords like protected, internal, readonly, virtual, override, abstract, sealed
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
            else
            {
                is_modifier_keyword = false;
                continue_parsing_modifiers = false; // Stop if it's not a modifier
            }

            if (is_modifier_keyword)
            {
                // Check for duplicate modifiers (optional, could be a semantic check)
                for (const auto &mod_pair : parsed_modifiers)
                {
                    if (mod_pair.first == kind)
                    {
                        record_error_at_current("Duplicate modifier '" + std::string(currentTokenInfo.lexeme) + "'.");
                        // Skip adding it again, but consume the token
                    }
                }
                // Add the modifier even if duplicate for now, semantic analysis can handle stricter rules.
                // Or, only add if not duplicate:
                // if (std::find_if(parsed_modifiers.begin(), parsed_modifiers.end(),
                //     [&](const auto& p){ return p.first == kind; }) == parsed_modifiers.end()) {
                //    parsed_modifiers.push_back({kind, create_token_node(currentTokenInfo.type, currentTokenInfo)});
                // }
                parsed_modifiers.push_back({kind, create_token_node(currentTokenInfo.type, currentTokenInfo)});
                advance_and_lex(); // Consume the modifier keyword
            }
        }
        return parsed_modifiers;
    }
    std::shared_ptr<ParameterDeclarationNode> ScriptParser::parse_parameter_declaration()
    {
        SourceLocation param_start_loc = currentTokenInfo.location;
        // TODO: Parse parameter modifiers if your language supports them (e.g., ref, out, params)
        // std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> param_modifiers = parse_parameter_modifiers();
        // if (!param_modifiers.empty()) param_start_loc = ...

        auto param_node = make_ast_node<ParameterDeclarationNode>(param_start_loc);
        // param_node->modifiers = param_modifiers; // If you add param modifiers

        param_node->type = parse_type_name();
        if (!param_node->type)
        {
            record_error_at_current("Expected type name for parameter.");
            // Create a dummy type to allow parsing to potentially continue
            auto dummy_type = make_ast_node<TypeNameNode>(currentTokenInfo.location);
            auto dummy_ident_type = make_ast_node<IdentifierNode>(currentTokenInfo.location);
            dummy_ident_type->name = "_ERROR_PARAM_TYPE_";
            finalize_node_location(dummy_ident_type);
            dummy_type->name_segment = dummy_ident_type;
            finalize_node_location(dummy_type);
            param_node->type = dummy_type;
            // Don't advance here, let name parsing try
        }

        if (check_token(TokenType::Identifier))
        {
            param_node->name = create_identifier_node(currentTokenInfo); // Inherited from DeclarationNode
            advance_and_lex();                                           // Consume parameter name
        }
        else
        {
            record_error_at_current("Expected identifier for parameter name.");
            param_node->name = make_ast_node<IdentifierNode>(currentTokenInfo.location);
            param_node->name->name = "_ERROR_PARAM_NAME_";
            finalize_node_location(param_node->name);
        }

        if (check_token(TokenType::Assign)) // Optional default value
        {
            param_node->equalsToken = create_token_node(TokenType::Assign, currentTokenInfo);
            advance_and_lex(); // Consume '='
            param_node->defaultValue = parse_expression();
            if (!param_node->defaultValue)
            {
                record_error_at_current("Invalid default value expression for parameter.");
            }
        }

        finalize_node_location(param_node);
        return param_node;
    }

    std::optional<std::vector<std::shared_ptr<ParameterDeclarationNode>>> ScriptParser::parse_parameter_list_content(
        std::vector<std::shared_ptr<TokenNode>> &commas_list) // Out parameter
    {
        std::vector<std::shared_ptr<ParameterDeclarationNode>> parameters;
        commas_list.clear();

        // This function is called when already inside potential parameters (not on '(' or ')' initially)
        // It expects to parse at least one parameter if called and current token is not ')'
        // Or it can be called even if it might be an empty list e.g. `()`

        if (check_token(TokenType::CloseParen))
        { // Handles empty list like `()`
            return parameters;
        }

        // Parse first parameter
        std::shared_ptr<ParameterDeclarationNode> first_param = parse_parameter_declaration();
        if (first_param)
        {
            parameters.push_back(first_param);
        }
        else
        {
            record_error_at_current("Failed to parse first parameter in list.");
            return std::nullopt; // Critical failure
        }

        // Parse subsequent parameters (if any)
        while (check_token(TokenType::Comma))
        {
            commas_list.push_back(create_token_node(TokenType::Comma, currentTokenInfo));
            advance_and_lex(); // Consume ','

            // After a comma, another parameter is expected.
            if (check_token(TokenType::CloseParen))
            { // e.g. (int x, )
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

        // --- Lookahead for constructor or method/field ---
        CurrentTokenInfo potential_type_or_name_token = currentTokenInfo;

        bool is_potential_constructor_name = m_current_class_name.has_value() &&
                                           check_token(TokenType::Identifier) &&
                                           potential_type_or_name_token.lexeme == m_current_class_name.value();

        // Look ahead one token after the potential_type_or_name_token
        size_t original_char_offset_lookahead = currentCharOffset;
        int original_line_lookahead = currentLine;
        int original_column_lookahead = currentColumn;
        size_t original_line_start_offset_lookahead = currentLineStartOffset;
        CurrentTokenInfo original_current_token_info_lookahead = currentTokenInfo;
        CurrentTokenInfo original_previous_token_info_lookahead = previousTokenInfo;
        
        // Tentatively consume potential_type_or_name_token to peek next
        // This advance_and_lex() is part of the lookahead.
        if (check_token(TokenType::Identifier) || check_token(TokenType::Void) || 
            check_token({TokenType::Bool, TokenType::Int, TokenType::String, TokenType::Long, TokenType::Double, TokenType::Char, TokenType::Float})) {
            // Only advance if it's a token that could start a type or be a constructor name
             advance_and_lex(); 
        }
       
        bool is_followed_by_open_paren = check_token(TokenType::OpenParen);
        bool is_followed_by_less_than = check_token(TokenType::LessThan); // For generic methods/constructors

        // Restore state so currentTokenInfo is potential_type_or_name_token again
        currentCharOffset = original_char_offset_lookahead;
        currentLine = original_line_lookahead;
        currentColumn = original_column_lookahead;
        currentLineStartOffset = original_line_start_offset_lookahead;
        currentTokenInfo = original_current_token_info_lookahead;
        previousTokenInfo = original_previous_token_info_lookahead;
        // --- End Lookahead ---

        if (is_potential_constructor_name && (is_followed_by_open_paren || is_followed_by_less_than))
        {
            // It's a constructor. potential_type_or_name_token is the constructor name.
            // currentTokenInfo is still at the constructor name token.
            return parse_constructor_declaration(member_start_loc, std::move(modifiers), currentTokenInfo);
        }
        else
        {
            // It's a method or a field. Parse the type first.
            std::shared_ptr<TypeNameNode> type;
            if (check_token(TokenType::Void) ||
                check_token(TokenType::Identifier) || // Could be a custom type name
                check_token({TokenType::Bool, TokenType::Int, TokenType::String, TokenType::Long, TokenType::Double, TokenType::Char, TokenType::Float}))
            {
                type = parse_type_name(); // This consumes the type tokens
            }
            else
            {
                record_error_at_current("Expected type name, 'void', or constructor name at start of member declaration.");
                return nullptr;
            }

            if (!type)
            {
                // Error already recorded by parse_type_name or the check above
                return nullptr;
            }

            // Now, the current token should be the member name (identifier).
            if (check_token(TokenType::Identifier))
            {
                CurrentTokenInfo name_token_info = currentTokenInfo; // This is the member name.

                // --- Lookahead again, this time just for '(' or '<' after the name ---
                original_char_offset_lookahead = currentCharOffset;
                original_line_lookahead = currentLine;
                original_column_lookahead = currentColumn;
                original_line_start_offset_lookahead = currentLineStartOffset;
                original_current_token_info_lookahead = currentTokenInfo;
                original_previous_token_info_lookahead = previousTokenInfo;

                advance_and_lex(); // Tentatively consume the name_token_info
                bool is_method_like_signature_after_name = check_token(TokenType::OpenParen) || check_token(TokenType::LessThan);

                currentCharOffset = original_char_offset_lookahead;
                currentLine = original_line_lookahead;
                currentColumn = original_column_lookahead;
                currentLineStartOffset = original_line_start_offset_lookahead;
                currentTokenInfo = original_current_token_info_lookahead;
                previousTokenInfo = original_previous_token_info_lookahead;
                // --- End Lookahead ---

                if (is_method_like_signature_after_name)
                {
                    return parse_method_declaration(member_start_loc, std::move(modifiers), type, name_token_info);
                }
                else
                {
                    // It's a field. currentTokenInfo is still the identifier (name_token_info).
                    // parse_field_declaration will consume this identifier.
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

    void ScriptParser::parse_base_method_declaration_parts(
        std::shared_ptr<BaseMethodDeclarationNode> method_node,
        const CurrentTokenInfo &method_name_token_info)
    {
        // Method name was identified by the caller.
        // The current token at entry here is the method name identifier.
        method_node->name = create_identifier_node(method_name_token_info);
        advance_and_lex(); // Consume method/constructor name identifier

        // TODO: Parse Generic Method Parameters <T, U> (e.g., void MyMethod<T>(T value))
        // if (check_token(TokenType::LessThan)) {
        //     method_node->genericOpenAngleBracketToken = create_token_node(TokenType::LessThan, currentTokenInfo);
        //     advance_and_lex(); // Consume '<'
        //     // ... parse type parameters ...
        //     if (check_token(TokenType::GreaterThan)) {
        //         method_node->genericCloseAngleBracketToken = create_token_node(TokenType::GreaterThan, currentTokenInfo);
        //         advance_and_lex(); // Consume '>'
        //     } else {
        //         record_error_at_current("Expected '>' to close generic parameter list for " + std::string(method_name_token_info.lexeme) + ".");
        //     }
        // }

        // Parse Parameter List
        if (check_token(TokenType::OpenParen))
        {
            method_node->openParenToken = create_token_node(TokenType::OpenParen, currentTokenInfo);
            advance_and_lex(); // Consume '('

            std::vector<std::shared_ptr<TokenNode>> param_commas;
            std::optional<std::vector<std::shared_ptr<ParameterDeclarationNode>>> params_opt = parse_parameter_list_content(param_commas);

            if (params_opt.has_value())
            {
                method_node->parameters = params_opt.value();
                method_node->parameterCommas = param_commas;
            }
            // else: error already recorded by helper

            if (check_token(TokenType::CloseParen))
            {
                method_node->closeParenToken = create_token_node(TokenType::CloseParen, currentTokenInfo);
                advance_and_lex(); // Consume ')'
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
        // TODO: Parse method constraints (where T : IConstraint)
    }
    
    std::shared_ptr<ConstructorDeclarationNode> ScriptParser::parse_constructor_declaration(
        const SourceLocation &decl_start_loc,
        std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers,
        const CurrentTokenInfo &constructor_name_token_info)
    {
        auto ctor_node = make_ast_node<ConstructorDeclarationNode>(decl_start_loc);
        ctor_node->modifiers = std::move(modifiers);
        // Constructors don't have an explicit return type in 'type' field of BaseMethodDeclarationNode.
        // Their 'name' is the class name.

        parse_base_method_declaration_parts(ctor_node, constructor_name_token_info);

        // Parse Constructor Body
        if (check_token(TokenType::OpenBrace))
        {
            ctor_node->body = parse_block_statement();
        }
        else if (check_token(TokenType::Semicolon)) { 
            ctor_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex();
            // This might be for an extern constructor or an error if not supported.
            // For now, just note it. Semantic analysis would validate.
        }
        else
        {
            record_error_at_current("Expected '{' for constructor body or ';' for declaration.");
        }

        finalize_node_location(ctor_node);
        return ctor_node;
    }

    std::shared_ptr<MethodDeclarationNode> ScriptParser::parse_method_declaration(
        const SourceLocation &decl_start_loc,
        std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers,
        std::shared_ptr<TypeNameNode> return_type,
        const CurrentTokenInfo &method_name_token_info)
    {
        auto method_node = make_ast_node<MethodDeclarationNode>(decl_start_loc);
        method_node->modifiers = std::move(modifiers);
        method_node->type = return_type; // Set the return type

        parse_base_method_declaration_parts(method_node, method_name_token_info);
        
        // Parse Method Body or Semicolon
        if (check_token(TokenType::OpenBrace))
        {
            method_node->body = parse_block_statement();
        }
        else if (check_token(TokenType::Semicolon))
        {
            method_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex(); // Consume ';'
            // This indicates an abstract method, interface method, or potentially an error if not in such context.
        }
        else
        {
            record_error_at_current("Expected '{' for method body or ';' for declaration.");
        }

        finalize_node_location(method_node);
        return method_node;
    }

    std::shared_ptr<ExternalMethodDeclarationNode> ScriptParser::parse_external_method_declaration()
    {
        SourceLocation start_loc = currentTokenInfo.location;
        auto extern_method_node = make_ast_node<ExternalMethodDeclarationNode>(start_loc);

        if (check_token(TokenType::Extern))
        {
            extern_method_node->externKeyword = create_token_node(TokenType::Extern, currentTokenInfo);
            advance_and_lex(); // Consume 'extern'
        }
        else
        {
            record_error_at_current("Expected 'extern' keyword for external method declaration.");
            return nullptr; // Cannot proceed without 'extern'
        }

        auto type = parse_type_name();
        if (!type)
        {
            record_error_at_current("Expected type name for external method declaration.");
            return nullptr; // Type is mandatory
        }
        extern_method_node->type = type; // Set return type

        if (!check_token(TokenType::Identifier)) {
            record_error_at_current("Expected identifier for external method name.");
            return nullptr;
        }
        CurrentTokenInfo name_token_info = currentTokenInfo; 

        parse_base_method_declaration_parts(extern_method_node, name_token_info);

        // Extern methods must end with a semicolon and have no body
        if (extern_method_node->body) { // Should not have a body
             record_error("External method declaration cannot have a body.", extern_method_node->body.value()->location.value());
             extern_method_node->body = std::nullopt; 
        }

        if (check_token(TokenType::Semicolon))
        {
            extern_method_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex(); // Consume ';'
        }
        else
        {
            record_error_at_current("Expected ';' to end external method declaration.");
        }
        
        finalize_node_location(extern_method_node);
        return extern_method_node;
    }

    std::shared_ptr<FieldDeclarationNode> ScriptParser::parse_field_declaration(
        const SourceLocation &decl_start_loc, // Start loc of modifiers or type
        std::vector<std::pair<ModifierKind, std::shared_ptr<TokenNode>>> modifiers,
        std::shared_ptr<TypeNameNode> type)
    {
        auto field_node = make_ast_node<FieldDeclarationNode>(decl_start_loc);
        field_node->modifiers = std::move(modifiers);
        field_node->type = type; // This is the common type for all declarators in this statement

        // The MemberDeclarationNode::name is often not used if there are multiple declarators.
        // field_node->name will remain nullopt or default unless specifically set (e.g. if only one declarator and we want to populate it)

        // Parse one or more VariableDeclaratorNodes
        bool first_declarator = true;
        do
        {
            if (!first_declarator)
            {
                if (check_token(TokenType::Comma))
                {
                    field_node->declaratorCommas.push_back(create_token_node(TokenType::Comma, currentTokenInfo));
                    advance_and_lex(); // Consume ','
                }
                else
                {
                    // This would be an error if another declarator was expected (e.g. missing semicolon)
                    // But if it's a semicolon, the loop condition (check_token(Comma)) will handle it.
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
                advance_and_lex(); // Consume identifier (field name)
            }
            else
            {
                record_error_at_current("Expected identifier for field name.");
                declarator_node->name = make_ast_node<IdentifierNode>(currentTokenInfo.location);
                declarator_node->name->name = "_ERROR_FIELD_NAME_";
                finalize_node_location(declarator_node->name);
                // Attempt to recover if possible, e.g. if '=' or ';' or ',' follows
                if (!check_token(TokenType::Assign) && !check_token(TokenType::Comma) && !check_token(TokenType::Semicolon))
                {
                    // Unlikely to recover well, break declarator parsing. Semicolon check below will fail.
                    field_node->declarators.push_back(declarator_node); // Add malformed declarator
                    goto end_declarators;                               // Use goto to break out and head for semicolon check
                }
            }

            if (check_token(TokenType::Assign))
            {
                declarator_node->equalsToken = create_token_node(TokenType::Assign, currentTokenInfo);
                advance_and_lex();                                 // Consume '='
                declarator_node->initializer = parse_expression(); // Parse initializer expression
                if (!declarator_node->initializer)
                {
                    record_error_at_current("Invalid initializer expression for field.");
                }
            }
            finalize_node_location(declarator_node);
            field_node->declarators.push_back(declarator_node);

        } while (check_token(TokenType::Comma));

    end_declarators:; // Label for goto

        if (check_token(TokenType::Semicolon))
        {
            field_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex(); // Consume ';'
        }
        else
        {
            // Error location: after the last successfully parsed part of the field declaration
            SourceLocation error_loc = previousTokenInfo.location;
            if (!field_node->declarators.empty() && field_node->declarators.back()->location.has_value())
            {
                error_loc.lineStart = field_node->declarators.back()->location.value().lineEnd;
                // Column should be after the last token of the last declarator
                error_loc.columnStart = field_node->declarators.back()->location.value().columnEnd + 1;
            }
            else if (field_node->type && field_node->type.has_value() && field_node->type.value()->location.has_value())
            {
                error_loc.lineStart = field_node->type.value()->location.value().lineEnd;
                error_loc.columnStart = field_node->type.value()->location.value().columnEnd + 1;
            }
            record_error("Expected ';' after field declaration.", error_loc);
        }

        finalize_node_location(field_node); // Finalize the overall FieldDeclarationNode location
        return field_node;
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
        case TokenType::NullLiteral: // 'null' keyword is treated as a literal here
        {
            auto literal_node = make_ast_node<LiteralExpressionNode>(primary_start_loc);
            literal_node->token = create_token_node(currentTokenInfo.type, currentTokenInfo); // Store the original token

            if (currentTokenInfo.type == TokenType::BooleanLiteral)
            {
                literal_node->kind = LiteralKind::Boolean;
                literal_node->valueText = std::string(currentTokenInfo.lexeme); // "true" or "false"
                // literal_node->parsed_value = std::get<bool>(currentTokenInfo.literalValue); // If you add parsed_value
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
                literal_node->valueText = std::get<std::string>(currentTokenInfo.literalValue); // Use unescaped
            }
            else if (currentTokenInfo.type == TokenType::CharLiteral)
            {
                literal_node->kind = LiteralKind::Char;
                literal_node->valueText = std::string(1, std::get<char>(currentTokenInfo.literalValue)); // Use unescaped
            }
            else if (currentTokenInfo.type == TokenType::NullLiteral)
            {
                literal_node->kind = LiteralKind::Null;
                literal_node->valueText = "null";
            }

            advance_and_lex(); // Consume the literal token
            finalize_node_location(literal_node);
            return literal_node;
        }
        case TokenType::Identifier:
        {
            auto ident_node_for_expr = make_ast_node<IdentifierExpressionNode>(primary_start_loc);
            ident_node_for_expr->identifier = create_identifier_node(currentTokenInfo);
            advance_and_lex(); // Consume the identifier token
            finalize_node_location(ident_node_for_expr);
            return ident_node_for_expr;
        }
        case TokenType::This:
        {
            auto this_node = make_ast_node<ThisExpressionNode>(primary_start_loc);
            this_node->thisKeyword = create_token_node(TokenType::This, currentTokenInfo);
            advance_and_lex(); // Consume 'this'
            finalize_node_location(this_node);
            return this_node;
        }
        case TokenType::OpenParen:
        {
            auto paren_expr_node = make_ast_node<ParenthesizedExpressionNode>(primary_start_loc);
            paren_expr_node->openParenToken = create_token_node(TokenType::OpenParen, currentTokenInfo);
            advance_and_lex(); // Consume '('

            paren_expr_node->expression = parse_expression(); // Recursive call for the inner expression

            paren_expr_node->closeParenToken = create_token_node(TokenType::CloseParen, currentTokenInfo);
            consume_token(TokenType::CloseParen, "Expected ')' after expression in parentheses.");
            // Note: consume_token advances, so previousTokenInfo is now ')'
            finalize_node_location(paren_expr_node);
            return paren_expr_node;
        }
        case TokenType::New:
        {
            return parse_object_creation_expression(); // This function will handle consuming 'new'
        }
        // Handle primitive type keywords as potential static access targets
        case TokenType::Bool:
        case TokenType::Int:
        case TokenType::String:
        case TokenType::Long:
        case TokenType::Double:
        case TokenType::Char:
            // case TokenType::Void: // Void usually isn't a static access target, but included for completeness if needed
            {
                // This is a primitive type keyword. It could be the start of a static member access
                // (e.g., int.Parse) or potentially a cast in some grammars (though casts are often unary).
                // For now, we'll treat it as a TypeNameNode that can be a target for postfix operations.
                CurrentTokenInfo type_keyword_token_data = currentTokenInfo;
                advance_and_lex(); // Consume the type keyword token

                auto type_name_node = make_ast_node<TypeNameNode>(type_keyword_token_data.location);

                // Create an IdentifierNode for the type keyword itself to populate TypeNameNode.name_segment
                auto ident_for_type_name = make_ast_node<IdentifierNode>(type_keyword_token_data.location);
                ident_for_type_name->name = std::string(type_keyword_token_data.lexeme);
                finalize_node_location(ident_for_type_name); // Finalize with its own token
                type_name_node->name_segment = ident_for_type_name;
                // type_name_node->is_primitive_type_keyword = true; // Optional: add a flag if useful

                finalize_node_location(type_name_node); // Finalize with its own token

                // IMPORTANT: A TypeNameNode is not an ExpressionNode directly.
                // The caller (e.g., parse_expression -> parse_unary_expression -> parse_postfix_expression_rhs)
                // needs to handle this. parse_postfix_expression_rhs will take this TypeNameNode
                // and if followed by '.', build a MemberAccess.
                // For now, we need a way to return this TypeNameNode such that it can be the start of a postfix chain.
                // One way is to wrap it in a specific ExpressionNode, e.g., StaticTypeTargetExpressionNode.
                // For simplicity in this step, if your expression hierarchy expects ExpressionNode,
                // you might need such a wrapper or adjust parse_postfix_expression_rhs.
                // Let's assume for now that parse_postfix_expression_rhs can handle a TypeNameNode if returned by a specific path.
                // This implies parse_expression() might return a variant or have specific logic.
                // A simpler interim step: create a dummy "TypeAsExpression" node or an IdentifierExpressionNode
                // using the type keyword's name. Semantic analysis would later clarify its role.
                // For now, let's create an IdentifierExpressionNode for simplicity, assuming postfix will handle it.
                // This is a slight simplification and might need refinement.

                auto type_as_expr = make_ast_node<IdentifierExpressionNode>(type_keyword_token_data.location);
                type_as_expr->identifier = ident_for_type_name; // Reuse the identifier node
                finalize_node_location(type_as_expr);
                return type_as_expr; // The postfix parser will see an IdentifierExpressionNode
                                     // and then a '.' if it's like "int.Parse".
            }

        default:
            record_error_at_current("Unexpected token '" + std::string(currentTokenInfo.lexeme) + "' when expecting a primary expression.");
            // Create a dummy error expression or advance and try again (latter is complex error recovery)
            // For now, return a nullptr or a specific error node if you have one.
            // Let's return nullptr and let the caller handle it.
            // To prevent infinite loops if the caller doesn't advance, we should consume the error token here.
            auto error_expr_node = make_ast_node<LiteralExpressionNode>(primary_start_loc); // Use Literal as a placeholder error node
            error_expr_node->kind = LiteralKind::Null;                                      // Indicate error somewhat
            error_expr_node->valueText = "_ERROR_EXPR_";
            error_expr_node->token = create_token_node(TokenType::Error, currentTokenInfo);
            advance_and_lex(); // Consume the problematic token
            finalize_node_location(error_expr_node);
            return error_expr_node; // Return a placeholder error node
        }
    }

    std::shared_ptr<ObjectCreationExpressionNode> ScriptParser::parse_object_creation_expression()
    {
        SourceLocation start_loc = currentTokenInfo.location; // Location of 'new'

        auto new_keyword_token_node = create_token_node(TokenType::New, currentTokenInfo);
        consume_token(TokenType::New, "Expected 'new' keyword for object creation."); // consume_token also advances

        auto node = make_ast_node<ObjectCreationExpressionNode>(start_loc);
        node->newKeyword = new_keyword_token_node;
        node->type = parse_type_name(); // Parse the type to be instantiated

        // Arguments are optional in some languages, but C# typically requires them for constructors.
        // We'll parse them if '(' is present.
        if (check_token(TokenType::OpenParen))
        {
            node->argumentList = parse_argument_list(); // parse_argument_list consumes '(' and ')'
        }
        else
        {
            // If your language *requires* parentheses for new Type(), this is an error.
            // record_error_at_current("Expected '(' for constructor arguments after type name in new expression.");
            // For now, we'll allow it to be nullopt, implying a parameterless constructor
            // or an object initializer syntax if that's added later.
            node->argumentList = std::nullopt;
        }

        // Object initializers like `new MyType { Prop = val }` would be parsed here. Not implemented in this step.

        finalize_node_location(node);
        return node;
    }

    std::shared_ptr<ExpressionNode> ScriptParser::parse_expression()
    {
        // Entry point for expression parsing, starts with the lowest precedence (assignment)
        return parse_assignment_expression();
    }

    std::shared_ptr<ExpressionNode> ScriptParser::parse_multiplicative_expression()
    {
        SourceLocation expression_start_loc = currentTokenInfo.location; // Potential start of a binary expression
        std::shared_ptr<ExpressionNode> left_operand = parse_unary_expression();

        while (check_token(TokenType::Asterisk) || check_token(TokenType::Slash) || check_token(TokenType::Percent))
        {
            // If we found an operator, the expression_start_loc should be the start of the left_operand
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
            // No else needed due to while condition

            advance_and_lex(); // Consume the operator

            binary_expr_node->right = parse_unary_expression(); // Right operand (higher precedence)

            finalize_node_location(binary_expr_node);
            left_operand = binary_expr_node; // For left-associativity
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

            advance_and_lex(); // Consume the operator

            binary_expr_node->right = parse_multiplicative_expression(); // Right operand (higher precedence)

            finalize_node_location(binary_expr_node);
            left_operand = binary_expr_node; // For left-associativity
        }

        return left_operand;
    }

    std::shared_ptr<ExpressionNode> ScriptParser::parse_relational_expression()
    {
        SourceLocation expression_start_loc = currentTokenInfo.location;
        std::shared_ptr<ExpressionNode> left_operand = parse_additive_expression();

        // For now, we only handle <, >, <=, >=. 'is' and 'as' will be added later.
        while (check_token(TokenType::LessThan) || check_token(TokenType::GreaterThan) ||
               check_token(TokenType::LessThanOrEqual) || check_token(TokenType::GreaterThanOrEqual))
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

            advance_and_lex(); // Consume the operator

            binary_expr_node->right = parse_additive_expression(); // Right operand (higher precedence)

            finalize_node_location(binary_expr_node);
            left_operand = binary_expr_node; // For left-associativity (though relational ops are usually not chained this way often)
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

            advance_and_lex(); // Consume the operator

            binary_expr_node->right = parse_relational_expression(); // Right operand (higher precedence)

            finalize_node_location(binary_expr_node);
            left_operand = binary_expr_node; // For left-associativity
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

            advance_and_lex(); // Consume the '&&' operator

            binary_expr_node->right = parse_equality_expression(); // Right operand (higher precedence)

            finalize_node_location(binary_expr_node);
            left_operand = binary_expr_node; // For left-associativity
        }

        return left_operand;
    }

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

            advance_and_lex(); // Consume the '||' operator

            binary_expr_node->right = parse_logical_and_expression(); // Right operand (higher precedence)

            finalize_node_location(binary_expr_node);
            left_operand = binary_expr_node; // For left-associativity
        }

        return left_operand;
    }

    std::shared_ptr<ExpressionNode> ScriptParser::parse_conditional_expression()
    {
        // For now, this is a placeholder. Ternary operator (?:) will be implemented later.
        // It would involve parsing a condition, then '?', then a true_expression, then ':', then a false_expression.
        // Conditional operator is right-associative.
        // Example structure:
        // auto condition = parse_logical_or_expression();
        // if (match_token(TokenType::QuestionMark)) { // Assuming QuestionMark token
        //     auto true_expr = parse_expression(); // Or parse_assignment_expression() to avoid issues
        //     consume_token(TokenType::Colon, "Expected ':' in ternary conditional expression."); // Assuming Colon token
        //     auto false_expr = parse_conditional_expression(); // For right-associativity
        //     // ... create ConditionalExpressionNode ...
        //     return conditional_node;
        // }
        // return condition;

        return parse_logical_or_expression();
    }

    std::shared_ptr<ExpressionNode> ScriptParser::parse_assignment_expression()
    {
        SourceLocation expression_start_loc = currentTokenInfo.location;
        // The left-hand side of an assignment can be a conditional expression or higher.
        // However, the result of parse_conditional_expression() itself isn't necessarily an L-value.
        // L-value checking is a semantic analysis task. The parser just captures the structure.
        std::shared_ptr<ExpressionNode> left_target = parse_conditional_expression();

        if (check_token(TokenType::Assign) || check_token(TokenType::PlusAssign) ||
            check_token(TokenType::MinusAssign) || check_token(TokenType::AsteriskAssign) ||
            check_token(TokenType::SlashAssign) || check_token(TokenType::PercentAssign))
        {
            // If we found an operator, the expression_start_loc should be the start of the left_target
            if (left_target && left_target->location.has_value())
            {
                expression_start_loc = left_target->location.value();
            }

            auto assignment_node = make_ast_node<AssignmentExpressionNode>(expression_start_loc);
            assignment_node->target = left_target;
            assignment_node->operatorToken = create_token_node(currentTokenInfo.type, currentTokenInfo);

            TokenType op_type = currentTokenInfo.type;
            advance_and_lex(); // Consume the assignment operator

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
                // Should not happen due to check_token
                record_error_at_previous("Internal parser error: Unexpected assignment operator.");
                assignment_node->opKind = AssignmentOperatorKind::Assign; // Fallback
                break;
            }

            // Assignment is right-associative, so the right-hand side is parsed by calling parse_assignment_expression itself.
            assignment_node->source = parse_assignment_expression();

            finalize_node_location(assignment_node);
            return assignment_node;
        }

        return left_target; // Not an assignment, just return the higher-precedence expression
    }

    std::shared_ptr<ExpressionNode> ScriptParser::parse_unary_expression()
    {
        SourceLocation unary_start_loc = currentTokenInfo.location;

        // Attempt to parse a C-style cast: (TypeName) unary_expression
        // This lookahead is tricky. A robust way is to try parsing TypeName and see if it's followed by CloseParen.
        // Simplified check: OpenParen then (Identifier or primitive_type_keyword) then ... CloseParen
        if (check_token(TokenType::OpenParen))
        {
            // Tentative parsing: We need a way to "peek" further or backtrack if it's not a cast.
            // For a full implementation, we might need a state save/restore mechanism or more predictive parsing.
            // Let's try a moderately safe approach:
            // 1. Peek at token after '('. If it's an identifier or known type keyword.
            // 2. Try to parse a TypeName.
            // 3. Check if it's followed by a ')'.

            // Save current state for potential backtrack (simplified version)
            size_t original_char_offset = currentCharOffset;
            int original_line = currentLine;
            int original_column = currentColumn;
            size_t original_line_start_offset = currentLineStartOffset;
            CurrentTokenInfo original_current_token_info = currentTokenInfo;
            CurrentTokenInfo original_previous_token_info = previousTokenInfo;
            std::vector<ParseError> original_errors = errors; // More complex state to manage

            bool might_be_cast = false;
            SourceLocation open_paren_loc = currentTokenInfo.location;
            advance_and_lex(); // Consume '('

            // Check if the next token could start a type name
            if (check_token(TokenType::Identifier) ||
                check_token({TokenType::Bool, TokenType::Int, TokenType::String, TokenType::Long, TokenType::Double, TokenType::Char, TokenType::Void, TokenType::Float})) // Added Float
            {
                // Temporarily parse TypeName without consuming tokens if it fails in a way that means it's not a type.
                // This is where a more robust "try_parse_type_name" would be good.
                // For now, parse_type_name records errors and advances.
                // If parse_type_name fails significantly, it's probably not a type.
                std::shared_ptr<TypeNameNode> potential_type_name = parse_type_name(); // This advances

                if (potential_type_name && check_token(TokenType::CloseParen))
                {
                    // It looks like a cast! (TypeName)
                    might_be_cast = true;
                    auto cast_node = make_ast_node<CastExpressionNode>(open_paren_loc);                               // Start loc is the '('
                    cast_node->openParenToken = create_token_node(TokenType::OpenParen, original_current_token_info); // Use original token info for '('
                    cast_node->targetType = potential_type_name;
                    cast_node->closeParenToken = create_token_node(TokenType::CloseParen, currentTokenInfo);

                    advance_and_lex(); // Consume ')'

                    cast_node->expression = parse_unary_expression(); // The expression to cast (handles another cast or other unary)
                    finalize_node_location(cast_node);                // Final location up to the end of the casted expression

                    // Successfully parsed a cast, clear any speculative errors from this attempt
                    // errors = original_errors; // Careful if parse_unary_expression() added legitimate errors
                    // For now, let's assume errors added by parse_type_name IF it was part of a successful cast are fine.
                    return cast_node;
                }
            }

            // If it wasn't a cast, restore state and parse as parenthesized expression or other unary.
            // This backtrack is crucial.
            currentCharOffset = original_char_offset;
            currentLine = original_line;
            currentColumn = original_column;
            currentLineStartOffset = original_line_start_offset;
            currentTokenInfo = original_current_token_info;
            previousTokenInfo = original_previous_token_info;
            errors = original_errors; // Restore errors too
            // Note: `advance_and_lex()` was called in the try block, it needs to be "undone" if we fall through.
            // The above state restoration effectively does this. currentTokenInfo is back to '('.
        }

        // Existing unary operator parsing
        if (check_token(TokenType::LogicalNot) ||
            check_token(TokenType::Plus) ||      // Unary Plus
            check_token(TokenType::Minus) ||     // Unary Minus
            check_token(TokenType::Increment) || // Prefix ++
            check_token(TokenType::Decrement))   // Prefix --
        {
            auto unary_node = make_ast_node<UnaryExpressionNode>(unary_start_loc);
            unary_node->operatorToken = create_token_node(currentTokenInfo.type, currentTokenInfo);
            unary_node->isPostfix = false;

            TokenType op_token_type = currentTokenInfo.type;
            advance_and_lex(); // Consume the unary operator

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

            unary_node->operand = parse_unary_expression(); // Right-associative: --++x
            finalize_node_location(unary_node);
            return unary_node;
        }

        return parse_postfix_expression();
    }

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
            // Check for method calls (generic or non-generic)
            else if (check_token(TokenType::LessThan) || check_token(TokenType::OpenParen))
            {
                bool is_potential_generic_disambiguation_needed = check_token(TokenType::LessThan) &&
                                                                  (std::dynamic_pointer_cast<IdentifierExpressionNode>(left_expr) ||
                                                                   std::dynamic_pointer_cast<MemberAccessExpressionNode>(left_expr) ||
                                                                   std::dynamic_pointer_cast<IndexerExpressionNode>(left_expr));

                bool actually_parse_as_generic_call = false;
                if (is_potential_generic_disambiguation_needed)
                {
                    if (can_parse_as_generic_arguments_followed_by_call())
                    {
                        actually_parse_as_generic_call = true;
                    }
                    else
                    {
                        // It looked like it could be generic, but the predicate said no (e.g. MyVar < OtherVar)
                        // Break the postfix loop, the '<' will be handled by relational operator parsing.
                        break;
                    }
                }

                // Proceed if it's a confirmed generic call or a non-generic call (starts with '(')
                if (actually_parse_as_generic_call || check_token(TokenType::OpenParen))
                {
                    auto call_node = make_ast_node<MethodCallExpressionNode>(overall_start_loc);
                    call_node->target = left_expr;

                    if (actually_parse_as_generic_call) // We've already confirmed this via predicate
                    {
                        call_node->genericOpenAngleBracketToken = create_token_node(TokenType::LessThan, currentTokenInfo);
                        advance_and_lex(); // Consume '<'

                        std::vector<std::shared_ptr<TypeNameNode>> type_args_vec;
                        std::vector<std::shared_ptr<TokenNode>> type_arg_commas_vec;

                        if (!check_token(TokenType::GreaterThan))
                        {
                            do
                            {
                                type_args_vec.push_back(parse_type_name()); // Full parse now
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

                    // After optional generics, we MUST have an argument list for a method call
                    if (check_token(TokenType::OpenParen))
                    {
                        auto arg_list_opt = parse_argument_list(); // parse_argument_list consumes ( and )
                        if (arg_list_opt)
                        {
                            call_node->argumentList = arg_list_opt.value();
                        }
                        else
                        {
                            // This can happen if parse_argument_list returns nullopt because it didn't find '('
                            // (which shouldn't happen if check_token(OpenParen) was true before call)
                            // or if it had an internal failure.
                            record_error_at_current("Failed to parse argument list for method call starting with '('.");
                            auto dummy_arg_list = make_ast_node<ArgumentListNode>(currentTokenInfo.location);
                            // Create dummy open paren if it was supposed to be there but parsing failed
                            dummy_arg_list->openParenToken = create_token_node(TokenType::OpenParen, currentTokenInfo); // Current might not be '('
                            // Create a dummy close paren as well
                            SourceLocation dummy_close_loc = currentTokenInfo.location; // Best guess for location
                            dummy_close_loc.columnStart++;
                            dummy_close_loc.columnEnd++;
                            dummy_arg_list->closeParenToken = create_token_node(TokenType::CloseParen, {TokenType::CloseParen, ")", dummy_close_loc});

                            finalize_node_location(dummy_arg_list);
                            call_node->argumentList = dummy_arg_list;
                        }
                    }
                    else
                    {
                        // This case means it was `expr<T>` but no `(args)` followed.
                        // This is a syntax error for a method call.
                        if (call_node->genericOpenAngleBracketToken)
                        {
                            record_error_at_current("Expected '(' for arguments after generic type arguments in method call.");
                            auto dummy_arg_list = make_ast_node<ArgumentListNode>(previousTokenInfo.location); // location after '>'
                            finalize_node_location(dummy_arg_list);                                            // Finalize based on previous token ('>')
                            call_node->argumentList = dummy_arg_list;                                          // Assign an empty (but present) argument list
                        }
                        else
                        {
                            // This path should not be reachable if the outer 'if' conditions are correct.
                            // If it's not a generic call attempt and not an OpenParen, we shouldn't be here.
                            // For safety, break.
                            break;
                        }
                    }
                    finalize_node_location(call_node);
                    left_expr = call_node;
                    overall_start_loc = left_expr->location.value_or(overall_start_loc);
                }
                else
                {
                    // This 'else' corresponds to `is_potential_generic_disambiguation_needed` being false
                    // AND `check_token(TokenType::OpenParen)` being false.
                    // Or, `is_potential_generic_disambiguation_needed` was true, but `can_parse_as_...` was false,
                    // and then the outer `if` condition `(actually_parse_as_generic_call || check_token(TokenType::OpenParen))`
                    // also evaluated to false (which means `check_token(TokenType::OpenParen)` was false).
                    // Essentially, if it's not a confirmed generic call and not an open paren, break.
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

    std::shared_ptr<ExpressionStatementNode> ScriptParser::parse_expression_statement()
    {
        SourceLocation statement_start_loc = currentTokenInfo.location; // Location of the start of the expression

        auto expr_stmt_node = make_ast_node<ExpressionStatementNode>(statement_start_loc);

        std::shared_ptr<ExpressionNode> expression = parse_expression();
        if (expression)
        {
            expr_stmt_node->expression = expression;
        }
        else
        {
            // parse_expression should record an error if it fails and cannot produce a node.
            // If it returns nullptr, it's a critical failure for this statement.
            // We might already have an error recorded by parse_expression.
            // If not, record one here.
            if (errors.empty() || !(errors.back().location.lineStart == statement_start_loc.lineStart && errors.back().location.columnStart == statement_start_loc.columnStart))
            {
                record_error_at_current("Invalid expression for expression statement.");
            }
            // The statement node will be created but might lack a valid expression.
            // This allows parsing to potentially continue to find the semicolon.
        }

        if (check_token(TokenType::Semicolon))
        {
            expr_stmt_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex(); // Consume ';'
        }
        else
        {
            // Use m_previous_token_info for error location if expression was valid,
            // otherwise use current token if expression parsing failed at the start.
            SourceLocation error_loc = expr_stmt_node->expression ? previousTokenInfo.location : currentTokenInfo.location;
            if (expr_stmt_node->expression && expr_stmt_node->expression->location.has_value())
            {
                error_loc.lineStart = expr_stmt_node->expression->location.value().lineEnd;
                error_loc.columnStart = expr_stmt_node->expression->location.value().columnEnd + 1; // After the expression
            }

            record_error("Expected ';' after expression statement.", error_loc);
            // The statement node is still created, but it's malformed.
        }

        finalize_node_location(expr_stmt_node);
        return expr_stmt_node;
    }

    std::shared_ptr<TypeNameNode> ScriptParser::parse_type_name()
    {
        SourceLocation type_name_start_loc = currentTokenInfo.location;
        auto node = make_ast_node<TypeNameNode>(type_name_start_loc);

        if (check_token(TokenType::Identifier) ||
            check_token({TokenType::Bool, TokenType::Int, TokenType::String, TokenType::Long, TokenType::Double, TokenType::Char, TokenType::Void, TokenType::Float})) // Added Float
        {

            auto ident_for_segment = create_identifier_node(currentTokenInfo);
            advance_and_lex();
            node->name_segment = ident_for_segment;

            // Handle Qualified Name
            // This loop builds a right-associative chain of QualifiedNameNodes wrapped in TypeNameNodes if needed.
            // Or, more simply, the TypeNameNode's name_segment becomes the QualifiedNameNode.
            std::shared_ptr<AstNode> current_segment_owner = node; // The node whose name_segment we are building

            while (check_token(TokenType::Dot))
            {
                auto qualified_node = make_ast_node<QualifiedNameNode>(type_name_start_loc); // Start loc of whole type

                // The current 'node' (which holds the left part either as Ident or previous QN) becomes the left of new QN.
                // This might require 'QualifiedNameNode::left' to be std::shared_ptr<AstNode> or a variant if it can be Ident or QN.
                // Your AST: QualifiedNameNode::left is TypeNameNode.
                // So, we need to ensure the 'left' is a TypeNameNode.
                // The 'node' itself is already a TypeNameNode.
                qualified_node->left = node;

                qualified_node->dotToken = create_token_node(TokenType::Dot, currentTokenInfo);
                advance_and_lex(); // Consume '.'

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

                // Update the main 'node' to wrap this new QualifiedNameNode
                auto new_top_node = make_ast_node<TypeNameNode>(type_name_start_loc);
                new_top_node->name_segment = qualified_node;
                // Transfer other properties if necessary (like array specifiers if they could appear mid-qualification, though usually not)
                node = new_top_node; // The 'node' we are building is now this new one
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

        // Optional Generic Type Arguments for the type itself (e.g., List<int>)
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

        // Array Specifiers (simple T[] version)
        if (check_token(TokenType::OpenBracket))
        { // Only one rank for now based on your AST
            node->openSquareBracketToken = create_token_node(TokenType::OpenBracket, currentTokenInfo);
            advance_and_lex(); // Consume '['

            node->closeSquareBracketToken = create_token_node(TokenType::CloseBracket, currentTokenInfo);
            consume_token(TokenType::CloseBracket, "Expected ']' for array type specifier.");
        }

        finalize_node_location(node);
        return node;
    }

    std::optional<std::shared_ptr<ArgumentListNode>> ScriptParser::parse_argument_list()
    {
        SourceLocation start_loc = currentTokenInfo.location; // Location of '('

        if (!check_token(TokenType::OpenParen))
        {
            // This function expects to be called when an OpenParen is current or expected.
            // If it's not an OpenParen, it's not an argument list in the typical sense.
            // For example, object creation might allow `new Type;` without parens in some languages,
            // but method calls usually require them.
            // Let's assume for now if OpenParen is not here, it's not a valid call to this function
            // for parsing a standard argument list.
            return std::nullopt; // Or record an error if '(' was mandatory.
        }

        auto arg_list_node = make_ast_node<ArgumentListNode>(start_loc);
        arg_list_node->openParenToken = create_token_node(TokenType::OpenParen, currentTokenInfo);
        advance_and_lex(); // Consume '('

        bool first_argument = true;
        if (!check_token(TokenType::CloseParen))
        { // Check if the list is not empty: `()`
            do
            {
                if (!first_argument)
                {
                    // Expect a comma before subsequent arguments
                    if (check_token(TokenType::Comma))
                    {
                        arg_list_node->commas.push_back(create_token_node(TokenType::Comma, currentTokenInfo));
                        advance_and_lex(); // Consume ','
                    }
                    else
                    {
                        record_error_at_current("Expected ',' or ')' in argument list.");
                        // Attempt to recover by breaking or trying to parse an argument anyway,
                        // but for now, we'll break. The CloseParen check below will fail.
                        break;
                    }
                }
                first_argument = false;

                SourceLocation argument_start_loc = currentTokenInfo.location;
                auto argument_node = make_ast_node<ArgumentNode>(argument_start_loc);

                // TODO: Add support for named arguments (e.g., "name: value")
                // For now, all arguments are positional.
                // if (check_token(TokenType::Identifier) && peek_token(1).type == TokenType::Colon) {
                //     argument_node->nameLabel = create_identifier_node(currentTokenInfo);
                //     advance_and_lex(); // consume identifier
                //     argument_node->colonToken = create_token_node(TokenType::Colon, currentTokenInfo);
                //     advance_and_lex(); // consume colon
                // }

                std::shared_ptr<ExpressionNode> expr = parse_expression();
                if (expr)
                {
                    argument_node->expression = expr;
                    finalize_node_location(argument_node); // Finalize location of ArgumentNode
                    arg_list_node->arguments.push_back(argument_node);
                }
                else
                {
                    // parse_expression should ideally return an error node or have recorded an error.
                    // If it returns nullptr, we have a problem.
                    // For now, assume parse_expression records errors and we might get a dummy/error expression back.
                    // If we received a valid (even if error) expression, it's added. If nullptr, we might skip.
                    // Let's assume parse_expression always returns something or errors appropriately.
                    // If an error occurred during expression parsing, and we need to stop, break.
                    if (errors.back().location.lineStart == argument_start_loc.lineStart &&
                        errors.back().location.columnStart == argument_start_loc.columnStart)
                    {
                        // Error was at the start of this argument's expression, probably fatal for this arg.
                        break;
                    }
                }
            } while (!check_token(TokenType::CloseParen) && !is_at_end_of_token_stream());
        }

        if (check_token(TokenType::CloseParen))
        {
            arg_list_node->closeParenToken = create_token_node(TokenType::CloseParen, currentTokenInfo);
            advance_and_lex(); // Consume ')'
        }
        else
        {
            record_error_at_current("Expected ')' to close argument list.");
            // The arg_list_node is still created, but it's malformed.
            // finalize_node_location below will use the location of the token *before* the error.
        }

        finalize_node_location(arg_list_node);
        return arg_list_node;
    }

    std::shared_ptr<BlockStatementNode> ScriptParser::parse_block_statement()
    {
        SourceLocation block_start_loc = currentTokenInfo.location; // Location of '{'

        auto block_node = make_ast_node<BlockStatementNode>(block_start_loc);

        if (check_token(TokenType::OpenBrace))
        {
            block_node->openBraceToken = create_token_node(TokenType::OpenBrace, currentTokenInfo);
            advance_and_lex(); // Consume '{'
        }
        else
        {
            record_error_at_current("Expected '{' to start a block statement.");
            // Attempt to create a node anyway, but it will be malformed.
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
                // parse_statement returned nullptr, implying an error it couldn't recover from,
                // or it consumed a token and wants us to try again.
                // If it's EOF, the loop condition will handle it.
                // If an error was recorded by parse_statement and it advanced, we continue.
                // If it didn't advance, we must advance here to prevent an infinite loop.
                if (!is_at_end_of_token_stream() && currentTokenInfo.location.columnStart == previousTokenInfo.location.columnStart && currentTokenInfo.location.lineStart == previousTokenInfo.location.lineStart)
                {
                    // Safety break if parse_statement failed but didn't advance. This shouldn't happen if parse_statement handles errors by advancing.
                    record_error_at_current("Parser stuck in block statement. Advancing token.");
                    advance_and_lex();
                }
                // If many errors, maybe stop trying to parse this block.
                if (errors.size() > 10 && errors.back().location.fileName == block_start_loc.fileName)
                { // rough guard
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
            advance_and_lex(); // Consume '}'
        }
        else
        {
            // Error location should be where '}' was expected, which is after the last successfully parsed statement
            // or at the current token if block was empty or parsing failed early.
            SourceLocation error_loc = previousTokenInfo.location;
            if (!block_node->statements.empty() && block_node->statements.back()->location.has_value())
            {
                error_loc = block_node->statements.back()->location.value(); // End of last statement
                error_loc.columnStart = error_loc.columnEnd + 1;             // Position after it
            }
            else if (block_node->openBraceToken && block_node->openBraceToken->location.has_value())
            {
                error_loc = block_node->openBraceToken->location.value();
                error_loc.columnStart = error_loc.columnEnd + 1;
            }
            else
            {
                error_loc = currentTokenInfo.location; // Fallback
            }
            record_error("Expected '}' to close block statement. Found " + token_type_to_string(currentTokenInfo.type) + " instead.", error_loc);
        }

        finalize_node_location(block_node);
        return block_node;
    }

    std::shared_ptr<LocalVariableDeclarationStatementNode> ScriptParser::parse_local_variable_declaration_statement()
    {
        SourceLocation statement_start_loc = currentTokenInfo.location; // Location of 'var' or type

        auto var_decl_node = make_ast_node<LocalVariableDeclarationStatementNode>(statement_start_loc);

        if (check_token(TokenType::Var))
        {
            var_decl_node->varKeywordToken = create_token_node(TokenType::Var, currentTokenInfo);
            advance_and_lex(); // Consume 'var'

            // For 'var', the TypeNameNode in the AST might represent 'var' itself or be simplified.
            // Let's create a TypeNameNode that explicitly holds 'var' as its identifier.
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
            // Parse a full type name
            std::shared_ptr<TypeNameNode> type_node = parse_type_name();
            if (type_node)
            {
                var_decl_node->type = type_node;
            }
            else
            {
                record_error_at_current("Expected type name or 'var' for local variable declaration.");
                // Create a dummy type node to allow parsing to continue for declarators
                auto dummy_type_name = make_ast_node<TypeNameNode>(currentTokenInfo.location);
                auto dummy_ident = make_ast_node<IdentifierNode>(currentTokenInfo.location);
                dummy_ident->name = "_ERROR_TYPE_";
                finalize_node_location(dummy_ident);
                dummy_type_name->name_segment = dummy_ident;
                finalize_node_location(dummy_type_name);
                var_decl_node->type = dummy_type_name;
                // No advance here, let declarator parsing try from current token.
            }
        }

        // Parse one or more declarators
        bool first_declarator = true;
        do
        {
            if (!first_declarator)
            {
                if (check_token(TokenType::Comma))
                {
                    var_decl_node->declaratorCommas.push_back(create_token_node(TokenType::Comma, currentTokenInfo));
                    advance_and_lex(); // Consume ','
                }
                else
                {
                    record_error_at_current("Expected ',' or ';' in variable declaration.");
                    break; // Stop parsing declarators
                }
            }
            first_declarator = false;

            SourceLocation declarator_start_loc = currentTokenInfo.location;
            auto declarator_node = make_ast_node<VariableDeclaratorNode>(declarator_start_loc);

            if (check_token(TokenType::Identifier))
            {
                declarator_node->name = create_identifier_node(currentTokenInfo);
                advance_and_lex(); // Consume identifier
            }
            else
            {
                record_error_at_current("Expected identifier for variable name.");
                auto dummy_ident = make_ast_node<IdentifierNode>(currentTokenInfo.location);
                dummy_ident->name = "_ERROR_VAR_NAME_";
                finalize_node_location(dummy_ident);
                declarator_node->name = dummy_ident;
                // Attempt to recover by breaking declarator loop or hoping for '=' or ';'
                if (!check_token(TokenType::Assign) && !check_token(TokenType::Comma) && !check_token(TokenType::Semicolon))
                {
                    break;
                }
            }

            if (check_token(TokenType::Assign))
            {
                declarator_node->equalsToken = create_token_node(TokenType::Assign, currentTokenInfo);
                advance_and_lex(); // Consume '='

                std::shared_ptr<ExpressionNode> initializer_expr = parse_expression();
                if (initializer_expr)
                {
                    declarator_node->initializer = initializer_expr;
                }
                else
                {
                    record_error_at_current("Invalid initializer expression for variable.");
                    // Declarator node will have no initializer or a dummy one.
                }
            }
            finalize_node_location(declarator_node);
            var_decl_node->declarators.push_back(declarator_node);

        } while (check_token(TokenType::Comma));

        if (check_token(TokenType::Semicolon))
        {
            var_decl_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex(); // Consume ';'
        }
        else
        {
            SourceLocation error_loc = previousTokenInfo.location; // Error after the last valid token
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
        SourceLocation statement_start_loc = currentTokenInfo.location; // Location of 'return'

        auto return_node = make_ast_node<ReturnStatementNode>(statement_start_loc);

        if (check_token(TokenType::Return))
        {
            return_node->returnKeyword = create_token_node(TokenType::Return, currentTokenInfo);
            advance_and_lex(); // Consume 'return'
        }
        else
        {
            // This function should only be called if 'return' is the current token.
            // This is an internal parser error or incorrect dispatch.
            record_error_at_current("Internal Parser Error: parse_return_statement called without 'return' token.");
            // Create a malformed node.
        }

        // Check if there's an expression to return (i.e., not followed immediately by a semicolon)
        if (!check_token(TokenType::Semicolon) && !is_at_end_of_token_stream())
        {
            std::shared_ptr<ExpressionNode> expression = parse_expression();
            if (expression)
            {
                return_node->expression = expression;
            }
            else
            {
                // parse_expression should record an error.
                // The return node will be created without a valid expression.
                record_error_at_current("Invalid expression for return statement.");
            }
        }
        // If it is a semicolon, expression remains std::nullopt (void return)

        if (check_token(TokenType::Semicolon))
        {
            return_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex(); // Consume ';'
        }
        else
        {
            SourceLocation error_loc = previousTokenInfo.location; // Error after the last valid token
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

    std::shared_ptr<StatementNode> ScriptParser::parse_statement()
    {
        if (is_at_end_of_token_stream())
        {
            return nullptr; // Nothing to parse
        }

        // Explicit statement keywords first
        if (check_token(TokenType::OpenBrace))
        {
            return parse_block_statement();
        }
        else if (check_token(TokenType::Return))
        {
            return parse_return_statement();
        }
        else if (check_token(TokenType::If))
        {
            return parse_if_statement();
        }
        else if (check_token(TokenType::While))
        {
            return parse_while_statement();
        }
        else if (check_token(TokenType::For)) // Added dispatch for For
        {
            // Note: This will also catch 'ForEach' for now if its TokenType is the same or not yet distinguished.
            // A separate parse_foreach_statement would be needed if TokenType::ForEach exists and is distinct.
            return parse_for_statement();
        }
        // Check for local variable declarations (starting with 'var' or a primitive type)
        else if (check_token(TokenType::Var) ||
                 check_token(TokenType::Bool) ||
                 check_token(TokenType::Int) ||
                 check_token(TokenType::String) ||
                 check_token(TokenType::Long) ||
                 check_token(TokenType::Double) ||
                 check_token(TokenType::Char) ||
                 check_token(TokenType::Float) ||
                 check_token(TokenType::Identifier) // Potential custom type
                 )
        {
            // If it's an Identifier, we need to be more certain it's a declaration
            // (e.g., "TypeName varName") rather than an expression statement ("funcName();").
            if (check_token(TokenType::Identifier))
            {
                // Save parser state for lookahead
                size_t original_char_offset_peek = currentCharOffset;
                int original_line_peek = currentLine;
                int original_column_peek = currentColumn;
                size_t original_line_start_offset_peek = currentLineStartOffset;
                CurrentTokenInfo original_current_token_info_peek = currentTokenInfo;
                CurrentTokenInfo original_previous_token_info_peek = previousTokenInfo;
                // Note: errors are not saved/restored for this simple peek, as it shouldn't generate errors.

                advance_and_lex(); // Consume the first Identifier (potential TypeName)
                bool is_followed_by_identifier = check_token(TokenType::Identifier); // Is next token an Identifier (potential varName)?
                
                // Restore parser state after peeking
                currentCharOffset = original_char_offset_peek;
                currentLine = original_line_peek;
                currentColumn = original_column_peek;
                currentLineStartOffset = original_line_start_offset_peek;
                currentTokenInfo = original_current_token_info_peek;
                previousTokenInfo = original_previous_token_info_peek;

                if (is_followed_by_identifier) {
                    // Likely "TypeName VariableName ...", proceed to parse as local variable declaration.
                    return parse_local_variable_declaration_statement();
                }
                // If not "Identifier Identifier", then the first Identifier is likely the start of an expression.
                // Fall through to expression statement parsing.
            }
            else
            {
                // It's 'var' or a primitive type keyword, definitely a local var decl.
                return parse_local_variable_declaration_statement();
            }
        }
        else if (check_token(TokenType::Semicolon))
        {
            auto empty_stmt_node = make_ast_node<ExpressionStatementNode>(currentTokenInfo.location);
            empty_stmt_node->expression = nullptr;
            empty_stmt_node->semicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex(); // Consume ';'
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

    std::shared_ptr<ForStatementNode> ScriptParser::parse_for_statement()
    {
        SourceLocation statement_start_loc = currentTokenInfo.location; // Location of 'for'
        auto for_node = make_ast_node<ForStatementNode>(statement_start_loc);

        for_node->forKeyword = create_token_node(TokenType::For, currentTokenInfo);
        consume_token(TokenType::For, "Expected 'for' keyword."); // Consumes 'for'

        if (check_token(TokenType::OpenParen))
        {
            for_node->openParenToken = create_token_node(TokenType::OpenParen, currentTokenInfo);
            advance_and_lex(); // Consume '('
        }
        else
        {
            record_error_at_current("Expected '(' after 'for' keyword.");
        }

        // Initializer part
        // Can be a LocalVariableDeclarationStatement (without its own semicolon if it's the only thing)
        // or a list of expression statements (which themselves would not have semicolons here,
        // the semicolon separating sections is parsed by the for loop logic).
        if (!check_token(TokenType::Semicolon)) // If not an empty initializer section
        {
            // Try to parse as local variable declaration first.
            // This requires some lookahead or a way to distinguish.
            // A simple check: if 'var' or a primitive type keyword is next.
            if (check_token(TokenType::Var) ||
                check_token(TokenType::Bool) || check_token(TokenType::Int) ||
                check_token(TokenType::String) || check_token(TokenType::Long) ||
                check_token(TokenType::Double) || check_token(TokenType::Char) ||
                check_token(TokenType::Float) // Added Float
                // || could_be_custom_type_followed_by_identifier() // More complex lookahead
            )
            {
                // The LocalVariableDeclarationStatementNode includes its own semicolon in the AST.
                // For a 'for' loop, this semicolon acts as the first separator.
                auto local_var_decl = parse_local_variable_declaration_statement();
                for_node->initializers = local_var_decl;
                // The firstSemicolonToken for the for_node is conceptually the one from local_var_decl.
                if (local_var_decl && local_var_decl->semicolonToken)
                {
                    for_node->firstSemicolonToken = local_var_decl->semicolonToken;
                }
                else
                {
                    // This case implies parse_local_variable_declaration_statement failed to find a semicolon
                    // which it should have reported. If it's null, an error was already reported.
                    // If it's not null but has no semicolon token, that's an AST inconsistency for this use case.
                    // For safety, if the semicolon isn't there on the decl, we expect one immediately.
                    if (check_token(TokenType::Semicolon))
                    {
                        for_node->firstSemicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
                        advance_and_lex(); // Consume ';'
                    }
                    else
                    {
                        record_error_at_current("Expected ';' after 'for' loop initializer declaration.");
                    }
                }
            }
            else // Parse as list of expression statements
            {
                std::vector<std::shared_ptr<ExpressionNode>> init_expressions;
                std::vector<std::shared_ptr<TokenNode>> init_commas;
                if (!check_token(TokenType::Semicolon)) // Check again in case the var decl path wasn't taken but it's empty
                {
                    do
                    {
                        init_expressions.push_back(parse_expression());
                        if (check_token(TokenType::Comma))
                        {
                            init_commas.push_back(create_token_node(TokenType::Comma, currentTokenInfo));
                            advance_and_lex(); // Consume comma
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
                    advance_and_lex(); // Consume ';'
                }
                else
                {
                    record_error_at_current("Expected ';' after 'for' loop initializer expressions.");
                }
            }
        }
        else // Empty initializer section
        {
            for_node->firstSemicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex(); // Consume ';'
        }

        // Condition part
        if (!check_token(TokenType::Semicolon)) // If not an empty condition section
        {
            for_node->condition = parse_expression();
        }
        // else condition remains std::nullopt

        if (check_token(TokenType::Semicolon))
        {
            for_node->secondSemicolonToken = create_token_node(TokenType::Semicolon, currentTokenInfo);
            advance_and_lex(); // Consume ';'
        }
        else
        {
            record_error_at_current("Expected ';' after 'for' loop condition.");
        }

        // Incrementors part
        if (!check_token(TokenType::CloseParen)) // If not an empty incrementor section
        {
            std::vector<std::shared_ptr<ExpressionNode>> incr_expressions;
            std::vector<std::shared_ptr<TokenNode>> incr_commas;
            do
            {
                incr_expressions.push_back(parse_expression());
                if (check_token(TokenType::Comma))
                {
                    incr_commas.push_back(create_token_node(TokenType::Comma, currentTokenInfo));
                    advance_and_lex(); // Consume comma
                }
                else
                {
                    break;
                }
            } while (!check_token(TokenType::CloseParen) && !is_at_end_of_token_stream());
            for_node->incrementors = incr_expressions;
            for_node->incrementorCommas = incr_commas;
        }
        // else incrementors remain empty

        if (check_token(TokenType::CloseParen))
        {
            for_node->closeParenToken = create_token_node(TokenType::CloseParen, currentTokenInfo);
            advance_and_lex(); // Consume ')'
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

    std::shared_ptr<IfStatementNode> ScriptParser::parse_if_statement()
    {
        SourceLocation statement_start_loc = currentTokenInfo.location; // Location of 'if'
        auto if_node = make_ast_node<IfStatementNode>(statement_start_loc);

        if_node->ifKeyword = create_token_node(TokenType::If, currentTokenInfo);
        consume_token(TokenType::If, "Expected 'if' keyword."); // Consumes 'if'

        if (check_token(TokenType::OpenParen))
        {
            if_node->openParenToken = create_token_node(TokenType::OpenParen, currentTokenInfo);
            advance_and_lex(); // Consume '('
        }
        else
        {
            record_error_at_current("Expected '(' after 'if' keyword.");
            // Create a dummy token if needed by AST, or allow nullopt
        }

        if_node->condition = parse_expression();
        if (!if_node->condition)
        {
            // parse_expression should have recorded an error or returned a placeholder.
            // If it returned nullptr, it's a significant issue.
            record_error_at_current("Expected condition expression in 'if' statement.");
            // Potentially create a dummy error expression node for condition
        }

        if (check_token(TokenType::CloseParen))
        {
            if_node->closeParenToken = create_token_node(TokenType::CloseParen, currentTokenInfo);
            advance_and_lex(); // Consume ')'
        }
        else
        {
            record_error_at_current("Expected ')' after 'if' condition.");
            // Create a dummy token if needed by AST, or allow nullopt
        }

        if_node->thenStatement = parse_statement();
        if (!if_node->thenStatement)
        {
            record_error_at_current("Expected statement for 'then' branch of 'if' statement.");
            // Potentially create a dummy empty block statement
        }

        if (match_token(TokenType::Else)) // match_token consumes 'else' if present
        {
            if_node->elseKeyword = create_token_node(TokenType::Else, previousTokenInfo); // 'else' was just consumed
            if_node->elseStatement = parse_statement();
            if (!if_node->elseStatement.has_value() || !if_node->elseStatement.value())
            {
                record_error_at_current("Expected statement for 'else' branch of 'if' statement.");
                // Potentially create a dummy empty block statement for else branch
            }
        }

        finalize_node_location(if_node);
        return if_node;
    }

    std::shared_ptr<WhileStatementNode> ScriptParser::parse_while_statement()
    {
        SourceLocation statement_start_loc = currentTokenInfo.location; // Location of 'while'
        auto while_node = make_ast_node<WhileStatementNode>(statement_start_loc);

        while_node->whileKeyword = create_token_node(TokenType::While, currentTokenInfo);
        consume_token(TokenType::While, "Expected 'while' keyword."); // Consumes 'while'

        if (check_token(TokenType::OpenParen))
        {
            while_node->openParenToken = create_token_node(TokenType::OpenParen, currentTokenInfo);
            advance_and_lex(); // Consume '('
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
            advance_and_lex(); // Consume ')'
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

#pragma endregion

#pragma region "Parsing Helpers"

    bool ScriptParser::check_token(TokenType type) const
    {
        return !is_at_end_of_token_stream() && currentTokenInfo.type == type;
    }

    bool ScriptParser::check_token(const std::vector<TokenType> &types) const
    {
        if (is_at_end_of_token_stream())
        {
            return false;
        }
        TokenType current_type = currentTokenInfo.type;
        for (TokenType type : types)
        {
            if (current_type == type)
            {
                return true;
            }
        }
        return false;
    }

    bool ScriptParser::match_token(TokenType type)
    {
        if (check_token(type))
        {
            advance_and_lex(); // Consumes the current token and advances to the next
            return true;
        }
        return false;
    }

    const CurrentTokenInfo &ScriptParser::consume_token(TokenType expected_type, const std::string &error_message)
    {
        if (check_token(expected_type))
        {
            // Token is as expected, advance and return the info of the consumed token
            SourceLocation loc_before_advance = currentTokenInfo.location; // Save location before advance
            std::string_view lexeme_before_advance = currentTokenInfo.lexeme;
            // ... any other relevant fields from currentTokenInfo ...

            advance_and_lex(); // This updates currentTokenInfo and previousTokenInfo
                               // previousTokenInfo now holds the data of the token we just successfully consumed.
            return previousTokenInfo;
        }
        else
        {
            // Token is not what was expected. Record an error. Do NOT advance.
            // The parser is still positioned at the unexpected token.
            record_error_at_current(error_message + " Expected " + token_type_to_string(expected_type) +
                                    " but got " + token_type_to_string(currentTokenInfo.type) +
                                    " ('" + std::string(currentTokenInfo.lexeme) + "').");
            // In this error case, what should be returned? The current (unexpected) token seems most informative
            // for the caller to potentially use its location for an error AST node.
            // However, the contract is "return the consumed token". Since nothing was consumed,
            // this is problematic. A consume function that can fail often returns bool or throws.
            // Given our error list approach:
            // We'll return currentTokenInfo (the error token) to signal to the caller what was found.
            // The caller should check errors.
            return currentTokenInfo; // Return the problematic token
        }
    }

    bool ScriptParser::is_at_end_of_token_stream() const
    {
        // This check is against the *currently lexed token*, not the raw character stream.
        return currentTokenInfo.type == TokenType::EndOfFile;
    }

    bool ScriptParser::can_parse_as_generic_arguments_followed_by_call()
    {
        // --- 1. Save Parser State ---
        size_t original_char_offset = currentCharOffset;
        int original_line = currentLine;
        int original_column = currentColumn;
        size_t original_line_start_offset = currentLineStartOffset;
        CurrentTokenInfo original_current_token_info = currentTokenInfo;
        CurrentTokenInfo original_previous_token_info = previousTokenInfo;

        // Save and clear errors for this tentative parse.
        // We don't want errors from this speculative parse to affect the main error list.
        std::vector<ParseError> original_errors_backup = errors;
        errors.clear(); // Clear for the duration of this function

        bool is_likely_generic_call = false;

        // We expect currentTokenInfo.type == TokenType::LessThan when this is called
        if (!check_token(TokenType::LessThan))
        {
            errors = original_errors_backup; // Restore errors before returning
            return false;                    // Should not happen if called correctly
        }

        advance_and_lex(); // Tentatively consume '<'

        // --- 2. Tentatively Parse Type Arguments ---
        //    (Simplified: just checks structure, doesn't build full TypeNameNodes)
        if (!check_token(TokenType::GreaterThan))
        { // Allow empty <>
            bool first_type_arg = true;
            do
            {
                if (!first_type_arg)
                {
                    if (match_token(TokenType::Comma))
                    { // Tentatively consumes ','
                      // continue
                    }
                    else
                    {
                        goto end_trial_parse; // Not a valid comma-separated list
                    }
                }
                first_type_arg = false;

                // Simplified "can_parse_type_name_light" for lookahead:
                // Checks for an identifier or primitive type keyword, and skips simple qualifications/arrays.
                if (check_token(TokenType::Identifier) ||
                    check_token({TokenType::Bool, TokenType::Int, TokenType::String, TokenType::Long, TokenType::Double, TokenType::Char, TokenType::Void, TokenType::Float})) // Added Float
                {

                    advance_and_lex(); // Tentatively consume the start of the type name (identifier or keyword)

                    // Tentatively skip over .identifier sequences (qualified name parts)
                    while (check_token(TokenType::Dot))
                    {
                        advance_and_lex(); // Consume '.'
                        if (check_token(TokenType::Identifier))
                        {
                            advance_and_lex(); // Consume identifier
                        }
                        else
                        {
                            goto end_trial_parse; // Malformed qualified name
                        }
                    }

                    // Tentatively skip over generic arguments for this type argument (e.g., List<int>)
                    // This part is tricky to do without full recursion. For a predicate, we might
                    // just check for balanced < > or make it very simple.
                    // For now, a simple skip of one level of matching < > if present.
                    if (check_token(TokenType::LessThan))
                    {
                        int angle_bracket_depth = 1;
                        advance_and_lex(); // Consume opening '<'
                        while (angle_bracket_depth > 0 && !is_at_end_of_token_stream())
                        {
                            if (check_token(TokenType::LessThan))
                                angle_bracket_depth++;
                            else if (check_token(TokenType::GreaterThan))
                                angle_bracket_depth--;
                            if (angle_bracket_depth == 0 && check_token(TokenType::GreaterThan))
                            { // Matched the initial one
                                // advance_and_lex() will happen for this matching '>' outside this if
                                break;
                            }
                            advance_and_lex();
                        }
                        if (angle_bracket_depth != 0)
                            goto end_trial_parse; // Unbalanced generics in type arg
                    }

                    // Tentatively skip over array specifiers []
                    if (check_token(TokenType::OpenBracket))
                    {
                        advance_and_lex(); // Tentatively consume '['
                        if (!match_token(TokenType::CloseBracket))
                            goto end_trial_parse; // Tentatively consume ']'
                    }
                }
                else
                {
                    goto end_trial_parse; // Not a type name
                }
            } while (!check_token(TokenType::GreaterThan) && !is_at_end_of_token_stream());
        }

        // --- 3. Check for '>' ---
        if (match_token(TokenType::GreaterThan))
        { // Tentatively consumes '>'
            // --- 4. Check for '(' ---
            if (check_token(TokenType::OpenParen))
            {
                is_likely_generic_call = true;
            }
        }

    end_trial_parse:
        // --- 5. Restore Parser State ---
        currentCharOffset = original_char_offset;
        currentLine = original_line;
        currentColumn = original_column;
        currentLineStartOffset = original_line_start_offset;
        currentTokenInfo = original_current_token_info;
        previousTokenInfo = original_previous_token_info;
        errors = original_errors_backup; // Restore original errors, discarding any from the trial

        return is_likely_generic_call;
    }

#pragma endregion

#pragma region "AST Node Helpers"
    template <typename T>
    std::shared_ptr<T> ScriptParser::make_ast_node(const SourceLocation &start_loc)
    {
        auto node = std::make_shared<T>();
        node->location = start_loc;
        // If m_current_parent_node is used: node->parent = m_current_parent_node;
        return node;
    }

    void ScriptParser::finalize_node_location(std::shared_ptr<AstNode> node)
    {
        if (!node || !node->location.has_value())
            return; // Should have start location already

        // End location is based on the end of the previously consumed token
        node->location->lineEnd = previousTokenInfo.location.lineEnd;
        node->location->columnEnd = previousTokenInfo.location.columnEnd;
    }

    std::shared_ptr<TokenNode> ScriptParser::create_token_node(TokenType type, const CurrentTokenInfo &token_info)
    {
        // Assuming TokenNode's constructor or an init method takes location and text.
        // For now, make_ast_node sets start_loc, finalize_node_location sets end_loc.
        // We need to ensure TokenNode itself stores its full original location.
        auto node = make_ast_node<TokenNode>(token_info.location); // This sets the start and end initially to token_info.location
        node->tokenType = type;                                    // Add TokenType to TokenNode AST if not already there
        node->text = std::string(token_info.lexeme);
        // The location in TokenNode should be the token's precise span.
        // make_ast_node already sets node->location to token_info.location.
        // If TokenNode needs its own explicit start/end separate from AstNode's optional location:
        // node->start_line = token_info.location.lineStart;
        // node->end_column = token_info.location.columnEnd;
        // For now, relying on AstNode::location.
        return node;
    }

    std::shared_ptr<IdentifierNode> ScriptParser::create_identifier_node(const CurrentTokenInfo &token_info)
    {
        auto node = make_ast_node<IdentifierNode>(token_info.location);
        node->name = std::string(token_info.lexeme);
        // Location is set by make_ast_node
        return node;
    }

#pragma endregion

#pragma region "Lexing"

    CurrentTokenInfo ScriptParser::lex_number_literal()
    {
        CurrentTokenInfo token_info;
        token_info.location.fileName = std::string(fileName);
        token_info.location.lineStart = currentLine;
        token_info.location.columnStart = currentColumn;

        size_t token_start_offset = currentCharOffset;
        bool is_floating_point = false; // Changed from is_double to be more general before suffix
        bool has_exponent = false;

        // Consume leading digits
        while (!is_at_end_of_source() && std::isdigit(static_cast<unsigned char>(peek_char())))
        {
            consume_char();
        }

        // Check for fractional part
        if (!is_at_end_of_source() && peek_char() == '.')
        {
            if (std::isdigit(static_cast<unsigned char>(peek_char(1))))
            {
                is_floating_point = true;
                consume_char(); // Consume '.'
                while (!is_at_end_of_source() && std::isdigit(static_cast<unsigned char>(peek_char())))
                {
                    consume_char();
                }
            }
            // If not followed by a digit, it's an integer followed by a Dot token.
        }

        // Basic exponent support (e.g., 1e10, 1.0E-5)
        if (!is_at_end_of_source() && (peek_char() == 'e' || peek_char() == 'E'))
        {
            is_floating_point = true; // Numbers with exponents are floating point
            has_exponent = true;
            consume_char(); // Consume 'e' or 'E'

            if (!is_at_end_of_source() && (peek_char() == '+' || peek_char() == '-'))
            {
                consume_char(); // Consume sign
            }

            if (is_at_end_of_source() || !std::isdigit(static_cast<unsigned char>(peek_char())))
            {
                SourceLocation error_loc = {currentLine, currentLine, currentColumn, currentColumn};
                error_loc.fileName = std::string(fileName);
                record_error("Exponent in number literal lacks digits.", error_loc);
                // Lexeme will include the 'e' and sign if present. Conversion will likely fail.
            }
            else
            {
                while (!is_at_end_of_source() && std::isdigit(static_cast<unsigned char>(peek_char())))
                {
                    consume_char();
                }
            }
        }

        token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
        token_info.type = TokenType::IntegerLiteral; // Default, will be overridden by suffixes or if floating point

        // Handle Suffixes
        char suffix = '\0';
        if (!is_at_end_of_source())
        {
            suffix = peek_char();
            if (suffix == 'L' || suffix == 'l')
            {
                if (is_floating_point)
                {
                    record_error("Suffix 'L'/'l' cannot be applied to a floating-point literal.", token_info.location);
                    token_info.type = TokenType::Error;
                }
                else
                {
                    token_info.type = TokenType::LongLiteral;
                    consume_char(); // Consume 'L' or 'l'
                    token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
                }
            }
            else if (suffix == 'F' || suffix == 'f')
            {
                token_info.type = TokenType::FloatLiteral;
                is_floating_point = true; // Ensure it's treated as float for conversion
                consume_char();           // Consume 'F' or 'f'
                token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
            }
            else if (suffix == 'D' || suffix == 'd')
            {
                // Optional 'd' or 'D' suffix for double, default for floating point without suffix
                token_info.type = TokenType::DoubleLiteral;
                is_floating_point = true;
                consume_char(); // Consume 'D' or 'd'
                token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
            }
            // Note: Other suffixes like 'U', 'UL', 'M' (decimal) could be added here.
        }

        token_info.location.lineEnd = currentLine;
        token_info.location.columnEnd = currentColumn - 1; // Column of the last char of the token

        // Value Conversion (if no error type set by suffix check)
        if (token_info.type != TokenType::Error)
        {
            if (is_floating_point || token_info.type == TokenType::FloatLiteral || token_info.type == TokenType::DoubleLiteral)
            {
                if (token_info.type == TokenType::IntegerLiteral)
                {                                               // Was integer, but became float due to . or E and no suffix
                    token_info.type = TokenType::DoubleLiteral; // Default float type is double
                }
                try
                {
                    std::string value_str = std::string(token_info.lexeme);
                    // Remove suffix if it's still there for std::stod/stof (e.g. if 'd' was optional)
                    // Our current logic updates lexeme after consuming suffix, so this might not be needed.
                    char last_char_of_str = value_str.empty() ? '\0' : value_str.back();
                    if (last_char_of_str == 'F' || last_char_of_str == 'f' || last_char_of_str == 'D' || last_char_of_str == 'd')
                    {
                        // This case should be handled by the lexeme update above if suffix was consumed for type.
                        // If not, need to strip it here for stod/stof.
                        // Given the current logic, lexeme is updated AFTER suffix consumption for typed literals.
                    }

                    if (token_info.type == TokenType::FloatLiteral)
                    {
                        token_info.literalValue = std::stof(value_str);
                    }
                    else
                    { // DoubleLiteral or default floating point
                        token_info.literalValue = std::stod(value_str);
                    }
                }
                catch (const std::out_of_range &)
                {
                    record_error("Floating point literal out of range: " + std::string(token_info.lexeme), token_info.location);
                    token_info.type = TokenType::Error;
                    token_info.literalValue = 0.0;
                }
                catch (const std::invalid_argument &)
                {
                    record_error("Invalid floating point literal format: " + std::string(token_info.lexeme), token_info.location);
                    token_info.type = TokenType::Error;
                    token_info.literalValue = 0.0;
                }
            }
            else
            { // Integer or Long
                try
                {
                    if (token_info.type == TokenType::LongLiteral)
                    {
                        token_info.literalValue = std::stoll(std::string(token_info.lexeme));
                    }
                    else
                    { // IntegerLiteral
                        // Could attempt int, then long if it overflows int, or just use long long for IntegerLiteral storage
                        // For simplicity, let's use long long for IntegerLiteral too, and semantic analysis can check range.
                        // Or, strictly, parse as int32 and error on overflow if 'int' is fixed 32-bit.
                        // Using stoll for IntegerLiteral for now, matching earlier behavior.
                        // A true 'int' might need std::stoi and careful range checking.
                        token_info.literalValue = std::stoll(std::string(token_info.lexeme));
                    }
                }
                catch (const std::out_of_range &)
                {
                    record_error("Integer/Long literal out of range: " + std::string(token_info.lexeme), token_info.location);
                    token_info.type = TokenType::Error;
                    token_info.literalValue = static_cast<int64_t>(0);
                }
                catch (const std::invalid_argument &)
                {
                    record_error("Invalid integer/long literal format: " + std::string(token_info.lexeme), token_info.location);
                    token_info.type = TokenType::Error;
                    token_info.literalValue = static_cast<int64_t>(0);
                }
            }
        }

        return token_info;
    }

    CurrentTokenInfo ScriptParser::lex_string_literal()
    {
        CurrentTokenInfo token_info;
        token_info.location.fileName = std::string(fileName);
        token_info.location.lineStart = currentLine;
        token_info.location.columnStart = currentColumn;
        token_info.type = TokenType::StringLiteral;

        size_t token_start_offset = currentCharOffset;

        consume_char(); // Consume the opening double quote "

        std::string string_value;
        bool properly_terminated = false;

        while (!is_at_end_of_source())
        {
            char ch = peek_char();

            if (ch == '"')
            {
                consume_char(); // Consume the closing double quote "
                properly_terminated = true;
                break;
            }

            if (ch == '\\')
            {                   // Escape sequence
                consume_char(); // Consume '\'
                if (is_at_end_of_source())
                {
                    // Error: escape sequence at end of file
                    record_error("String literal has unterminated escape sequence at end of file", token_info.location);
                    // The string will be unterminated, handled below
                    break;
                }
                char escaped_char = consume_char(); // Consume char after '\'
                switch (escaped_char)
                {
                case 'n':
                    string_value += '\n';
                    break;
                case 't':
                    string_value += '\t';
                    break;
                case 'r':
                    string_value += '\r';
                    break;
                case '\\':
                    string_value += '\\';
                    break;
                case '"':
                    string_value += '"';
                    break;
                // Add more escapes like \', \0, \xHH, \uHHHH if needed later
                default:
                    // Invalid escape sequence
                    string_value += '\\';         // Store the backslash
                    string_value += escaped_char; // Store the char after backslash
                    SourceLocation err_loc = {currentLine, currentLine, currentColumn - 2, currentColumn - 1};
                    err_loc.fileName = std::string(fileName);
                    record_error("Unknown escape sequence '\\" + std::string(1, escaped_char) + "' in string literal", err_loc);
                    break;
                }
            }
            else if (ch == '\n' || ch == '\r')
            {
                // Error: Newline in string literal without escape (C#-like behavior)
                // Some languages allow multi-line strings without escapes, but C# doesn't for regular strings.
                record_error("Newline in string literal. Use verbatim strings (@\"...\") or escape sequences.", token_info.location);
                // We'll let it terminate here for simplicity of this step.
                // A more robust parser might try to recover or continue to find the closing quote.
                properly_terminated = false; // Mark as not properly terminated
                break;                       // Stop processing this string literal
            }
            else
            {
                string_value += consume_char();
            }
        }

        if (!properly_terminated)
        {
            record_error("Unterminated string literal", token_info.location);
            token_info.type = TokenType::Error; // Mark the token itself as an error token
        }

        token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
        token_info.location.lineEnd = currentLine;
        token_info.location.columnEnd = currentColumn - 1; // Column of the last char of the token (or where error occurred)
        token_info.literalValue = string_value;

        return token_info;
    }

    CurrentTokenInfo ScriptParser::lex_char_literal()
    {
        CurrentTokenInfo token_info;
        token_info.location.fileName = std::string(fileName);
        token_info.location.lineStart = currentLine;
        token_info.location.columnStart = currentColumn;
        token_info.type = TokenType::CharLiteral;

        size_t token_start_offset = currentCharOffset;
        char char_value = '\0'; // Default
        int char_count = 0;     // Number of actual characters (after unescaping)

        consume_char(); // Consume the opening single quote '

        if (is_at_end_of_source() || peek_char() == '\'')
        {
            record_error("Empty character literal", token_info.location);
            token_info.type = TokenType::Error;
            if (!is_at_end_of_source() && peek_char() == '\'')
                consume_char(); // Consume closing ' if present
        }
        else
        {
            char ch = peek_char();
            if (ch == '\\')
            {                   // Escape sequence
                consume_char(); // Consume '\'
                if (is_at_end_of_source())
                {
                    record_error("Character literal has unterminated escape sequence at end of file", token_info.location);
                    token_info.type = TokenType::Error;
                }
                else
                {
                    char escaped_char = consume_char();
                    switch (escaped_char)
                    {
                    case 'n':
                        char_value = '\n';
                        break;
                    case 't':
                        char_value = '\t';
                        break;
                    case 'r':
                        char_value = '\r';
                        break;
                    case '\\':
                        char_value = '\\';
                        break;
                    case '\'':
                        char_value = '\'';
                        break;
                    // Add more escapes if needed
                    default:
                        char_value = escaped_char; // Store the char after backslash
                        SourceLocation err_loc = {currentLine, currentLine, currentColumn - 2, currentColumn - 1};
                        err_loc.fileName = std::string(fileName);
                        record_error("Unknown escape sequence '\\" + std::string(1, escaped_char) + "' in char literal", err_loc);
                        // Still count it as one character for the "too many chars" check
                        break;
                    }
                    char_count = 1;
                }
            }
            else if (ch == '\n' || ch == '\r')
            {
                record_error("Newline in character literal", token_info.location);
                token_info.type = TokenType::Error;
                // Do not consume the newline, let the main loop handle it or EOF.
            }
            else
            {
                char_value = consume_char();
                char_count = 1;
            }

            if (token_info.type != TokenType::Error)
            { // Only proceed if not already errored
                if (is_at_end_of_source() || peek_char() != '\'')
                {
                    record_error("Unterminated character literal or too many characters", token_info.location);
                    token_info.type = TokenType::Error;
                    // Try to consume until ' or newline or EOF to recover somewhat
                    while (!is_at_end_of_source() && peek_char() != '\'' && peek_char() != '\n' && peek_char() != '\r')
                    {
                        consume_char();
                    }
                    if (!is_at_end_of_source() && peek_char() == '\'')
                        consume_char();
                }
                else
                {
                    consume_char(); // Consume the closing single quote '
                    if (char_count > 1)
                    { // This logic needs refinement if multi-char escapes like \u were added
                        record_error("Too many characters in character literal", token_info.location);
                        token_info.type = TokenType::Error;
                    }
                }
            }
        }

        token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
        token_info.location.lineEnd = currentLine;
        token_info.location.columnEnd = currentColumn - 1;
        if (token_info.type != TokenType::Error)
        {
            token_info.literalValue = char_value;
        }
        else
        {
            token_info.literalValue = '\0'; // Default for error
        }

        return token_info;
    }

    CurrentTokenInfo ScriptParser::lex_identifier_or_keyword()
    {
        CurrentTokenInfo token_info;
        token_info.location.fileName = std::string(fileName);
        token_info.location.lineStart = currentLine;
        token_info.location.columnStart = currentColumn;

        size_t token_start_offset = currentCharOffset;

        // Consume the first character, which is already known to be valid for an identifier start
        consume_char();

        // Consume subsequent characters of the identifier
        while (!is_at_end_of_source())
        {
            char ch = peek_char();
            if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')
            {
                consume_char();
            }
            else
            {
                break;
            }
        }

        token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
        token_info.location.lineEnd = currentLine;
        token_info.location.columnEnd = currentColumn - 1; // Column of the last char of the token

        auto keyword_it = keywords.find(token_info.lexeme);
        if (keyword_it != keywords.end())
        {
            token_info.type = keyword_it->second;
            // Handle boolean/null literal values if type is Bool or Null
            if (token_info.type == TokenType::Bool)
            {
                token_info.literalValue = (token_info.lexeme == "true");
            }
            else if (token_info.type == TokenType::NullLiteral)
            {
                // Null doesn't really have a value other than its type, but can be represented.
                // Using monostate or a specific nullopt_t like value might be good.
                // For now, string "null" or monostate is fine.
                token_info.literalValue = std::monostate{}; // Or a specific "null" marker
            }
        }
        else
        {
            token_info.type = TokenType::Identifier;
            // For identifiers, literalValue remains monostate or could store the string itself if needed elsewhere
        }

        return token_info;
    }

    CurrentTokenInfo ScriptParser::lex_operator_or_punctuation()
    {
        CurrentTokenInfo token_info;
        token_info.location.fileName = std::string(fileName);
        token_info.location.lineStart = currentLine;
        token_info.location.columnStart = currentColumn;

        size_t token_start_offset = currentCharOffset;
        char c1 = consume_char(); // Consume the first character

        // Default to the single character, update if it's multi-character
        token_info.type = TokenType::Error; // Fallback
        char c2 = peek_char(0);             // Look at the next character without consuming

        // Handle multi-character tokens first
        switch (c1)
        {
        case '+':
            if (c2 == '=')
            { // +=
                consume_char();
                token_info.type = TokenType::PlusAssign; // Assuming you'll add PlusAssign, etc.
            }
            else if (c2 == '+')
            { // ++
                consume_char();
                token_info.type = TokenType::Increment; // Assuming Increment
            }
            else
            {
                token_info.type = TokenType::Plus;
            }
            break;
        case '-':
            if (c2 == '=')
            { // -=
                consume_char();
                token_info.type = TokenType::MinusAssign;
            }
            else if (c2 == '-')
            { // --
                consume_char();
                token_info.type = TokenType::Decrement;
            }
            else if (c2 == '>')
            { // -> (Arrow operator if you add it)
                // consume_char();
                // token_info.type = TokenType::Arrow;
                token_info.type = TokenType::Minus; // For now, just minus if no arrow
            }
            else
            {
                token_info.type = TokenType::Minus;
            }
            break;
        case '*':
            if (c2 == '=')
            { // *=
                consume_char();
                token_info.type = TokenType::AsteriskAssign;
            }
            else
            {
                token_info.type = TokenType::Asterisk;
            }
            break;
        case '/':
            if (c2 == '=')
            { // /=
                consume_char();
                token_info.type = TokenType::SlashAssign;
            }
            else
            {
                token_info.type = TokenType::Slash;
            }
            break;
        case '%':
            if (c2 == '=')
            { // %=
                consume_char();
                token_info.type = TokenType::PercentAssign;
            }
            else
            {
                token_info.type = TokenType::Percent;
            }
            break;
        case '=':
            if (c2 == '=')
            { // ==
                consume_char();
                token_info.type = TokenType::EqualsEquals;
            }
            else
            {
                token_info.type = TokenType::Assign;
            }
            break;
        case '!':
            if (c2 == '=')
            { // !=
                consume_char();
                token_info.type = TokenType::NotEquals;
            }
            else
            {
                token_info.type = TokenType::LogicalNot;
            }
            break;
        case '<':
            if (c2 == '=')
            { // <=
                consume_char();
                token_info.type = TokenType::LessThanOrEqual;
            }
            else
            {
                token_info.type = TokenType::LessThan;
            }
            break;
        case '>':
            if (c2 == '=')
            { // >=
                consume_char();
                token_info.type = TokenType::GreaterThanOrEqual;
            }
            else
            {
                token_info.type = TokenType::GreaterThan;
            }
            break;
        case '&':
            if (c2 == '&')
            { // &&
                consume_char();
                token_info.type = TokenType::LogicalAnd;
            }
            else
            {
                // token_info.type = TokenType::BitwiseAnd; // If you add bitwise
                token_info.type = TokenType::Error; // For now, single '&' is an error or unhandled
                record_error("Unexpected character '&'. Did you mean '&&'?", token_info.location);
            }
            break;
        case '|':
            if (c2 == '|')
            { // ||
                consume_char();
                token_info.type = TokenType::LogicalOr;
            }
            else
            {
                // token_info.type = TokenType::BitwiseOr; // If you add bitwise
                token_info.type = TokenType::Error; // For now, single '|' is an error or unhandled
                record_error("Unexpected character '|'. Did you mean '||'?", token_info.location);
            }
            break;
        // Single character tokens
        case '(':
            token_info.type = TokenType::OpenParen;
            break;
        case ')':
            token_info.type = TokenType::CloseParen;
            break;
        case '{':
            token_info.type = TokenType::OpenBrace;
            break;
        case '}':
            token_info.type = TokenType::CloseBrace;
            break;
        case '[':
            token_info.type = TokenType::OpenBracket;
            break; // Assuming OpenBracket
        case ']':
            token_info.type = TokenType::CloseBracket;
            break; // Assuming CloseBracket
        case ';':
            token_info.type = TokenType::Semicolon;
            break;
        case ',':
            token_info.type = TokenType::Comma;
            break;
        case '.':
            token_info.type = TokenType::Dot;
            break;
        // Add case for ':', '?', etc. if they are part of your language
        // case ':': token_info.type = TokenType::Colon; break;
        // case '?': token_info.type = TokenType::QuestionMark; break;
        default:
            // If we reach here, it means c1 was not a recognized operator start
            // This case should ideally be caught by the advance_and_lex dispatcher before calling this.
            // However, as a safeguard:
            token_info.type = TokenType::Error;
            // We already consumed c1, so current_char_offset is one past c1.
            // To report error on c1:
            SourceLocation error_loc = token_info.location; // start loc is fine
            error_loc.columnEnd = error_loc.columnStart;    // single char error
            error_loc.lineEnd = error_loc.lineStart;
            record_error("Unknown operator or punctuation character '" + std::string(1, c1) + "'", error_loc);
            // The lexeme below will be just c1.
            break;
        }

        token_info.lexeme = sourceCode.substr(token_start_offset, currentCharOffset - token_start_offset);
        token_info.location.lineEnd = currentLine;
        token_info.location.columnEnd = currentColumn - 1; // Column of the last char of the token
        token_info.literalValue = std::monostate{};        // Operators don't have literal values in this sense

        return token_info;
    }

    void ScriptParser::skip_whitespace_and_comments()
    {
        while (!is_at_end_of_source())
        {
            char current_char = peek_char();

            if (current_char == ' ' || current_char == '\t' || current_char == '\r' || current_char == '\n')
            {
                consume_char(); // consume_char handles line/column updates
            }
            else if (current_char == '/' && peek_char(1) == '/')
            {
                // Single-line comment
                consume_char(); // consume '/'
                consume_char(); // consume '/'
                while (!is_at_end_of_source() && peek_char() != '\n')
                {
                    consume_char();
                }
                // consume_char() will handle the '\n' if present in the next iteration, or EOF will be hit
            }
            else if (current_char == '/' && peek_char(1) == '*')
            {
                // Multi-line comment
                SourceLocation comment_start_loc;
                comment_start_loc.fileName = std::string(fileName);
                comment_start_loc.lineStart = currentLine;
                comment_start_loc.columnStart = currentColumn;

                consume_char(); // consume '/'
                consume_char(); // consume '*'

                bool comment_closed = false;
                while (!is_at_end_of_source())
                {
                    char c = consume_char();
                    if (c == '*' && peek_char() == '/')
                    {
                        consume_char(); // consume '/'
                        comment_closed = true;
                        break;
                    }
                }
                if (!comment_closed)
                {
                    record_error("Unterminated multi-line comment", comment_start_loc);
                    // The parser will likely hit EOF next and report that if this error isn't fatal.
                }
            }
            else
            {
                // Not whitespace or a comment starter
                break;
            }
        }
    }

    void ScriptParser::advance_and_lex()
    {
        previousTokenInfo = currentTokenInfo;

        skip_whitespace_and_comments();

        if (is_at_end_of_source())
        {
            currentTokenInfo.type = TokenType::EndOfFile;
            currentTokenInfo.lexeme = "";
            currentTokenInfo.location.fileName = std::string(fileName);
            currentTokenInfo.location.lineStart = currentLine;
            currentTokenInfo.location.columnStart = currentColumn;
            currentTokenInfo.location.lineEnd = currentLine;
            currentTokenInfo.location.columnEnd = currentColumn;
            currentTokenInfo.literalValue = std::monostate{};
        }
        else
        {
            char first_char = peek_char();

            if (std::isalpha(static_cast<unsigned char>(first_char)) || first_char == '_')
            {
                currentTokenInfo = lex_identifier_or_keyword();
            }
            else if (std::isdigit(static_cast<unsigned char>(first_char)))
            {
                currentTokenInfo = lex_number_literal();
            }
            else if (first_char == '"')
            {
                currentTokenInfo = lex_string_literal();
            }
            else if (first_char == '\'')
            {
                currentTokenInfo = lex_char_literal();
            }
            else
            {
                // Assume it's an operator or punctuation if none of the above
                // This includes characters like '(', ')', '{', '}', '+', '-', '=', etc.
                currentTokenInfo = lex_operator_or_punctuation();
            }
        }

        // Debug Print (Optional)
        std::cout << "LEXED TOKEN: Type=" << token_type_to_string(currentTokenInfo.type)
                  << ", Lexeme='";
        std::cout.write(currentTokenInfo.lexeme.data(), currentTokenInfo.lexeme.length());
        std::cout << "', Loc=" << currentTokenInfo.location.to_string();
        std::cout << std::endl;
    }

#pragma endregion

#pragma region "Lexing Helpers"

    char ScriptParser::peek_char(size_t offset) const
    {
        if (currentCharOffset + offset >= sourceCode.length())
        {
            return '\0'; // End of source
        }
        return sourceCode[currentCharOffset + offset];
    }

    char ScriptParser::consume_char()
    {
        if (is_at_end_of_source())
        {
            return '\0';
        }
        char current_char = sourceCode[currentCharOffset];
        currentCharOffset++;

        if (current_char == '\n')
        {
            currentLine++;
            currentColumn = 1;
            currentLineStartOffset = currentCharOffset;
        }
        else
        {
            // TODO: Handle UTF-8 correctly for column counting if necessary.
            // For ASCII or fixed-width, this is fine.
            currentColumn++;
        }
        return current_char;
    }

    bool ScriptParser::is_at_end_of_source() const
    {
        return currentCharOffset >= sourceCode.length();
    }

#pragma endregion

#pragma region "Error Handling"

    void ScriptParser::record_error(const std::string &message, const SourceLocation &loc)
    {
        errors.emplace_back(message, loc);
    }

    void ScriptParser::record_error_at_current(const std::string &message)
    {
        record_error(message, currentTokenInfo.location);
    }

    void ScriptParser::record_error_at_previous(const std::string &message)
    {
        // Ensure previousTokenInfo is valid (e.g., not the initial dummy state)
        // For instance, check if its type is not the initial Error type or if its location is meaningful.
        // However, its location should always be updated by advance_and_lex.
        record_error(message, previousTokenInfo.location);
    }

#pragma endregion

} // namespace Mycelium::Scripting::Lang
