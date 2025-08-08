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
        bool check_declaration();
        ParseResult<DeclarationNode> parse_declaration();

        // Specific declaration type parsers
        ParseResult<DeclarationNode> parse_function_declaration();
        ParseResult<DeclarationNode> parse_constructor_declaration();
        ParseResult<DeclarationNode> parse_type_declaration();
        ParseResult<DeclarationNode> parse_enum_declaration();
        ParseResult<StatementNode> parse_using_directive();
        ParseResult<DeclarationNode> parse_namespace_declaration();

        // Variable declaration parsing - used by both StatementParser and DeclarationParser
        ParseResult<DeclarationNode> parse_variable_declaration();

        // Supporting parsers
        ParseResult<AstNode> parse_parameter_list();
        ParseResult<ParameterNode> parse_parameter();
        ParseResult<ParameterNode> parse_enum_parameter();
        ParseResult<EnumCaseNode> parse_enum_case();
        ParseResult<AstNode> parse_generic_parameters();
        ParseResult<AstNode> parse_generic_constraints();
        ParseResult<StatementNode> parse_for_variable_declaration();

    private:
        std::vector<ModifierKind> parse_all_modifiers();
    };

} // namespace Myre