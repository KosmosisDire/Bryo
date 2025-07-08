#include "test/test_framework.hpp"
#include "test/parser_test_helpers.hpp"
#include "parser/lexer.hpp"
#include "parser/token_stream.hpp"

using namespace Mycelium::Testing;
using namespace Mycelium::Scripting::Parser;
using namespace Mycelium::Scripting;

// Test diagnostic sink for collecting errors
class TestLexerDiagnosticSink : public LexerDiagnosticSink {
public:
    std::vector<LexerDiagnostic> diagnostics;
    
    void report_diagnostic(const LexerDiagnostic& diagnostic) override {
        diagnostics.push_back(diagnostic);
    }
    
    void clear() { diagnostics.clear(); }
    
    bool has_errors() const {
        for (const auto& diag : diagnostics) {
            if (diag.is_error) return true;
        }
        return false;
    }
};

TestResult test_basic_tokenization() {
    std::string source = "x + 42";
    TestLexerDiagnosticSink sink;
    Lexer lexer(source, {}, &sink);
    
    // Get all tokens
    TokenStream stream = lexer.tokenize_all();
    
    std::vector<TokenKind> expected = {
        TokenKind::Identifier,
        TokenKind::Plus,
        TokenKind::IntegerLiteral,
        TokenKind::EndOfFile
    };
    
    ASSERT_TOKEN_SEQUENCE(stream, expected, "Basic tokenization should match expected sequence");
    
    // Verify specific token text
    ASSERT_TOKEN_TEXT(stream[0].text, "x", 0, "First token text should be 'x'");
    ASSERT_TOKEN_TEXT(stream[1].text, "+", 1, "Second token text should be '+'");
    ASSERT_TOKEN_TEXT(stream[2].text, "42", 2, "Third token text should be '42'");
    
    ASSERT_FALSE(sink.has_errors(), "Should not have lexical errors");
    
    return TestResult(true);
}

TestResult test_keywords() {
    std::string source = "fn type if else true false";
    TestLexerDiagnosticSink sink;
    Lexer lexer(source, {}, &sink);
    TokenStream stream = lexer.tokenize_all();
    
    std::vector<TokenKind> expected = {
        TokenKind::Fn,
        TokenKind::Type,
        TokenKind::If,
        TokenKind::Else,
        TokenKind::BooleanLiteral,
        TokenKind::BooleanLiteral,
        TokenKind::EndOfFile
    };
    
    ASSERT_TOKEN_SEQUENCE(stream, expected, "Keyword tokens should match expected sequence");
    
    ASSERT_FALSE(sink.has_errors(), "Should not have lexical errors");
    
    return TestResult(true);
}

TestResult test_operators() {
    std::string source = "++ += == != <= >= && || -> :: ..=";
    TestLexerDiagnosticSink sink;
    Lexer lexer(source, {}, &sink);
    TokenStream stream = lexer.tokenize_all();
    
    std::vector<TokenKind> expected = {
        TokenKind::Increment,
        TokenKind::PlusAssign,
        TokenKind::Equal,
        TokenKind::NotEqual,
        TokenKind::LessEqual,
        TokenKind::GreaterEqual,
        TokenKind::And,
        TokenKind::Or,
        TokenKind::Arrow,
        TokenKind::DoubleColon,
        TokenKind::DotDotEquals,
        TokenKind::EndOfFile
    };
    
    ASSERT_TOKEN_SEQUENCE(stream, expected, "Operator tokens should match expected sequence");
    
    ASSERT_FALSE(sink.has_errors(), "Should not have lexical errors");
    
    return TestResult(true);
}

TestResult test_string_literals() {
    std::string source = R"("hello world" "with\nescapes")";
    TestLexerDiagnosticSink sink;
    Lexer lexer(source, {}, &sink);
    TokenStream stream = lexer.tokenize_all();
    
    std::vector<TokenKind> expected = {
        TokenKind::StringLiteral,
        TokenKind::StringLiteral,
        TokenKind::EndOfFile
    };
    
    ASSERT_TOKEN_SEQUENCE(stream, expected, "String literal tokens should match expected sequence");
    
    // Verify specific token text
    ASSERT_TOKEN_TEXT(stream[0].text, R"("hello world")", 0, "First string should match");
    ASSERT_TOKEN_TEXT(stream[1].text, R"("with\nescapes")", 1, "Second string should match");
    
    ASSERT_FALSE(sink.has_errors(), "Should not have lexical errors for valid strings");
    
    return TestResult(true);
}

