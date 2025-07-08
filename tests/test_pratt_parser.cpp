#include "test/test_framework.hpp"
#include "test/test_helpers.hpp"
#include "test/parser_test_helpers.hpp"
#include "parser/pratt_parser.hpp"
#include "parser/token_stream.hpp"
#include "parser/lexer.hpp"
#include "parser/parser_context.hpp"
#include "ast/ast_allocator.hpp"
#include "ast/ast_printer.hpp"
#include <set>

using namespace Mycelium::Testing;
using namespace Mycelium::Scripting::Parser;
using namespace Mycelium::Scripting::Lang;

// Test environment that holds all required objects
struct PrattParserTestEnv {
    std::unique_ptr<Lexer> lexer;
    std::unique_ptr<TokenStream> token_stream;
    std::unique_ptr<ParserContext> context;
    std::unique_ptr<AstAllocator> allocator;
    std::unique_ptr<PrattParser> parser;
    
    PrattParserTestEnv(std::string_view source) {
        lexer = std::make_unique<Lexer>(source);
        auto token_stream_value = lexer->tokenize_all();
        token_stream = std::make_unique<TokenStream>(std::move(token_stream_value));
        context = std::make_unique<ParserContext>(source);
        allocator = std::make_unique<AstAllocator>();
        parser = std::make_unique<PrattParser>(*token_stream, *context, *allocator);
        parser->enable_debug(true);
    }
};

// Test basic literal parsing
TestResult test_literal_parsing() {
    PrattParserTestEnv env("42");
    
    auto expr = env.parser->parse_expression();
    ASSERT_AST_NOT_NULL(expr, expr, "Should parse integer literal");
    auto literal ASSERT_NODE_TYPE(expr, LiteralExpressionNode, expr, "Should be literal expression");
    ASSERT_AST_EQ(LiteralKind::Integer, literal->kind, expr, "Should be integer literal");
    
    return TestResult(true);
}

// Test identifier parsing
TestResult test_identifier_parsing() {
    PrattParserTestEnv env("myVar");
    
    auto expr = env.parser->parse_expression();
    ASSERT_AST_NOT_NULL(expr, expr, "Should parse identifier");
    auto id_expr ASSERT_NODE_TYPE(expr, IdentifierExpressionNode, expr, "Should be identifier expression");
    ASSERT_AST_NOT_NULL(id_expr->identifier, expr, "Should have identifier");
    
    return TestResult(true);
}

// Test binary expression parsing
TestResult test_binary_expression_parsing() {
    PrattParserTestEnv env("1 + 2");
    
    auto expr = env.parser->parse_expression();
    ASSERT_AST_NOT_NULL(expr, expr, "Should parse binary expression");
    auto binary ASSERT_NODE_TYPE(expr, BinaryExpressionNode, expr, "Should be binary expression");
    ASSERT_BINARY_OP(binary, BinaryOperatorKind::Add, expr, "Should be addition");
    ASSERT_AST_NOT_NULL(binary->left, expr, "Should have left operand");
    ASSERT_AST_NOT_NULL(binary->right, expr, "Should have right operand");
    
    return TestResult(true);
}

// Test operator precedence
TestResult test_operator_precedence() {
    PrattParserTestEnv env("1 + 2 * 3");
    
    auto expr = env.parser->parse_expression();
    ASSERT_AST_NOT_NULL(expr, expr, "Should parse expression with precedence");
    auto binary ASSERT_NODE_TYPE(expr, BinaryExpressionNode, expr, "Should be binary expression");
    ASSERT_BINARY_OP(binary, BinaryOperatorKind::Add, expr, "Root should be addition");
    
    // Right side should be multiplication (higher precedence)
    auto right_binary ASSERT_NODE_TYPE(binary->right, BinaryExpressionNode, expr, "Right should be binary expression");
    ASSERT_BINARY_OP(right_binary, BinaryOperatorKind::Multiply, expr, "Right should be multiplication");
    
    return TestResult(true);
}

