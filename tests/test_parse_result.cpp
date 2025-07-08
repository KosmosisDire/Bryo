#include "test/test_framework.hpp"
#include "test/test_helpers.hpp"
#include "parser/parse_result.hpp"
#include "parser/parser_context.hpp"
#include "parser/parser_base.hpp"
#include "parser/token_stream.hpp"
#include "parser/lexer.hpp"
#include "ast/ast_allocator.hpp"

using namespace Mycelium::Testing;
using namespace Mycelium::Scripting::Parser;
using namespace Mycelium::Scripting::Lang;
using namespace Mycelium::Scripting;

// Test environment for ParseResult testing
struct ParseResultTestEnv {
    std::unique_ptr<Lexer> lexer;
    std::unique_ptr<TokenStream> token_stream;
    std::unique_ptr<ParserContext> context;
    std::unique_ptr<AstAllocator> allocator;
    std::unique_ptr<ParserBase> parser;
    
    ParseResultTestEnv(std::string_view source) {
        lexer = std::make_unique<Lexer>(source);
        auto token_stream_value = lexer->tokenize_all();
        token_stream = std::make_unique<TokenStream>(std::move(token_stream_value));
        context = std::make_unique<ParserContext>(source);
        allocator = std::make_unique<AstAllocator>();
        parser = std::make_unique<ParserBase>(*token_stream, *context, *allocator);
    }
};

// Test basic ParseResult creation and access
TestResult test_parse_result_basics() {
    // Test successful result
    auto success_result = ParseResult<int>::success(42);
    ASSERT_TRUE(success_result.has_value(), "Success result should have value");
    ASSERT_TRUE(success_result.is_success(), "Should be marked as success");
    ASSERT_FALSE(success_result.has_errors(), "Success shouldn't have errors");
    ASSERT_EQ(42, success_result.value(), "Should return correct value");
    
    // Test error result
    ParserDiagnostic error(DiagnosticLevel::Error, "Test error", SourceLocation(0, 1, 1));
    auto error_result = ParseResult<int>::error(std::move(error));
    ASSERT_FALSE(error_result.has_value(), "Error result shouldn't have value");
    ASSERT_TRUE(error_result.is_failure(), "Should be marked as failure");
    ASSERT_TRUE(error_result.has_errors(), "Error result should have errors");
    ASSERT_EQ(1, error_result.errors().size(), "Should have one error");
    
    return TestResult(true);
}

// Test ParseResult composition with simple operations
TestResult test_parse_result_composition() {
    // Test simple success case
    auto initial = ParseResult<int>::success(5);
    ASSERT_TRUE(initial.has_value(), "Initial should have value");
    ASSERT_EQ(5, initial.value(), "Should have correct initial value");
    
    // Test error case
    ParserDiagnostic error(DiagnosticLevel::Error, "Initial error", SourceLocation(0, 1, 1));
    auto error_initial = ParseResult<int>::error(std::move(error));
    
    ASSERT_FALSE(error_initial.has_value(), "Error result shouldn't have value");
    ASSERT_TRUE(error_initial.has_errors(), "Should have errors");
    
    return TestResult(true);
}

// Test ParseResult error recovery
TestResult test_parse_result_recovery() {
    ParserDiagnostic error(DiagnosticLevel::Error, "Original error", SourceLocation(0, 1, 1));
    auto failed_result = ParseResult<int>::error(std::move(error));
    
    auto recovered = failed_result.or_else([]() {
        return ParseResult<int>::success(99);
    });
    
    ASSERT_TRUE(recovered.has_value(), "Should have recovered value");
    ASSERT_EQ(99, recovered.value(), "Should return recovery value");
    ASSERT_TRUE(recovered.is_recovered(), "Should be marked as recovered");
    ASSERT_TRUE(recovered.has_errors(), "Should preserve original errors");
    
    return TestResult(true);
}

// Test successful token consumption
TestResult test_consume_token_success() {
    ParseResultTestEnv env("identifier");
    
    auto result = env.parser->consume_token(TokenKind::Identifier);
    
    ASSERT_TRUE(result.has_value(), "Should successfully consume identifier");
    ASSERT_FALSE(result.has_errors(), "Successful consumption shouldn't have errors");
    ASSERT_TRUE(result.value().kind == TokenKind::Identifier, "Should return identifier token");
    
    return TestResult(true);
}

