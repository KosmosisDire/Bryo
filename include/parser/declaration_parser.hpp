#pragma once

#include "ast/ast.hpp"
#include "parse_result.hpp"
#include "parser_base.hpp"

namespace Myre
{

    /**
     * DeclarationParser - Handles all declaration parsing
     *
     * Responsibilities:
     * - Parse function declarations with parameters, return types, and bodies
     * - Parse type declarations (structs/classes) with members and inheritance
     * - Parse enum declarations with associated data
     * - Parse using directives and namespace declarations
     * - Handle access modifiers (public, private, protected)
     * - Support generic parameters and constraints
     */
    class DeclarationParser : public ParserBase
    {

    public:
        explicit DeclarationParser(Parser *parser);

        // Main declaration parsing entry point
        ParseResult<DeclarationNode> parse_declaration();

        // Specific declaration type parsers
        ParseResult<DeclarationNode> parse_function_declaration();
        ParseResult<DeclarationNode> parse_type_declaration();
        ParseResult<DeclarationNode> parse_enum_declaration();
        ParseResult<StatementNode> parse_using_directive();
        ParseResult<DeclarationNode> parse_namespace_declaration();

        // Variable declaration parsing - used by both StatementParser and DeclarationParser
        ParseResult<VariableDeclarationNode> parse_variable_declaration();
        ParseResult<VariableDeclarationNode> parse_typed_variable_declaration();

        // Supporting parsers
        ParseResult<AstNode> parse_parameter_list();
        ParseResult<ParameterNode> parse_parameter();
        ParseResult<ParameterNode> parse_enum_parameter();
        ParseResult<AstNode> parse_type_member();
        ParseResult<AstNode> parse_var_field_declaration(const std::vector<ModifierKind> &modifiers);
        ParseResult<EnumCaseNode> parse_enum_case();
        ParseResult<AstNode> parse_generic_parameters();
        ParseResult<AstNode> parse_generic_constraints();

    private:
        // Helper methods
        ModifierKind parse_access_modifiers();
        std::vector<ModifierKind> parse_all_modifiers();
        bool is_declaration_start();
        ParseResult<AstNode> parse_member_declaration();
    };

} // namespace Myre