// Test parenthesized expressions
TestResult test_parenthesized_expressions() {
    PrattParserTestEnv env("(1 + 2) * 3");
    
    auto expr = env.parser->parse_expression();
    ASSERT_AST_NOT_NULL(expr, expr, "Should parse parenthesized expression");
    auto binary ASSERT_NODE_TYPE(expr, BinaryExpressionNode, expr, "Should be binary expression");
    ASSERT_BINARY_OP(binary, BinaryOperatorKind::Multiply, expr, "Root should be multiplication");
    
    // Left side should be parenthesized addition
    auto paren ASSERT_NODE_TYPE(binary->left, ParenthesizedExpressionNode, expr, "Left should be parenthesized");
    
    return TestResult(true);
}

// Test unary expressions
TestResult test_unary_expressions() {
    PrattParserTestEnv env("-5");
    
    auto expr = env.parser->parse_expression();
    ASSERT_AST_NOT_NULL(expr, expr, "Should parse unary expression");
    auto unary ASSERT_NODE_TYPE(expr, UnaryExpressionNode, expr, "Should be unary expression");
    ASSERT_UNARY_OP(unary, UnaryOperatorKind::Minus, expr, "Should be negation");
    ASSERT_AST_NOT_NULL(unary->operand, expr, "Should have operand");
    
    return TestResult(true);
}

// Test member access
TestResult test_member_access() {
    PrattParserTestEnv env("obj.member");
    
    auto expr = env.parser->parse_expression();
    ASSERT_AST_NOT_NULL(expr, expr, "Should parse member access");
    auto member_access ASSERT_NODE_TYPE(expr, MemberAccessExpressionNode, expr, "Should be member access");
    ASSERT_AST_NOT_NULL(member_access->target, expr, "Should have target");
    ASSERT_AST_NOT_NULL(member_access->member, expr, "Should have member");
    
    return TestResult(true);
}

// Test this expression
TestResult test_this_expression() {
    PrattParserTestEnv env("this");
    
    auto expr = env.parser->parse_expression();
    ASSERT_AST_NOT_NULL(expr, expr, "Should parse this expression");
    auto this_expr ASSERT_NODE_TYPE(expr, ThisExpressionNode, expr, "Should be this expression");
    
    return TestResult(true);
}

// Test boolean literals
TestResult test_boolean_literals() {
    {
        PrattParserTestEnv env("true");
        auto expr = env.parser->parse_expression();
        ASSERT_AST_NOT_NULL(expr, expr, "Should parse true literal");
        auto literal ASSERT_NODE_TYPE(expr, LiteralExpressionNode, expr, "Should be literal expression");
        ASSERT_AST_EQ(LiteralKind::Boolean, literal->kind, expr, "Should be boolean literal");
    }
    
    {
        PrattParserTestEnv env("false");
        auto expr = env.parser->parse_expression();
        ASSERT_AST_NOT_NULL(expr, expr, "Should parse false literal");
        auto literal ASSERT_NODE_TYPE(expr, LiteralExpressionNode, expr, "Should be literal expression");
        ASSERT_AST_EQ(LiteralKind::Boolean, literal->kind, expr, "Should be boolean literal");
    }
    
    return TestResult(true);
}

// Test complex expression with multiple operators
TestResult test_complex_expression_pratt() {
    PrattParserTestEnv env("a + b * c - d");
    
    auto expr = env.parser->parse_expression();
    ASSERT_AST_NOT_NULL(expr, expr, "Should parse complex expression");
    // Should be parsed as: (a + (b * c)) - d
    auto binary ASSERT_NODE_TYPE(expr, BinaryExpressionNode, expr, "Should be binary expression");
    ASSERT_BINARY_OP(binary, BinaryOperatorKind::Subtract, expr, "Root should be subtraction");
    
    return TestResult(true);
}