TestResult test_number_literals() {
    std::string source = "42 3.14 0x1F 0b1010";
    TestLexerDiagnosticSink sink;
    Lexer lexer(source, {}, &sink);
    TokenStream stream = lexer.tokenize_all();
    
    std::vector<TokenKind> expected = {
        TokenKind::IntegerLiteral,
        TokenKind::FloatLiteral,
        TokenKind::IntegerLiteral,
        TokenKind::IntegerLiteral,
        TokenKind::EndOfFile
    };
    
    ASSERT_TOKEN_SEQUENCE(stream, expected, "Number literal tokens should match expected sequence");
    
    // Verify specific token text
    ASSERT_TOKEN_TEXT(stream[0].text, "42", 0, "Integer should match");
    ASSERT_TOKEN_TEXT(stream[1].text, "3.14", 1, "Float should match");
    ASSERT_TOKEN_TEXT(stream[2].text, "0x1F", 2, "Hex should match");
    ASSERT_TOKEN_TEXT(stream[3].text, "0b1010", 3, "Binary should match");
    
    ASSERT_FALSE(sink.has_errors(), "Should not have lexical errors");
    
    return TestResult(true);
}

TestResult test_position_tracking() {
    std::string source = "line1\nline2\n  token";
    TestLexerDiagnosticSink sink;
    Lexer lexer(source, {}, &sink);
    
    // Get all tokens
    TokenStream stream = lexer.tokenize_all();
    
    std::vector<TokenKind> expected = {
        TokenKind::Identifier,
        TokenKind::Identifier,
        TokenKind::Identifier,
        TokenKind::EndOfFile
    };
    
    ASSERT_TOKEN_SEQUENCE(stream, expected, "Position tracking tokens should match expected sequence");
    
    // Verify position information
    ASSERT_EQ(1, stream[0].location.line, "First token should be on line 1");
    ASSERT_EQ(1, stream[0].location.column, "First token should be at column 1");
    
    ASSERT_EQ(2, stream[1].location.line, "Second token should be on line 2");
    ASSERT_EQ(1, stream[1].location.column, "Second token should be at column 1");
    
    ASSERT_EQ(3, stream[2].location.line, "Third token should be on line 3");
    ASSERT_EQ(3, stream[2].location.column, "Third token should be at column 3 (after 2 spaces)");
    
    ASSERT_FALSE(sink.has_errors(), "Should not have lexical errors");
    
    return TestResult(true);
}

TestResult test_peek_operations() {
    std::string source = "a b c";
    TestLexerDiagnosticSink sink;
    Lexer lexer(source, {}, &sink);
    
    // Get all tokens
    TokenStream stream = lexer.tokenize_all();
    
    std::vector<TokenKind> expected = {
        TokenKind::Identifier,
        TokenKind::Identifier,
        TokenKind::Identifier,
        TokenKind::EndOfFile
    };
    
    ASSERT_TOKEN_SEQUENCE(stream, expected, "Peek operation tokens should match expected sequence");
    
    // Verify specific token text
    ASSERT_TOKEN_TEXT(stream[0].text, "a", 0, "Token 0 should be 'a'");
    ASSERT_TOKEN_TEXT(stream[1].text, "b", 1, "Token 1 should be 'b'");
    ASSERT_TOKEN_TEXT(stream[2].text, "c", 2, "Token 2 should be 'c'");
    
    // Test peek method on TokenStream
    const Token& peek1 = stream.peek(1);
    ASSERT_TRUE(peek1.kind == TokenKind::Identifier, "Peek 1 should be identifier");
    ASSERT_STR_EQ("b", std::string(peek1.text), "Peek 1 should be 'b'");
    
    const Token& peek2 = stream.peek(2);
    ASSERT_TRUE(peek2.kind == TokenKind::Identifier, "Peek 2 should be identifier");
    ASSERT_STR_EQ("c", std::string(peek2.text), "Peek 2 should be 'c'");
    
    ASSERT_FALSE(sink.has_errors(), "Should not have lexical errors");
    
    return TestResult(true);
}

