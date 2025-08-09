#pragma once

#include "ast/ast.hpp"
#include "parse_result.hpp"
#include "token_stream.hpp"
#include <vector>
#include <memory>
#include <iostream>
#include <common/logger.hpp>

namespace Myre
{

    // Diagnostic collection for error reporting
    struct Diagnostic
    {
        std::string message;
        SourceRange location;

        Diagnostic(const std::string &msg, const SourceRange &loc)
            : message(msg), location(loc) {}

        std::string to_string() const
        {
            return "Error (" + location.start.to_string() + "): " + message;
        }
    };

    class DiagnosticCollection
    {
    private:
        std::vector<Diagnostic> diagnostics_;

    public:
        void add(const Diagnostic &diagnostic)
        {
            diagnostics_.push_back(diagnostic);
        }

        const std::vector<Diagnostic> &get_diagnostics() const
        {
            return diagnostics_;
        }

        auto begin() const { return diagnostics_.begin(); }
        auto end() const { return diagnostics_.end(); }
        size_t size() const { return diagnostics_.size(); }
        bool empty() const { return diagnostics_.empty(); }

        void print()
        {
            for (const auto &diag : diagnostics_)
            {
                LOG_ERROR(diag.to_string(), LogCategory::PARSER);
            }
        }
    };

    // Main Parser class
    class Parser
    {
    private:
        AstAllocator alloc;
        TokenStream &tokens;

    public:
        DiagnosticCollection diag;
        Parser(TokenStream &tokens);
        ~Parser();

        // Main parsing entry point
        ParseResult<CompilationUnitNode> parse();

        void recover_to_safe_point()
        {
            while (!tokens.at_end())
            {
                TokenKind kind = tokens.current().kind;

                if (kind == TokenKind::Semicolon ||  // Statement separator
                    kind == TokenKind::LeftBrace ||  // Block start
                    kind == TokenKind::RightBrace || // Block end
                    kind == TokenKind::Fn ||         // Function declaration
                    kind == TokenKind::Type ||       // Type declaration
                    kind == TokenKind::If ||         // If statement
                    kind == TokenKind::While ||      // While statement
                    kind == TokenKind::For)          // For statement
                { 
                    // Skip semicolon, but stop at other safe points
                    if (kind == TokenKind::Semicolon || kind == TokenKind::RightBrace)
                        tokens.advance();
                    break;
                }
                tokens.advance();
            }
        }

        ParseResult<QualifiedNameNode> parse_qualified_name()
        {
            if (!tokens.check(TokenKind::Identifier))
            {
                return create_error<QualifiedNameNode>("Expected identifier");
            }

            auto *name = alloc.alloc<QualifiedNameNode>();

            std::vector<IdentifierNode*> identifiers;

            // Parse first identifier
            while (true)
            {
                if (!tokens.check(TokenKind::Identifier))
                {
                    diag.add(Diagnostic("Expected identifier in type name", tokens.location()));
                    break;
                }

                const Token &id_token = tokens.current();
                tokens.advance();

                auto *identifier = alloc.alloc<IdentifierNode>();
                identifier->name = id_token.text;
                identifiers.push_back(identifier);

                // If next token is '.', continue parsing next identifier
                if (tokens.check(TokenKind::Dot))
                {
                    tokens.advance(); // consume '.'
                    continue;
                }
                break;
            }

            // allocate sized array for identifiers
            name->identifiers.values = alloc.alloc_array<IdentifierNode*>(identifiers.size());
            name->identifiers.size = static_cast<int>(identifiers.size());
            for (size_t i = 0; i < identifiers.size(); ++i)
            {
                name->identifiers.values[i] = identifiers[i];
            }

            return ParseResult<QualifiedNameNode>::success(name);
        }

        // Type parsing helper
        ParseResult<TypeNameNode> parse_type_expression()
        {
            // Parse qualified name
            auto qname_result = parse_qualified_name();
            if (qname_result.is_error())
            {
                return create_error<TypeNameNode>("Expected type name");
            }

            auto *type_name = alloc.alloc<TypeNameNode>();
            type_name->name = qname_result.get_node();
            type_name->location = type_name->name->location;

            // Check for array type suffix []
            if (tokens.check(TokenKind::LeftBracket))
            {
                tokens.advance(); // consume [

                // For now, we only support empty brackets [] for dynamic arrays
                // Later we can add support for fixed size arrays like [5]
                if (!tokens.check(TokenKind::RightBracket))
                {
                    // Skip to closing bracket
                    while (!tokens.check(TokenKind::RightBracket) && !tokens.at_end())
                    {
                        tokens.advance();
                    }
                }

                if (tokens.check(TokenKind::RightBracket))
                {
                    tokens.advance(); // consume ]

                    // Create ArrayTypeNameNode
                    auto *array_type = alloc.alloc<ArrayTypeNameNode>();
                    array_type->elementType = type_name;

                    return ParseResult<TypeNameNode>::success(array_type);
                }
                else
                {
                    return create_error<TypeNameNode>("Expected ']' after '['");
                }
            }

            return ParseResult<TypeNameNode>::success(type_name);
        }

