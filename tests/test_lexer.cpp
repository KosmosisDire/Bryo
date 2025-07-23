#include "test/test_framework.hpp"
#include "test/parser_test_helpers.hpp"
#include "parser/lexer.hpp"
#include "parser/token_stream.hpp"

using namespace Mycelium::Testing;
using namespace Mycelium::Scripting::Lang;
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
            TokenKind::Identifier, TokenKind::AtSymbol, TokenKind::Identifier, TokenKind::Hash,
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
using System.Collections;
using Tests;

namespace Test.Namespace;

// example of bracketed namespace (not valid to have two namespaces like this but it is just an example)
namespace Test.Bracketed.Namespace
{
    public fn Stuff(): i32
    {
        return 1;
    }
}

public enum Shape
{
    None,
    Square(i32 x, i32 y, i32 width, i32 height),
    Circle(i32 x, i32 y, i32 radius)
}

public enum Direction
{
    North,
    East,
    South,
    West,

    public fn Opposite(): Direction
    {
        return match (this)
        {
            .North => .South,
            .East =>
                {
                    Console.Log("West");
                    return .West;
                },
            .South => .North,
            .West => .East,
        };
    }
}

public static type Console
{
    // members of a static class are implicitly static
	public i32 messageCount;
	f64 doubleVar1 = 2.4;
	f64 doubleVar2 = 2.4;
	string lastMessage;

	public fn Log(string msg)
	{
		Print(msg);
		messageCount++;
		lastMessage = msg;
	}

    // virtual functions can be overriden
	public virtual fn GetLast(): string
	{
		return lastMessage;
	}
}

public type Vector3
{
	public f32 x, y, z;

	// An auto implemented constructor is provided if no constructor defined
}

ref type MutableConstraint<T, U>
{
    public T value;

    public fn GetValue(): T
    {
        return value;
    }
}

public ref type Observable<T> where T : ref type, Updateable, new(i32, i32)
{
    public T value;

    // This is a simple observable that can be used to notify changes
    public fn NotifyChange()
    {
        Console.Log("Value changed to: " + value.ToString());
    }

    public fn GetValue(): T
    {
        return value;
    }
}

public type Updateable
{
    // This is an interface that can be used to mark types that can be updated
    public abstract fn Update(f32 deltaTime);
}

public abstract type Health : Updateable
{
    // prop is used to declare a property with a getter and setter.
    // properties can use the field keyword to access a backing field.
    // the field keyword is optional, if not used no auto backing field is created and you must create your own field.
    // Although an auto backing field will be created is the default getter and setter are used.
    u32 health = 100
    {
        public get => field;
        protected set =>
        {
            // value is a keyword that refers to the value being set
            if (value < 0)
            {
                Console.Log("Health cannot be negative, setting to 0");
                // field is a keyword that refers to the backing field
                field = 0;
            }

            field = value;
        }
    }

    // you can also use default access modifiers for properties
    // this will inherit access from the property declaration
    // public u32 health = 100
    //     get => field;
    //     set =>
    //     {
    //         if (value < 0)
    //         {
    //             Console.Log("Health cannot be negative, setting to 0");
    //             field = 0;
    //         }
    //         field = value;
    //     }

    // you can of course also use auto implemented properties
    // public u32 health = 100 {get; set;}
    // or
    // u32 health = 100 {get: public; set: protected;}



    // properties with only a getter can be creates with a simple arrow function
    // the getter access level matches the level of the property
    public bool isAlive => health > 0;

    // or
    // public bool isAlive =>
    // {
    //     return health > 0;
    // }


    public u32 maxHealth = 100;

    // this function is enforced meaning that any derived class must explicity choose whether to inherit this implementation or define their own implementation
    // This help to make sure that the user of a derived class is aware that this function exists and can choose to override it if needed.
    // This is basically just an abstract function with a default implementation.
    public enforced fn TakeDamage(u32 amount)
    {
        health -= amount;
    }

    // default implementations are not required. This means the derived class MUST implement this function.
    // these can only be used in abstract classes.
    public abstract fn Heal(u32 amount);

    // We do nothing by default, but force the derived class to implement this function
    public enforced fn Update(f32 deltaTime)
    {
    }
}