TestResult test_token_stream() {
    std::string source = "x + 42";
    Lexer lexer(source);
    auto token_stream_value = lexer.tokenize_all();
    auto stream = std::make_unique<TokenStream>(std::move(token_stream_value));
    
    // Test current token
    ASSERT_TRUE(stream->current().kind == TokenKind::Identifier, "Current should be identifier");
    
    // Test match operation
    ASSERT_TRUE(stream->match(TokenKind::Identifier), "Should match identifier");
    ASSERT_TRUE(stream->current().kind == TokenKind::Plus, "Current should now be plus");
    
    // Test peek
    const Token& next = stream->peek(1);
    ASSERT_TRUE(next.kind == TokenKind::IntegerLiteral, "Peek should show integer literal");
    
    // Test consume
    Token plus_token = stream->consume(TokenKind::Plus);
    ASSERT_TRUE(plus_token.kind == TokenKind::Plus, "Consumed token should be plus");
    ASSERT_TRUE(stream->current().kind == TokenKind::IntegerLiteral, "Current should now be integer");
    
    return TestResult(true);
}

TestResult test_complex_expression() {
    std::string source = "fn calculate(x: i32) -> i32 { return x + 42; }";
    TestLexerDiagnosticSink sink;
    Lexer lexer(source, {}, &sink);
    
    // Get all tokens
    TokenStream stream = lexer.tokenize_all();
    
    std::vector<TokenKind> expected = {
        TokenKind::Fn,
        TokenKind::Identifier,      // calculate
        TokenKind::LeftParen,
        TokenKind::Identifier,      // x
        TokenKind::Colon,
        TokenKind::Identifier,      // i32
        TokenKind::RightParen,
        TokenKind::Arrow,
        TokenKind::Identifier,      // i32
        TokenKind::LeftBrace,
        TokenKind::Return,
        TokenKind::Identifier,      // x
        TokenKind::Plus,
        TokenKind::IntegerLiteral,  // 42
        TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::EndOfFile
    };
    
    ASSERT_EQ(expected.size(), stream.size(), "Should have correct number of tokens");
    
    ASSERT_TOKEN_SEQUENCE(stream, expected, "Complex expression tokens should match expected sequence");
    
    ASSERT_FALSE(sink.has_errors(), "Should not have lexical errors");
    
    return TestResult(true);
}