        // Direct error creation for helper parsers
        template<typename T>
        ParseResult<T> create_error(const char *msg)
        {
            auto* node = alloc.alloc<ErrorNode>();
            node->location = tokens.current().location;
            diag.add(Diagnostic(msg, node->location));
            return ParseResult<T>::error(node);
        }

    private:
        // Top-level parsing dispatcher
        ParseResult<StatementNode> parse_top_level_construct();

        // Main statement parsing entry point
        ParseResult<StatementNode> parse_statement();

        // Specific statement type parsers
        ParseResult<StatementNode> parse_block_statement();
        ParseResult<StatementNode> parse_expression_statement();

        // Future statement types (to be implemented)
        ParseResult<StatementNode> parse_if_statement();
        ParseResult<StatementNode> parse_while_statement();
        ParseResult<StatementNode> parse_for_statement();
        ParseResult<StatementNode> parse_for_in_statement();
        ParseResult<StatementNode> parse_for_variable_declaration();
        ParseResult<StatementNode> parse_return_statement();
        ParseResult<StatementNode> parse_break_statement();
        ParseResult<StatementNode> parse_continue_statement();

        // Main declaration parsing entry point
        bool check_declaration();
        ParseResult<DeclarationNode> parse_declaration();

        // Specific declaration type parsers
        ParseResult<DeclarationNode> parse_function_declaration();
        ParseResult<DeclarationNode> parse_constructor_declaration();
        ParseResult<DeclarationNode> parse_type_declaration();
        ParseResult<DeclarationNode> parse_enum_declaration();
        ParseResult<StatementNode> parse_using_directive();
        ParseResult<DeclarationNode> parse_namespace_declaration();

        // Variable declaration parsing - used by both Parser and Parser
        ParseResult<DeclarationNode> parse_variable_declaration();

        // Supporting parsers
        ParseResult<AstNode> parse_parameter_list();
        ParseResult<ParameterNode> parse_parameter();
        ParseResult<ParameterNode> parse_enum_parameter();
        ParseResult<EnumCaseNode> parse_enum_case();
        ParseResult<AstNode> parse_generic_parameters();
        ParseResult<AstNode> parse_generic_constraints();

        // Pratt parser implementation for binary expressions
        ParseResult<ExpressionNode> parse_binary_expression(int min_precedence);

        std::vector<ModifierKind> parse_all_modifiers();

        // Main expression parsing entry point
        ParseResult<ExpressionNode> parse_expression(int min_precedence = 0);

        // Primary expression parsing - handles literals, identifiers, parentheses
        ParseResult<ExpressionNode> parse_primary();

        // Literal parsing methods
        ParseResult<ExpressionNode> parse_range_expression(ExpressionNode* left);
        ParseResult<ExpressionNode> parse_prefix_range_expression();
        ParseResult<ExpressionNode> parse_integer_literal();
        ParseResult<ExpressionNode> parse_float_literal();
        ParseResult<ExpressionNode> parse_double_literal();
        ParseResult<ExpressionNode> parse_string_literal();
        ParseResult<ExpressionNode> parse_boolean_literal();
        ParseResult<ExpressionNode> parse_identifier_or_call();
        ParseResult<ExpressionNode> parse_parenthesized_expression();

        // Unary expression parsing
        ParseResult<ExpressionNode> parse_unary_expression();

        // Postfix expression parsing helpers
        ParseResult<ExpressionNode> parse_call_suffix(ExpressionNode *target);
        ParseResult<ExpressionNode> parse_member_access_suffix(ExpressionNode *target);
        ParseResult<ExpressionNode> parse_indexer_suffix(ExpressionNode *target);

        // Operator precedence
        int get_precedence(TokenKind op);

        // Future expression types (to be implemented)
        ParseResult<ExpressionNode> parse_call_expression();
        ParseResult<ExpressionNode> parse_member_access();
        ParseResult<ExpressionNode> parse_new_expression();
        ParseResult<ExpressionNode> parse_match_expression();
        ParseResult<ExpressionNode> parse_enum_variant();

    };

} // namespace Myre