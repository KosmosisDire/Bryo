#pragma once

#include "ast/ast.hpp"
#include "ast/ast_allocator.hpp"
#include "parse_result.hpp"
#include "token_stream.hpp"
#include <vector>
#include <functional>
#include <map>

namespace Myre
{
    class Parser
    {

    public:
        Parser(TokenStream& tokens);
        ~Parser();
        
        // Main entry point
        ParseResult<CompilationUnitNode> parse();
        
        // Error tracking
        struct ParseError {
            std::string message;
            SourceRange location;
            enum Level { WARNING, ERROR, FATAL } level;
        };
        
        const std::vector<ParseError>& get_errors() const { return errors; }
        bool has_errors() const { return !errors.empty(); }
        ErrorNode* create_error_node(const char* msg);
        
    private:
        AstAllocator alloc;
        TokenStream& tokens;
        std::vector<ParseError> errors;
        
        // ================== Error Handling ==================
        void error(const std::string& msg);
        void warning(const std::string& msg);
        void synchronize();
        
        // ================== Utility Helpers ==================
        TokenNode* create_token_node(const Token& token);
        TokenNode* consume_token(TokenKind kind);
        
        template<typename T>
        SizedArray<T> make_sized_array(const std::vector<T>& vec);
        
        // ================== Pattern Helpers ==================
        // Core pattern: Type Identifier
        ParseResult<TypedIdentifier> try_parse_typed_identifier();
        ParseResult<IdentifierNode> parse_var_identifier();
        ParseResult<VariablePattern> parse_variable_pattern(bool allow_untyped = false);
        
        // ================== Top Level Parsing ==================
        ParseResult<StatementNode> parse_top_level_construct();
        ParseResult<CompilationUnitNode> parse_compilation_unit();
        
        // ================== Statements ==================
        ParseResult<StatementNode> parse_statement();
        ParseResult<StatementNode> parse_block_statement();
        ParseResult<StatementNode> parse_expression_statement();
        ParseResult<StatementNode> parse_if_statement();
        ParseResult<StatementNode> parse_while_statement();
        ParseResult<StatementNode> parse_for_statement();
        ParseResult<StatementNode> parse_for_in_statement();
        ParseResult<StatementNode> parse_return_statement();
        ParseResult<StatementNode> parse_break_statement();
        ParseResult<StatementNode> parse_continue_statement();
        ParseResult<StatementNode> parse_using_directive();
        
        // ================== Declarations ==================
        bool check_declaration();
        ParseResult<DeclarationNode> parse_declaration();
        ParseResult<DeclarationNode> parse_namespace_declaration();
        ParseResult<DeclarationNode> parse_type_declaration();
        ParseResult<DeclarationNode> parse_enum_declaration();
        ParseResult<DeclarationNode> parse_function_declaration();
        ParseResult<DeclarationNode> parse_constructor_declaration();
        ParseResult<DeclarationNode> parse_variable_declaration();
        ParseResult<DeclarationNode> parse_property_declaration();
        
        // ================== Declaration Helpers ==================
        ParseResult<ParameterNode> parse_parameter();
        ParseResult<EnumCaseNode> parse_enum_case();
        ParseResult<PropertyAccessorNode> parse_property_accessor();
        std::vector<ModifierKind> parse_modifiers();
        
        StatementNode* convert_to_statement(const VariablePattern& pattern);
        
        // ================== Types ==================
        ParseResult<TypeNameNode> try_parse_type();
        ParseResult<TypeNameNode> parse_type_expression();
        ParseResult<QualifiedNameNode> parse_qualified_name();
        ParseResult<TypeNameNode> parse_generic_arguments();
        
        // ================== Expressions ==================
        // Main expression parsing with precedence
        ParseResult<ExpressionNode> parse_expression(int min_precedence = 0);
        ParseResult<ExpressionNode> parse_binary_expression(ExpressionNode* left, int min_precedence);
        ParseResult<ExpressionNode> parse_expression_piece();
        
        // Primary expressions
        ParseResult<ExpressionNode> parse_primary_expression();
        ParseResult<ExpressionNode> parse_literal();
        ParseResult<ExpressionNode> parse_identifier_expression();
        ParseResult<ExpressionNode> parse_this_expression();
        ParseResult<ExpressionNode> parse_parenthesized_expression();
        ParseResult<ExpressionNode> parse_new_expression();
        ParseResult<ExpressionNode> parse_match_expression();
        ParseResult<ExpressionNode> parse_lambda_expression();
        
        // Prefix expressions
        ParseResult<ExpressionNode> parse_prefix_expression();
        ParseResult<ExpressionNode> parse_unary_expression();
        
        // Postfix expressions
        ParseResult<ExpressionNode> parse_postfix_expression(ExpressionNode* expr);
        ParseResult<ExpressionNode> parse_call_suffix(ExpressionNode* target);
        ParseResult<ExpressionNode> parse_member_access_suffix(ExpressionNode* target);
        ParseResult<ExpressionNode> parse_index_suffix(ExpressionNode* target);
        
        // Special expressions
        ParseResult<ExpressionNode> parse_range_suffix(ExpressionNode* start, const Token& op);
        ParseResult<ExpressionNode> parse_assignment_expression(ExpressionNode* left);
        
        // ================== Match Patterns ==================
        ParseResult<MatchArmNode> parse_match_arm();
        ParseResult<MatchPatternNode> parse_match_pattern();
        
        // ================== Identifiers ==================
        ParseResult<IdentifierNode> parse_identifier();
        
        // ================== Operator Tables ==================
        struct OpInfo {
            int precedence;
            bool right_assoc = false;
            bool is_range = false;
            bool is_assignment = false;
        };
        
        static const std::map<TokenKind, OpInfo> operator_table;
        const OpInfo* get_operator_info(TokenKind kind) const;
        
        // ================== Context Tracking ==================
        struct Context {
            enum Type { TOP_LEVEL, FUNCTION, LOOP, TYPE_BODY, NAMESPACE, PROPERTY_GETTER, PROPERTY_SETTER };
            Type type;
            size_t start_position;
        };
        
        std::vector<Context> context_stack;
        
        template<typename F>
        auto with_context(Context::Type type, F&& func);
        
        bool in_loop() const;
        bool in_function() const;
        bool in_property_accessor() const;
        
        // ================== Utility Functions ==================
        bool is_expression_terminator() const;
        bool is_statement_terminator() const;
        bool is_modifier_keyword(TokenKind kind) const;
    };
}