// if I extends health, I must implement the Heal function or choose to inherit the default implementation
public type HealthWithRegeneration : Health
{
    public f32 regenerationRate;

    // we must either implement a new TakeDamage function or choose to inherit the default implementation
    // here we choose to inherit the existing implementation
    // if we wanted to override it, we would use the override keyword
    // since we kept the enforced keyword, anything that derives from this class must implement the TakeDamage function the same as here.
    public inherit enforced fn TakeDamage(u32 amount);

    // here you can see we override the Heal function since it is abstract
    public override fn Heal(u32 amount)
    {
        health += amount;
    }

    // we can ommit the enforced function, to allow the derived class to silently inherit this new implementation
    public fn Update(f32 deltaTime)
    {
        health += (regenerationRate * deltaTime);
    }
}

// ref types always passed by reference
public ref type Enemy
{
    public static var enemies = new List<Enemy>();
    public HealthWithRegeneration health;
	public Vector3 position;
	i32 attack;
	f32 hitChance = 0.5;

	new(Vector3 startPos, u32 damage = 5)
	{
		position = startPos;
		attack = damage;
        enemies.Add(this);
	}

    public enforced fn GetDamage(): u32
    {
        PrivateFunc(42, MutableConstraint<Shape, Health>(), (Direction direction) =>
        {
            return match (direction)
            {
                .North => .Square(0, 0, 10, 10),
                .East => .Circle(0, 0, 5),
                .South => .Square(5, 5, 15, 15),
                .West => .Circle(5, 5, 10),
            };
        });

        // shorthand lambda
        PrivateFunc(42, MutableConstraint<Shape, Health>(), d => .Square(0, 0, 10, 10));

	    return Random.Chance(hitChance) ? attack : 0;
    }

    protected virtual fn PrivateFunc(i32 param, MutableConstraint<Shape, Health> bigType, Fn<Direction, Shape> functionParam): Observable<Health>
    {
        Console.Log("This is a private function");
        return Observable<Health>(health);
    }

    public virtual fn PrintStatus()
    {
        match (health)
        {
            in ..=0 => Console.Log("Enemy is dead"),
            in 1..=10 => Console.Log("Enemy is severely injured"),
            in 11..=50 => Console.Log("Enemy is injured"),
            _ => Console.Log("Enemy is healthy"),
        };
    }

}

fn Main()
{
	var running = true;
	var newvar = "Hello there";
	var someVar = 5;
    var floatVar = 3.14;
    var enemy = new Enemy(Vector3(0, 0, 0), 10);

    // this is invalid because enemy is not mut
    // enemy = new Enemy(Vector3(1, 1, 1), 20);

    // implicit type inference
    var enemy2 = new Enemy(Vector3(1, 1, 1), 20);

    // valid because enemy2 is mut
    enemy2 = new Enemy(Vector3(2, 2, 2), 30);

    for (Enemy e in Enemy.enemies)
    {
        e.PrintStatus();
        Console.Log("Enemy damage: " + e.GetDamage().ToString());
    }

    // or type can be inferred
	for (var e in Enemy.enemies)
    {
        e.PrintStatus();
        Console.Log("Enemy damage: " + e.GetDamage().ToString());
    }

    // for i in range
    for (i32 i in 0..10)
    {
        Console.Log("Index: " + i.ToString());
    }

    // or type can be inferred
    for (var i in 0..10)
    {
        Console.Log("Index: " + i.ToString());
    }

    // step by 2, "0..10 by 2" is an expression that creates a range from 0 to 10 with a step of 2
    for (var i in 0..10 by 2)
    {
        Console.Log("Index: " + i.ToString());
    }

    // use a variable for range and with a float
    for (f32 i in 0.0..floatVar by 0.5)
    {
        Console.Log("Index: " + i.ToString());
    }

    // type can still be inferred
    for (var i in 0..floatVar by 0.5)
    {
        Console.Log("Index: " + i.ToString());
    }

    // subarray with a range
    for (var i in Enemy.enemies[0..2])
    {
        i.PrintStatus();
        Console.Log("Enemy damage: " + i.GetDamage().ToString());
    }

    // subarray with a range
    for (var i in Enemy.enemies[5..10 by 2])
    {
        i.PrintStatus();
        Console.Log("Enemy damage: " + i.GetDamage().ToString());
    }

    for (i32 i = 0; i < 10; i++)
    {
        Console.Log("Index: " + i.ToString());
    }

    // for in with an index
    var array = [2,56,2,5,7,2,3,6,7];
    for (var el in array at var i)
    {
        // access the element with el and index with i
    }

	while (running)
    {
        someVar++;
        if (someVar > 10)
        {
            running = false;
        }
    }

	Console.Log("Done");
}