// Test combination of multiple Pratt parser features
TestResult test_pratt_combinations() {
    // Test 1: Mixed precedence with unary and binary operators
    {
        PrattParserTestEnv env("-x + y * !z");
        auto expr = env.parser->parse_expression();
        
        ASSERT_AST_NOT_NULL(expr, expr, "Should parse mixed precedence expression");
        auto binary ASSERT_NODE_TYPE(expr, BinaryExpressionNode, expr, "Root should be binary expression");
        ASSERT_BINARY_OP(binary, BinaryOperatorKind::Add, expr, "Root operator should be +");
        
        // Left side: -x
        auto unary_left ASSERT_NODE_TYPE(binary->left, UnaryExpressionNode, expr, "Left should be unary -x");
        ASSERT_UNARY_OP(unary_left, UnaryOperatorKind::Minus, expr, "Should be unary minus");
        
        // Right side: y * !z
        auto binary_right ASSERT_NODE_TYPE(binary->right, BinaryExpressionNode, expr, "Right should be binary y * !z");
        ASSERT_BINARY_OP(binary_right, BinaryOperatorKind::Multiply, expr, "Right operator should be *");
        
        // Right side of multiplication: !z
        auto unary_right ASSERT_NODE_TYPE(binary_right->right, UnaryExpressionNode, expr, "Right of * should be unary !z");
        ASSERT_UNARY_OP(unary_right, UnaryOperatorKind::Not, expr, "Should be logical not");
    }
    
    // Test 2: Simple member access chains (avoiding method calls for now)
    {
        PrattParserTestEnv env("obj.field1.field2");
        auto expr = env.parser->parse_expression();
        
        ASSERT_AST_NOT_NULL(expr, expr, "Should parse chained member access");
        auto member ASSERT_NODE_TYPE(expr, MemberAccessExpressionNode, expr, "Root should be member access");
        ASSERT_IDENTIFIER_NAME(member->member, "field2", expr, "Final member should be field2");
        
        // Target should be another member access
        auto inner_member ASSERT_NODE_TYPE(member->target, MemberAccessExpressionNode, expr, "Target should be member access");
        ASSERT_IDENTIFIER_NAME(inner_member->member, "field1", expr, "Inner member should be field1");
    }
    
    // Test 3: Complex boolean expressions with short-circuit operators
    {
        PrattParserTestEnv env("a && b || c && d || e");
        auto expr = env.parser->parse_expression();
        
        ASSERT_AST_NOT_NULL(expr, expr, "Should parse boolean expression");
        // Should be right-associative: (a && b) || ((c && d) || e)
        auto root ASSERT_NODE_TYPE(expr, BinaryExpressionNode, expr, "Root should be binary expression");
        ASSERT_BINARY_OP(root, BinaryOperatorKind::LogicalOr, expr, "Root should be ||");
        
        // Left side: a && b
        auto left ASSERT_NODE_TYPE(root->left, BinaryExpressionNode, root, "Left should be a && b");
        ASSERT_BINARY_OP(left, BinaryOperatorKind::LogicalAnd, root, "Left operator should be &&");
    }
    
    // Test 4: Assignment chains with different operators
    {
        PrattParserTestEnv env("a = b += c *= d");
        auto expr = env.parser->parse_expression();
        
        ASSERT_AST_NOT_NULL(expr, expr, "Should parse assignment chain");
        auto assign1 ASSERT_NODE_TYPE(expr, AssignmentExpressionNode, expr, "Root should be assignment");
        ASSERT_ASSIGNMENT_OP(assign1, AssignmentOperatorKind::Assign, expr, "Root should be =");
        
        // Source should be another assignment
        auto assign2 ASSERT_NODE_TYPE(assign1->source, AssignmentExpressionNode, expr, "Source should be assignment");
        ASSERT_ASSIGNMENT_OP(assign2, AssignmentOperatorKind::Add, expr, "Second should be +=");
        
        // Its source should be another assignment
        auto assign3 ASSERT_NODE_TYPE(assign2->source, AssignmentExpressionNode, expr, "Third should be assignment");
        ASSERT_ASSIGNMENT_OP(assign3, AssignmentOperatorKind::Multiply, expr, "Third should be *=");
    }
    
    // Test 5: Postfix operators with member access
    {
        PrattParserTestEnv env("arr[i++].field--");
        auto expr = env.parser->parse_expression();
        
        ASSERT_AST_NOT_NULL(expr, expr, "Should parse postfix with member access");
        auto postfix ASSERT_NODE_TYPE(expr, UnaryExpressionNode, expr, "Root should be unary (postfix)");
        ASSERT_UNARY_OP(postfix, UnaryOperatorKind::PostDecrement, expr, "Should be post-decrement");
        ASSERT_AST_TRUE(postfix->isPostfix, expr, "Should be marked as postfix");
        
        // Operand should be member access
        auto member ASSERT_NODE_TYPE(postfix->operand, MemberAccessExpressionNode, expr, "Operand should be member access");
        
        // Target should be indexer
        auto indexer ASSERT_NODE_TYPE(member->target, IndexerExpressionNode, expr, "Target should be indexer");
        
        // Index should be postfix increment 
        auto index_postfix ASSERT_NODE_TYPE(indexer->index, UnaryExpressionNode, expr, "Index should be unary");
        ASSERT_UNARY_OP(index_postfix, UnaryOperatorKind::PostIncrement, expr, "Should be post-increment");
    }
    
    return TestResult(true);
}