// Test combination of multiple lexer features
TestResult test_lexer_combinations() {
    // Test 1: Keywords + operators + identifiers in one expression
    {
        std::string source = "if (x == 42 && y != null) { return true; }";
        TestLexerDiagnosticSink sink;
        Lexer lexer(source, {}, &sink);
        
        // Get all tokens
        TokenStream stream = lexer.tokenize_all();
        
        // Verify proper tokenization of mixed content
        std::vector<TokenKind> expected_mixed = {
            TokenKind::If, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Equal,
            TokenKind::IntegerLiteral, TokenKind::And, TokenKind::Identifier, TokenKind::NotEqual,
            TokenKind::Identifier, TokenKind::RightParen, TokenKind::LeftBrace, TokenKind::Return,
            TokenKind::BooleanLiteral, TokenKind::Semicolon, TokenKind::RightBrace, TokenKind::EndOfFile
        };
        
        ASSERT_TOKEN_SEQUENCE(stream, expected_mixed, "Mixed content tokens should match expected sequence");
    }
    
    // Test 2: String literals with escape sequences + numbers + operators
    {
        std::string source = R"(name = "Hello\nWorld" + str(123.45))";
        TestLexerDiagnosticSink sink;
        Lexer lexer(source, {}, &sink);
        
        // Get all tokens
        TokenStream stream = lexer.tokenize_all();
        
        std::vector<TokenKind> expected_string = {
            TokenKind::Identifier, TokenKind::Assign, TokenKind::StringLiteral, TokenKind::Plus,
            TokenKind::Identifier, TokenKind::LeftParen, TokenKind::FloatLiteral, TokenKind::RightParen,
            TokenKind::EndOfFile
        };
        
        ASSERT_TOKEN_SEQUENCE(stream, expected_string, "String with escapes tokens should match expected sequence");
        
        // Verify specific token text
        ASSERT_TOKEN_TEXT(stream[2].text, "\"Hello\\nWorld\"", 2, "String should include quotes and escapes");
        ASSERT_TOKEN_TEXT(stream[6].text, "123.45", 6, "Float value should be correct");
    }
    
    // Test 3: Comments + trivia + multiline code
    {
        std::string source = R"(// This is a comment
fn test() {
    /* Block comment
       spanning multiple lines */
    x = 42; // inline comment
})";
        LexerOptions options;
        options.preserve_trivia = true;
        TestLexerDiagnosticSink sink;
        Lexer lexer(source, options, &sink);
        
        // Get all tokens
        TokenStream stream = lexer.tokenize_all();
        
        Token fn_token = stream[0];
        ASSERT_TRUE(fn_token.kind == TokenKind::Fn, "Should tokenize 'fn'");
        ASSERT_TRUE(fn_token.leading_trivia.size() >= 1, "Should have leading trivia (comment + newline)");
        ASSERT_TRUE(fn_token.leading_trivia[0].kind == TriviaKind::LineComment, "First trivia should be line comment");
        
        // Find the 'x' token (after fn, test, (, ), {)
        Token x_token = stream[5];
        ASSERT_TRUE(x_token.kind == TokenKind::Identifier, "Should tokenize 'x'");
        ASSERT_TRUE(x_token.leading_trivia.size() >= 1, "Should have block comment in leading trivia");
        
        // Verify position tracking across lines
        ASSERT_TRUE(x_token.location.line > 1, "Should be on line > 1");
    }
    
    // Test 4: All operator types in complex expression
    {
        std::string source = "a += b * c << 2 & d | e && f >= g ? h : i++";
        TestLexerDiagnosticSink sink;
        Lexer lexer(source, {}, &sink);
        
        // Get all tokens
        TokenStream stream = lexer.tokenize_all();
        
        std::vector<TokenKind> expected_operators = {
            TokenKind::Identifier, TokenKind::PlusAssign, TokenKind::Identifier, TokenKind::Asterisk,
            TokenKind::Identifier, TokenKind::LeftShift, TokenKind::IntegerLiteral, TokenKind::BitwiseAnd,
            TokenKind::Identifier, TokenKind::BitwiseOr, TokenKind::Identifier, TokenKind::And,
            TokenKind::Identifier, TokenKind::GreaterEqual, TokenKind::Identifier, TokenKind::Question,
            TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Increment
        };
        
        expected_operators.push_back(TokenKind::EndOfFile);
        ASSERT_TOKEN_SEQUENCE(stream, expected_operators, "Complex operator tokens should match expected sequence");
    }
    
    // Test 5: Error recovery with invalid characters
    {
        std::string source = "valid @ invalid # more $valid";
        TestLexerDiagnosticSink sink;
        Lexer lexer(source, {}, &sink);
        
        // Get all tokens
        TokenStream stream = lexer.tokenize_all();
        
        std::vector<TokenKind> expected_error_recovery = {
            TokenKind::Identifier, TokenKind::At, TokenKind::Identifier, TokenKind::Hash,
            TokenKind::Identifier, TokenKind::Dollar, TokenKind::Identifier
        };
        
        expected_error_recovery.push_back(TokenKind::EndOfFile);
        ASSERT_TOKEN_SEQUENCE(stream, expected_error_recovery, "Error recovery tokens should match expected sequence");
    }
    
    return TestResult(true);
}

