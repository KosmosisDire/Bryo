#pragma once

#include "parser/parser_base.hpp"
#include "parser/pratt_parser.hpp"
#include "parser/parse_result.hpp"
#include "ast/ast.hpp"

namespace Mycelium::Scripting::Parser {

using namespace Mycelium::Scripting::Lang;

// Forward declaration for cross-referencing
class PrattParser;

// Recursive descent parser for statements and declarations
class RecursiveParser : public ParserBase {
public:
    // Constructor
    RecursiveParser(TokenStream& tokens, ParserContext& context, AstAllocator& allocator);
    
    // Destructor
    virtual ~RecursiveParser() = default;
    
    // Non-copyable but movable
    RecursiveParser(const RecursiveParser&) = delete;
    RecursiveParser& operator=(const RecursiveParser&) = delete;
    RecursiveParser(RecursiveParser&&) = default;
    RecursiveParser& operator=(RecursiveParser&&) = default;
    
    // Set expression parser for delegation
    void set_expression_parser(std::shared_ptr<PrattParser> expr_parser) {
        expression_parser_ = expr_parser;
    }
    
    // Main parsing entry points
    ParseResult<CompilationUnitNode*> parse_compilation_unit();
    ParseResult<StatementNode*> parse_statement();
    ParseResult<DeclarationNode*> parse_declaration();
    
    // Declaration parsing methods
    ParseResult<FunctionDeclarationNode*> parse_function_declaration();
    ParseResult<TypeDeclarationNode*> parse_type_declaration();
    ParseResult<InterfaceDeclarationNode*> parse_interface_declaration();
    ParseResult<EnumDeclarationNode*> parse_enum_declaration();
    ParseResult<FieldDeclarationNode*> parse_field_declaration();
    ParseResult<PropertyDeclarationNode*> parse_property_declaration();
    ParseResult<ConstructorDeclarationNode*> parse_constructor_declaration();
    ParseResult<UsingDirectiveNode*> parse_using_directive();
    ParseResult<NamespaceDeclarationNode*> parse_namespace_declaration();
    
    // Statement parsing methods
    ParseResult<BlockStatementNode*> parse_block_statement();
    ParseResult<IfStatementNode*> parse_if_statement();
    ParseResult<WhileStatementNode*> parse_while_statement();
    ParseResult<ForStatementNode*> parse_for_statement();
    ParseResult<ReturnStatementNode*> parse_return_statement();
    ParseResult<BreakStatementNode*> parse_break_statement();
    ParseResult<ContinueStatementNode*> parse_continue_statement();
    ParseResult<ExpressionStatementNode*> parse_expression_statement();
    ParseResult<LocalVariableDeclarationNode*> parse_local_variable_declaration();
    
    // Expression parsing delegation
    ParseResult<ExpressionNode*> parse_expression(int min_precedence = 0);
    ParseResult<WhenExpressionNode*> parse_when_expression();
    
    // Specialized parsing methods
    ParseResult<ParameterNode*> parse_parameter();
    ParseResult<EnumCaseNode*> parse_enum_case();
    ParseResult<PropertyAccessorNode*> parse_property_accessor();
    ParseResult<WhenArmNode*> parse_when_arm();
    ParseResult<WhenPatternNode*> parse_when_pattern();
    
    // Type parsing methods
    ParseResult<TypeNameNode*> parse_type_name_extended();
    ParseResult<ArrayTypeNameNode*> parse_array_type(TypeNameNode* element_type);
    ParseResult<PointerTypeNameNode*> parse_pointer_type(TypeNameNode* element_type);
    
    // Utility methods for complex parsing scenarios
    bool is_variable_declaration_start() const;
    bool is_function_declaration_start() const;
    bool is_type_declaration_start() const;
    bool is_property_declaration_start() const;
    
private:
    // Expression parser for delegation
    std::shared_ptr<PrattParser> expression_parser_;
    
    // Helper methods for parsing common constructs
    ParseResult<std::vector<ModifierKind>> parse_modifiers_result();
    ParseResult<std::vector<ParameterNode*>> parse_parameter_list_result();
    ParseResult<std::vector<ParameterNode*>> parse_parameter_list_only();
    ParseResult<std::vector<ExpressionNode*>> parse_argument_list_result();
    ParseResult<std::vector<IdentifierNode*>> parse_identifier_list_result();
    
    // Specialized list parsing
    ParseResult<std::vector<EnumCaseNode*>> parse_enum_case_list();
    ParseResult<std::vector<MemberDeclarationNode*>> parse_member_declaration_list();
    ParseResult<std::vector<StatementNode*>> parse_statement_list();
    ParseResult<std::vector<WhenArmNode*>> parse_when_arm_list();
    
    // Error recovery helpers
    void synchronize_to_declaration();
    void synchronize_to_statement();
    void synchronize_to_block_end();
    
    // Validation helpers
    bool validate_modifier_context(const std::vector<ModifierKind>& modifiers, 
                                  ParsingContext context) const;
    bool validate_declaration_context(TokenKind declaration_kind) const;
    
    // Type inference helpers  
    TypeNameNode* infer_type_from_initializer(ExpressionNode* initializer);
    bool can_infer_type() const;
    
    // Member parsing dispatch
    ParseResult<MemberDeclarationNode*> parse_member_declaration();
    
    // Pattern parsing for match expressions
    ParseResult<EnumPatternNode*> parse_enum_pattern();
    ParseResult<RangePatternNode*> parse_range_pattern();
    ParseResult<LiteralPatternNode*> parse_literal_pattern();
    ParseResult<WildcardPatternNode*> parse_wildcard_pattern();
    ParseResult<ComparisonPatternNode*> parse_comparison_pattern();
    
    // Context-sensitive parsing helpers
    bool is_in_type_context() const;
    bool is_in_enum_context() const;
    bool is_in_function_context() const;
    bool allows_local_functions() const;
    
    // Advanced error recovery
    TokenKind find_next_synchronization_point() const;
    std::vector<TokenKind> get_statement_sync_tokens() const;
    std::vector<TokenKind> get_declaration_sync_tokens() const;
};

// Utility functions for parser operations

// Check if current position looks like a declaration
bool looks_like_declaration(const TokenStream& tokens);

// Check if current position looks like a statement
bool looks_like_statement(const TokenStream& tokens);

// Get appropriate error message for parsing context
std::string get_context_specific_error_message(ParsingContext context, TokenKind found);

// Check if modifiers are valid in a given context
bool are_modifiers_valid_in_context(const std::vector<ModifierKind>& modifiers, 
                                   ParsingContext context);

} // namespace Mycelium::Scripting::Parser