// Test failed token consumption
TestResult test_consume_token_failure() {
    ParseResultTestEnv env("identifier");
    
    auto result = env.parser->consume_token(TokenKind::LeftParen);
    
    ASSERT_FALSE(result.has_value(), "Should fail to consume wrong token");
    ASSERT_TRUE(result.has_errors(), "Failed consumption should have errors");
    ASSERT_EQ(1, result.errors().size(), "Should have one error");
    
    return TestResult(true);
}

// Test consume_any_token success
TestResult test_consume_any_token_success() {
    ParseResultTestEnv env("+");
    
    auto result = env.parser->consume_any_token({TokenKind::Plus, TokenKind::Minus, TokenKind::Asterisk});
    
    ASSERT_TRUE(result.has_value(), "Should successfully consume one of the options");
    ASSERT_TRUE(result.value().kind == TokenKind::Plus, "Should return plus token");
    
    return TestResult(true);
}

// Test consume_any_token failure
TestResult test_consume_any_token_failure() {
    ParseResultTestEnv env("identifier");
    
    auto result = env.parser->consume_any_token({TokenKind::Plus, TokenKind::Minus, TokenKind::Asterisk});
    
    ASSERT_FALSE(result.has_value(), "Should fail when token doesn't match any option");
    ASSERT_TRUE(result.has_errors(), "Should have error message");
    
    return TestResult(true);
}

// Test identifier parsing with ParseResult
TestResult test_parse_identifier_result() {
    ParseResultTestEnv env("myVariable");
    
    auto result = env.parser->parse_identifier_result();
    
    ASSERT_TRUE(result.has_value(), "Should successfully parse identifier");
    ASSERT_FALSE(result.has_errors(), "Successful parsing shouldn't have errors");
    
    auto identifier = result.value();
    ASSERT_TRUE(identifier != nullptr, "Should return valid identifier node");
    ASSERT_TRUE(node_is<IdentifierNode>(identifier), "Should be IdentifierNode");
    
    return TestResult(true);
}

// Test identifier parsing failure
TestResult test_parse_identifier_failure() {
    ParseResultTestEnv env("123invalid");
    
    auto result = env.parser->parse_identifier_result();
    
    ASSERT_FALSE(result.has_value(), "Should fail to parse invalid identifier");
    ASSERT_TRUE(result.has_errors(), "Should have error messages");
    
    return TestResult(true);
}

// Test type name parsing 
TestResult test_parse_type_name_result() {
    ParseResultTestEnv env("String");
    
    auto result = env.parser->parse_type_name_result();
    
    ASSERT_TRUE(result.has_value(), "Should successfully parse type name");
    
    auto type_name = result.value();
    ASSERT_TRUE(type_name != nullptr, "Should return valid type name node");
    ASSERT_TRUE(node_is<TypeNameNode>(type_name), "Should be TypeNameNode");
    ASSERT_TRUE(type_name->identifier != nullptr, "Should have identifier");
    
    return TestResult(true);
}

// Test qualified type name parsing
TestResult test_parse_qualified_type_name_result() {
    ParseResultTestEnv env("System::Collections::List");
    
    auto result = env.parser->parse_qualified_type_name_result();
    
    ASSERT_TRUE(result.has_value(), "Should successfully parse qualified type name");
    
    auto qualified_type = result.value();
    ASSERT_TRUE(qualified_type != nullptr, "Should return valid qualified type node");
    ASSERT_TRUE(node_is<QualifiedTypeNameNode>(qualified_type), "Should be QualifiedTypeNameNode");
    
    return TestResult(true);
}

void run_parse_result_tests() {
    TestSuite suite("ParseResult Tests");
    
    suite.add_test("ParseResult Basics", test_parse_result_basics);
    suite.add_test("ParseResult Composition", test_parse_result_composition);
    suite.add_test("ParseResult Recovery", test_parse_result_recovery);
    suite.add_test("Consume Token Success", test_consume_token_success);
    suite.add_test("Consume Token Failure", test_consume_token_failure);
    suite.add_test("Consume Any Token Success", test_consume_any_token_success);
    suite.add_test("Consume Any Token Failure", test_consume_any_token_failure);
    suite.add_test("Parse Identifier Result", test_parse_identifier_result);
    suite.add_test("Parse Identifier Failure", test_parse_identifier_failure);
    suite.add_test("Parse Type Name Result", test_parse_type_name_result);
    suite.add_test("Parse Qualified Type Name Result", test_parse_qualified_type_name_result);
    
    suite.run_all();
}