// Test all Pratt parser features combined
TestResult test_pratt_all_features() {
    // Complex expression using all features (simplified to avoid unimplemented features)
    std::string source = R"(
        this.value + x * y - z && 
        !flag || 
        a > b ? true_val : false_val
    )";
    
    PrattParserTestEnv env(source);
    auto expr = env.parser->parse_expression();
    
    ASSERT_AST_NOT_NULL(expr, expr, "Should parse complex expression");
    
    // Verify the expression parsed without errors
    // The exact structure is complex, but we can verify key nodes exist
    
    // Helper to find nodes of specific types in the tree
    class NodeFinder : public StructuralVisitor {
    public:
        std::vector<AstNode*> found_nodes;
        std::set<uint8_t> target_types;
        
        NodeFinder(std::initializer_list<uint8_t> types) : target_types(types) {}
        
        void visit(AstNode* node) override {
            if (target_types.count(node->typeId)) {
                found_nodes.push_back(node);
            }
            // Continue visiting children
            StructuralVisitor::visit(node);
        }
    };
    
    // Find specific node types
    NodeFinder finder({
        ThisExpressionNode::sTypeInfo.typeId,
        MemberAccessExpressionNode::sTypeInfo.typeId,
        UnaryExpressionNode::sTypeInfo.typeId,
        BinaryExpressionNode::sTypeInfo.typeId,
        ConditionalExpressionNode::sTypeInfo.typeId,
        IdentifierExpressionNode::sTypeInfo.typeId
    });
    
    expr->accept(&finder);
    
    // Verify we found instances of various node types
    bool has_this = false, has_member = false;
    bool has_unary = false, has_binary = false;
    bool has_conditional = false, has_identifier = false;
    
    for (auto node : finder.found_nodes) {
        if (node_is<ThisExpressionNode>(node)) has_this = true;
        if (node_is<MemberAccessExpressionNode>(node)) has_member = true;
        if (node_is<UnaryExpressionNode>(node)) has_unary = true;
        if (node_is<BinaryExpressionNode>(node)) has_binary = true;
        if (node_is<ConditionalExpressionNode>(node)) has_conditional = true;
        if (node_is<IdentifierExpressionNode>(node)) has_identifier = true;
    }
    
    ASSERT_AST_TRUE(has_this, expr, "Should have this expression");
    ASSERT_AST_TRUE(has_member, expr, "Should have member access");
    ASSERT_AST_TRUE(has_unary, expr, "Should have unary expressions");
    ASSERT_AST_TRUE(has_binary, expr, "Should have binary expressions");
    ASSERT_AST_TRUE(has_conditional, expr, "Should have conditional expression");
    ASSERT_AST_TRUE(has_identifier, expr, "Should have identifier expressions");
    
    return TestResult(true);
}

void run_pratt_parser_tests() {
    TestSuite suite("Pratt Parser Tests");
    
    suite.add_test("Literal Parsing", test_literal_parsing);
    suite.add_test("Identifier Parsing", test_identifier_parsing);
    suite.add_test("Binary Expression Parsing", test_binary_expression_parsing);
    suite.add_test("Operator Precedence", test_operator_precedence);
    suite.add_test("Parenthesized Expressions", test_parenthesized_expressions);
    suite.add_test("Unary Expressions", test_unary_expressions);
    suite.add_test("Member Access", test_member_access);
    suite.add_test("This Expression", test_this_expression);
    suite.add_test("Boolean Literals", test_boolean_literals);
    suite.add_test("Complex Expression", test_complex_expression_pratt);
    suite.add_test("Pratt Feature Combinations", test_pratt_combinations);
    suite.add_test("Pratt All Features", test_pratt_all_features);
    
    suite.run_all();
}