Main();
)";
    
    LexerOptions options;
    options.preserve_trivia = true;
    TestLexerDiagnosticSink sink;
    Lexer lexer(source, options, &sink);
    
    // Get all tokens
    TokenStream stream = lexer.tokenize_all();
    
    // Verify no lexical errors
    ASSERT_FALSE(sink.has_errors(), "Should have no lexical errors");

    // Full token sequence check
    std::vector<TokenKind> expected = {
        TokenKind::Using, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::Using, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::Namespace, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::Namespace, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Public, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Colon, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Return, TokenKind::IntegerLiteral, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Enum, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Comma,
        TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Identifier, TokenKind::RightParen, TokenKind::Comma,
        TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Identifier, TokenKind::RightParen,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Enum, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Comma,
        TokenKind::Identifier, TokenKind::Comma,
        TokenKind::Identifier, TokenKind::Comma,
        TokenKind::Identifier, TokenKind::Comma,
        TokenKind::Public, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Colon, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Return, TokenKind::Match, TokenKind::LeftParen, TokenKind::This, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Dot, TokenKind::Identifier, TokenKind::FatArrow, TokenKind::Dot, TokenKind::Identifier, TokenKind::Comma,
        TokenKind::Dot, TokenKind::Identifier, TokenKind::FatArrow,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Return, TokenKind::Dot, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::RightBrace, TokenKind::Comma,
        TokenKind::Dot, TokenKind::Identifier, TokenKind::FatArrow, TokenKind::Dot, TokenKind::Identifier, TokenKind::Comma,
        TokenKind::Dot, TokenKind::Identifier, TokenKind::FatArrow, TokenKind::Dot, TokenKind::Identifier, TokenKind::Comma,
        TokenKind::RightBrace, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Static, TokenKind::Type, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Public, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Identifier, TokenKind::Assign, TokenKind::FloatLiteral, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Identifier, TokenKind::Assign, TokenKind::FloatLiteral, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::Public, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Increment, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Assign, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Virtual, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Colon, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Return, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Type, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Public, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::Ref, TokenKind::Type, TokenKind::Identifier, TokenKind::Less, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Greater,
        TokenKind::LeftBrace,
        TokenKind::Public, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::Public, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Colon, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Return, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Ref, TokenKind::Type, TokenKind::Identifier, TokenKind::Less, TokenKind::Identifier, TokenKind::Greater, TokenKind::Where, TokenKind::Identifier, TokenKind::Colon, TokenKind::Ref, TokenKind::Type, TokenKind::Comma, TokenKind::Identifier, TokenKind::Comma, TokenKind::New, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Public, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::Public, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::Plus, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Colon, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Return, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Type, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Public, TokenKind::Abstract, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Abstract, TokenKind::Type, TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Identifier, TokenKind::Assign, TokenKind::IntegerLiteral,
        TokenKind::LeftBrace,
        TokenKind::Public, TokenKind::Get, TokenKind::FatArrow, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::Protected, TokenKind::Set, TokenKind::FatArrow,
        TokenKind::LeftBrace,
        TokenKind::If, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Less, TokenKind::IntegerLiteral, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Assign, TokenKind::IntegerLiteral, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::Identifier, TokenKind::Assign, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Identifier, TokenKind::Identifier, TokenKind::FatArrow, TokenKind::Identifier, TokenKind::Greater, TokenKind::IntegerLiteral, TokenKind::Semicolon,
        TokenKind::Public, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Assign, TokenKind::IntegerLiteral, TokenKind::Semicolon,
        TokenKind::Public, TokenKind::Enforced, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::MinusAssign, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Abstract, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Public, TokenKind::Enforced, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::RightBrace,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Type, TokenKind::Identifier, TokenKind::Colon, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Public, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::Public, TokenKind::Inherit, TokenKind::Enforced, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Public, TokenKind::Override, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::PlusAssign, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::PlusAssign, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Asterisk, TokenKind::Identifier, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Ref, TokenKind::Type, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Public, TokenKind::Static, TokenKind::Var, TokenKind::Identifier, TokenKind::Assign, TokenKind::New, TokenKind::Identifier, TokenKind::Less, TokenKind::Identifier, TokenKind::Greater, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Public, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::Public, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Identifier, TokenKind::Assign, TokenKind::FloatLiteral, TokenKind::Semicolon,
        TokenKind::New, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Assign, TokenKind::IntegerLiteral, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Assign, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Assign, TokenKind::Identifier, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::This, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Enforced, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Colon, TokenKind::Identifier,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::LeftParen, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::Identifier, TokenKind::Less, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Greater, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Comma, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::RightParen, TokenKind::FatArrow,
        TokenKind::LeftBrace,
        TokenKind::Return, TokenKind::Match, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Dot, TokenKind::Identifier, TokenKind::FatArrow, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::RightParen, TokenKind::Comma,
        TokenKind::Dot, TokenKind::Identifier, TokenKind::FatArrow, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::RightParen, TokenKind::Comma,
        TokenKind::Dot, TokenKind::Identifier, TokenKind::FatArrow, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::RightParen, TokenKind::Comma,
        TokenKind::Dot, TokenKind::Identifier, TokenKind::FatArrow, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::RightParen, TokenKind::Comma,
        TokenKind::RightBrace, TokenKind::Semicolon,
        TokenKind::RightBrace, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::LeftParen, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::Identifier, TokenKind::Less, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Greater, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Comma, TokenKind::Identifier, TokenKind::FatArrow, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::RightParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Return, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::RightParen, TokenKind::Question, TokenKind::Identifier, TokenKind::Colon, TokenKind::IntegerLiteral, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::Protected, TokenKind::Virtual, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Less, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Greater, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Less, TokenKind::Identifier, TokenKind::Comma, TokenKind::Identifier, TokenKind::Greater, TokenKind::Identifier, TokenKind::RightParen, TokenKind::Colon, TokenKind::Identifier, TokenKind::Less, TokenKind::Identifier, TokenKind::Greater,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Return, TokenKind::Identifier, TokenKind::Less, TokenKind::Identifier, TokenKind::Greater, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::Public, TokenKind::Virtual, TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Match, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::In, TokenKind::DotDotEquals, TokenKind::IntegerLiteral, TokenKind::FatArrow, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::RightParen, TokenKind::Comma,
        TokenKind::In, TokenKind::IntegerLiteral, TokenKind::DotDotEquals, TokenKind::IntegerLiteral, TokenKind::FatArrow, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::RightParen, TokenKind::Comma,
        TokenKind::In, TokenKind::IntegerLiteral, TokenKind::DotDotEquals, TokenKind::IntegerLiteral, TokenKind::FatArrow, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::RightParen, TokenKind::Comma,
        TokenKind::Underscore, TokenKind::FatArrow, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::RightParen, TokenKind::Comma,
        TokenKind::RightBrace, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::RightBrace,
        TokenKind::Fn, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Var, TokenKind::Identifier, TokenKind::Assign, TokenKind::BooleanLiteral, TokenKind::Semicolon,
        TokenKind::Var, TokenKind::Identifier, TokenKind::Assign, TokenKind::StringLiteral, TokenKind::Semicolon,
        TokenKind::Var, TokenKind::Identifier, TokenKind::Assign, TokenKind::IntegerLiteral, TokenKind::Semicolon,
        TokenKind::Var, TokenKind::Identifier, TokenKind::Assign, TokenKind::FloatLiteral, TokenKind::Semicolon,
        TokenKind::Var, TokenKind::Identifier, TokenKind::Assign, TokenKind::New, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::RightParen, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Var, TokenKind::Identifier, TokenKind::Assign, TokenKind::New, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::RightParen, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Assign, TokenKind::New, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::RightParen, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::For, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::In, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::Plus, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::For, TokenKind::LeftParen, TokenKind::Var, TokenKind::Identifier, TokenKind::In, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::Plus, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::For, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::In, TokenKind::IntegerLiteral, TokenKind::DotDot, TokenKind::IntegerLiteral, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::Plus, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::For, TokenKind::LeftParen, TokenKind::Var, TokenKind::Identifier, TokenKind::In, TokenKind::IntegerLiteral, TokenKind::DotDot, TokenKind::IntegerLiteral, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::Plus, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::For, TokenKind::LeftParen, TokenKind::Var, TokenKind::Identifier, TokenKind::In, TokenKind::IntegerLiteral, TokenKind::DotDot, TokenKind::IntegerLiteral, TokenKind::By, TokenKind::IntegerLiteral, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::Plus, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::For, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::In, TokenKind::FloatLiteral, TokenKind::DotDot, TokenKind::Identifier, TokenKind::By, TokenKind::FloatLiteral, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::Plus, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::For, TokenKind::LeftParen, TokenKind::Var, TokenKind::Identifier, TokenKind::In, TokenKind::IntegerLiteral, TokenKind::DotDot, TokenKind::Identifier, TokenKind::By, TokenKind::FloatLiteral, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::Plus, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::For, TokenKind::LeftParen, TokenKind::Var, TokenKind::Identifier, TokenKind::In, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftBracket, TokenKind::IntegerLiteral, TokenKind::DotDot, TokenKind::IntegerLiteral, TokenKind::RightBracket, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::Plus, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::For, TokenKind::LeftParen, TokenKind::Var, TokenKind::Identifier, TokenKind::In, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftBracket, TokenKind::IntegerLiteral, TokenKind::DotDot, TokenKind::IntegerLiteral, TokenKind::By, TokenKind::IntegerLiteral, TokenKind::RightBracket, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::Plus, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::For, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Identifier, TokenKind::Assign, TokenKind::IntegerLiteral, TokenKind::Semicolon, TokenKind::Identifier, TokenKind::Less, TokenKind::IntegerLiteral, TokenKind::Semicolon, TokenKind::Identifier, TokenKind::Increment, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::Plus, TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::Var, TokenKind::Identifier, TokenKind::Assign, TokenKind::LeftBracket, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::Comma, TokenKind::IntegerLiteral, TokenKind::RightBracket, TokenKind::Semicolon,
        TokenKind::For, TokenKind::LeftParen, TokenKind::Var, TokenKind::Identifier, TokenKind::In, TokenKind::Identifier, TokenKind::At, TokenKind::Var, TokenKind::Identifier, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::RightBrace,
        TokenKind::While, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Increment, TokenKind::Semicolon,
        TokenKind::If, TokenKind::LeftParen, TokenKind::Identifier, TokenKind::Greater, TokenKind::IntegerLiteral, TokenKind::RightParen,
        TokenKind::LeftBrace,
        TokenKind::Identifier, TokenKind::Assign, TokenKind::BooleanLiteral, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::RightBrace,
        TokenKind::Identifier, TokenKind::Dot, TokenKind::Identifier, TokenKind::LeftParen, TokenKind::StringLiteral, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::RightBrace,
        TokenKind::Identifier, TokenKind::LeftParen, TokenKind::RightParen, TokenKind::Semicolon,
        TokenKind::EndOfFile
    };
    
    ASSERT_TOKEN_SEQUENCE(stream, expected, "All features token sequence should match expected");
    
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

    // Verify token kinds using sequence macro
    ASSERT_TOKEN_SEQUENCE(stream, expected, "Tokenize all method should produce expected sequence");

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