// Test all lexer features combined in a realistic code sample
TestResult test_lexer_all_features() {
    std::string source = R"(
// Mycelium comprehensive lexer test
namespace MyApp {
    /* Multi-line comment
     * with multiple lines
     */
    type Vector3 {
        x, y, z: f32 = 0.0
        
        fn length() -> f32 {
            return x + y + z
        }
    }
    
    fn main() {
        // Test various literals
        int_val: mut i32 = 42
        float_val: f32 = 3.14159
        hex_val: i32 = 0xFF00
        str_val: string = "Hello, \"World\"!\n"
        char_val: char = 'a'
        bool_val: bool = true && false || !true
        
        // Test operators
        sum: i32 = 10 + 20 - 5 * 2 / 3 % 4
        bits: i32 = 0b1010 & 0b1100 | 0b0001 ^ ~0b1111
        shift: i32 = 1 << 4 >> 2
        cmp: bool = x < y && y <= z || z > w && w >= x || x == y || y != z
        
        // Test assignments
        sum += 1
        bits &= 0xFF
        
        // Test control flow
        if (condition) {
            while (x < 10) {
                x++
            }
        } else {
            for (i = 0; i < 10; i++) {
                break
            }
        }
        
        // Test member access and calls
        v = new Vector3()
        len: f32 = v.length()
        arr[index] = value
        
        // Test special tokens
        range: i32 = 0..10
        inclusive: i32 = 0..=10
        ref_type: ref MyType
        path: Type = Module::SubModule::Type
    }
})";
    
    LexerOptions options;
    options.preserve_trivia = true;
    TestLexerDiagnosticSink sink;
    Lexer lexer(source, options, &sink);
    
    // Get all tokens
    TokenStream stream = lexer.tokenize_all();
    
    // Define the complete expected token sequence
    std::vector<TokenKind> expected = {
        // namespace MyApp {
        TokenKind::Namespace, TokenKind::Identifier, TokenKind::LeftBrace,
        
        // type Vector3 {
        TokenKind::Type, TokenKind::Identifier, TokenKind::LeftBrace,
        
        // x, y, z: f32 = 0.0
        TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Comma, 
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::FloatLiteral,
        
        // fn length() -> f32 {
        TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, 
        TokenKind::Arrow, TokenKind::Identifier, TokenKind::LeftBrace,
        
        // return x + y + z
        TokenKind::Return, TokenKind::Identifier, TokenKind::Plus, TokenKind::Identifier,
        TokenKind::Plus, TokenKind::Identifier,
        
        // }
        TokenKind::RightBrace,
        
        // }
        TokenKind::RightBrace,
        
        // fn main() {
        TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::LeftBrace,
        
        // int_val: mut i32 = 42
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Mut, TokenKind::Identifier, TokenKind::Assign, TokenKind::IntegerLiteral,
        
        // float_val: f32 = 3.14159
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::FloatLiteral,
        
        // hex_val: i32 = 0xFF00
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::IntegerLiteral,
        
        // str_val: string = "Hello, \"World\"!\n"
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::StringLiteral,
        
        // char_val: char = 'a'
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::CharLiteral,
        
        // bool_val: bool = true && false || !true
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::BooleanLiteral,
        TokenKind::And, TokenKind::BooleanLiteral, TokenKind::Or, TokenKind::Not, TokenKind::BooleanLiteral,
        
        // sum: i32 = 10 + 20 - 5 * 2 / 3 % 4
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::IntegerLiteral,
        TokenKind::Plus, TokenKind::IntegerLiteral, TokenKind::Minus, TokenKind::IntegerLiteral,
        TokenKind::Asterisk, TokenKind::IntegerLiteral, TokenKind::Slash, TokenKind::IntegerLiteral,
        TokenKind::Percent, TokenKind::IntegerLiteral,
        
        // bits: i32 = 0b1010 & 0b1100 | 0b0001 ^ ~0b1111
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::IntegerLiteral,
        TokenKind::BitwiseAnd, TokenKind::IntegerLiteral, TokenKind::BitwiseOr, TokenKind::IntegerLiteral,
        TokenKind::BitwiseXor, TokenKind::BitwiseNot, TokenKind::IntegerLiteral,
        
        // shift: i32 = 1 << 4 >> 2
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::IntegerLiteral,
        TokenKind::LeftShift, TokenKind::IntegerLiteral, TokenKind::RightShift, TokenKind::IntegerLiteral,
        
        // cmp: bool = x < y && y <= z || z > w && w >= x || x == y || y != z
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::Identifier,
        TokenKind::Less, TokenKind::Identifier, TokenKind::And, TokenKind::Identifier,
        TokenKind::LessEqual, TokenKind::Identifier, TokenKind::Or, TokenKind::Identifier,
        TokenKind::Greater, TokenKind::Identifier, TokenKind::And, TokenKind::Identifier,
        TokenKind::GreaterEqual, TokenKind::Identifier, TokenKind::Or, TokenKind::Identifier,
        TokenKind::Equal, TokenKind::Identifier, TokenKind::Or, TokenKind::Identifier,
        TokenKind::NotEqual, TokenKind::Identifier,
         
        // if (condition) {
        TokenKind::If, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::RightParen, TokenKind::LeftBrace,
        
        // while (x < 10) {
        TokenKind::While, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Less, 
        TokenKind::IntegerLiteral, TokenKind::RightParen, TokenKind::LeftBrace,
        
        // x++
        TokenKind::Identifier, TokenKind::Increment,
        
        // }
        TokenKind::RightBrace,
        
        // } else {
        TokenKind::RightBrace, TokenKind::Else, TokenKind::LeftBrace,
        
        // for (i = 0; i < 10; i++) {
        TokenKind::For, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Assign, TokenKind::IntegerLiteral,
        TokenKind::Semicolon, TokenKind::Identifier, TokenKind::Less, TokenKind::IntegerLiteral,
        TokenKind::Semicolon, TokenKind::Identifier, TokenKind::Increment, TokenKind::RightParen, TokenKind::LeftBrace,
        
        // break
        TokenKind::Break,
        
        // }
        TokenKind::RightBrace,
        
        // }
        TokenKind::RightBrace,
        
        // v: Vector3 = new Vector3()
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::New, TokenKind::Identifier,
        TokenKind::LeftParen, TokenKind::RightParen,
        
        // len: f32 = v.length()
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::Identifier,
        TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen,
        
        // arr[index] = value
        TokenKind::Identifier, TokenKind::LeftBracket, TokenKind::Identifier, TokenKind::RightBracket,
        TokenKind::Assign, TokenKind::Identifier,
        
        // range: i32 = 0..10
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::IntegerLiteral,
        TokenKind::DotDot, TokenKind::IntegerLiteral,
        
        // inclusive: i32 = 0..=10
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::IntegerLiteral,
        TokenKind::DotDotEquals, TokenKind::IntegerLiteral,
        
        // ref_type: ref MyType
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Ref, TokenKind::Identifier,
        
        // path: Type = Module::SubModule::Type
        TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier, TokenKind::Assign, TokenKind::Identifier,
        TokenKind::DoubleColon, TokenKind::Identifier, TokenKind::DoubleColon, TokenKind::Identifier,
        
        // }
        TokenKind::RightBrace,
        
        // }
        TokenKind::RightBrace,
        
        // EOF
        TokenKind::EndOfFile
    };
    
    // Verify the complete token sequence
    ASSERT_TOKEN_SEQUENCE(stream, expected, "Complete lexer test should match expected token sequence");
    
    // Verify no lexical errors
    ASSERT_FALSE(sink.has_errors(), "Should have no lexical errors");
    
    return TestResult(true);
}

TestResult test_tokenize_all() {
    std::string source = "fn main() { x + 42 }";
    Lexer lexer(source);
    
    // Test tokenize_all method
    TokenStream stream = lexer.tokenize_all();
    
    // Verify we got all tokens
    std::vector<TokenKind> expected = {
        TokenKind::Fn,
        TokenKind::Identifier,  // main
        TokenKind::LeftParen,
        TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier,  // x
        TokenKind::Plus,
        TokenKind::IntegerLiteral,  // 42
        TokenKind::RightBrace,
        TokenKind::EndOfFile
    };
    
    ASSERT_EQ(expected.size(), stream.size(), "Should have correct number of tokens");
    
    // Verify token kinds using sequence macro
    ASSERT_TOKEN_SEQUENCE(stream, expected, "Tokenize all method should produce expected sequence");
    
    // Verify at_end behavior
    ASSERT_TRUE(stream.at_end(), "Should be at end after last token");
    
    return TestResult(true);
}

void run_lexer_tests() {
    TestSuite suite("Lexer Tests");
    
    suite.add_test("Basic Tokenization", test_basic_tokenization);
    suite.add_test("Keywords", test_keywords);
    suite.add_test("Operators", test_operators);
    suite.add_test("String Literals", test_string_literals);
    suite.add_test("Number Literals", test_number_literals);
    suite.add_test("Position Tracking", test_position_tracking);
    suite.add_test("Peek Operations", test_peek_operations);
    suite.add_test("Token Stream", test_token_stream);
    suite.add_test("Complex Expression", test_complex_expression);
    suite.add_test("Lexer Feature Combinations", test_lexer_combinations);
    suite.add_test("Lexer All Features", test_lexer_all_features);
    suite.add_test("Tokenize All", test_tokenize_all);
    
    suite.run_all();
}