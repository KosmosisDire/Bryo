#include "test/test_framework.hpp"
#include "test/test_helpers.hpp"
#include "test/parser_test_helpers.hpp"
#include "parser/recursive_parser.hpp"
#include "parser/pratt_parser.hpp"
#include "parser/token_stream.hpp"
#include "parser/lexer.hpp"
#include "parser/parser_context.hpp"
#include "ast/ast_allocator.hpp"

using namespace Mycelium::Testing;
using namespace Mycelium::Scripting::Parser;
using namespace Mycelium::Scripting::Lang;

// Test environment setup
struct RecursiveParserTestEnv {
    std::string source;
    std::unique_ptr<Lexer> lexer;
    std::unique_ptr<TokenStream> token_stream;
    std::unique_ptr<ParserContext> context;
    std::unique_ptr<AstAllocator> allocator;
    std::unique_ptr<RecursiveParser> parser;
    std::shared_ptr<PrattParser> expr_parser;
    
    RecursiveParserTestEnv(const std::string& src) : source(src) {
        LexerOptions options;
        options.preserve_trivia = false; // Test with trivia disabled
        
        lexer = std::make_unique<Lexer>(source, options);
        auto token_stream_value = lexer->tokenize_all();
        token_stream = std::make_unique<TokenStream>(std::move(token_stream_value));
        context = std::make_unique<ParserContext>(source);
        allocator = std::make_unique<AstAllocator>();
        parser = std::make_unique<RecursiveParser>(*token_stream, *context, *allocator);
        
        // Create expression parser and link them
        expr_parser = std::make_shared<PrattParser>(*token_stream, *context, *allocator);
        parser->set_expression_parser(expr_parser);
    }
};

// Test basic function declaration parsing
TestResult test_function_declaration_basic() {
    RecursiveParserTestEnv env("fn test() {}");
    
    auto result = env.parser->parse_function_declaration();
    
    // DEBUG: Print error details if parsing failed
    if (!result.has_value()) {
        std::cout << "\n=== PARSE FAILURE DEBUG ===\n";
        std::cout << "Source: '" << env.source << "'\n";
        std::cout << "Error count: " << result.errors().size() << "\n";
        for (const auto& error : result.errors()) {
            std::cout << "Error: " << error.message << " at line " << error.location.line << ", col " << error.location.column << "\n";
        }
        std::cout << "=========================\n";
        return TestResult(false, "Parse failed - see debug output above");
    }
    
    ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse basic function declaration");
    
    auto func = result.value();
    ASSERT_AST_NOT_NULL(func, func, "Function should not be null");
    ASSERT_AST_NOT_NULL(func->name, func, "Function name should not be null");
    ASSERT_IDENTIFIER_NAME(func->name, "test", func, "Function name should be 'test'");
    ASSERT_AST_NOT_NULL(func->fnKeyword, func, "fn keyword should not be null");
    ASSERT_AST_NOT_NULL(func->body, func, "Function body should not be null");
    ASSERT_AST_NULL(func->returnType, func, "Return type should be null for void function");
    
    return TestResult(true);
}

// Test function declaration with return type
TestResult test_function_declaration_with_return_type() {
    RecursiveParserTestEnv env("fn getValue() -> i32 {}");
    
    auto result = env.parser->parse_function_declaration();
    ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse function with return type");
    
    auto func = result.value();
    ASSERT_AST_NOT_NULL(func, func, "Function should not be null");
    ASSERT_IDENTIFIER_NAME(func->name, "getValue", func, "Function name should be 'getValue'");
    ASSERT_AST_NOT_NULL(func->arrow, func, "Arrow token should not be null");
    ASSERT_AST_NOT_NULL(func->returnType, func, "Return type should not be null");
    
    return TestResult(true);
}

// Test function declaration with parameters
TestResult test_function_declaration_with_parameters() {
    RecursiveParserTestEnv env("fn add(a: i32, b: i32) -> i32 {}");
    
    auto result = env.parser->parse_function_declaration();
    ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse function with parameters");
    
    auto func = result.value();
    ASSERT_AST_NOT_NULL(func, func, "Function should not be null");
    ASSERT_IDENTIFIER_NAME(func->name, "add", func, "Function name should be 'add'");
    ASSERT_AST_EQ(2, func->parameters.size, func, "Should have 2 parameters");
    
    return TestResult(true);
}

// Test type declaration parsing
TestResult test_type_declaration_basic() {
    RecursiveParserTestEnv env("type MyType {}");
    
    auto result = env.parser->parse_type_declaration();
    ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse basic type declaration");
    
    auto type_decl = result.value();
    ASSERT_AST_NOT_NULL(type_decl, type_decl, "Type declaration should not be null");
    ASSERT_AST_NOT_NULL(type_decl->name, type_decl, "Type name should not be null");
    ASSERT_IDENTIFIER_NAME(type_decl->name, "MyType", type_decl, "Type name should be 'MyType'");
    ASSERT_AST_NOT_NULL(type_decl->typeKeyword, type_decl, "Type keyword should not be null");
    ASSERT_AST_NOT_NULL(type_decl->openBrace, type_decl, "Open brace should not be null");
    ASSERT_AST_NOT_NULL(type_decl->closeBrace, type_decl, "Close brace should not be null");
    
    return TestResult(true);
}

// Test block statement parsing
TestResult test_block_statement_basic() {
    RecursiveParserTestEnv env("{}");
    
    auto result = env.parser->parse_block_statement();
    ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse empty block statement");
    
    auto block = result.value();
    ASSERT_AST_NOT_NULL(block, block, "Block should not be null");
    ASSERT_AST_NOT_NULL(block->openBrace, block, "Open brace should not be null");
    ASSERT_AST_NOT_NULL(block->closeBrace, block, "Close brace should not be null");
    ASSERT_AST_EQ(0, block->statements.size, block, "Empty block should have no statements");
    
    return TestResult(true);
}

// Test if statement parsing
TestResult test_if_statement_basic() {
    RecursiveParserTestEnv env("if (true) {}");
    
    auto result = env.parser->parse_if_statement();
    ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse basic if statement");
    
    auto if_stmt = result.value();
    ASSERT_AST_NOT_NULL(if_stmt, if_stmt, "If statement should not be null");
    ASSERT_AST_NOT_NULL(if_stmt->ifKeyword, if_stmt, "If keyword should not be null");
    ASSERT_AST_NOT_NULL(if_stmt->openParen, if_stmt, "Open paren should not be null");
    ASSERT_AST_NOT_NULL(if_stmt->condition, if_stmt, "Condition should not be null");
    ASSERT_AST_NOT_NULL(if_stmt->closeParen, if_stmt, "Close paren should not be null");
    ASSERT_AST_NOT_NULL(if_stmt->thenStatement, if_stmt, "Then statement should not be null");
    ASSERT_AST_NULL(if_stmt->elseKeyword, if_stmt, "Else keyword should be null");
    ASSERT_AST_NULL(if_stmt->elseStatement, if_stmt, "Else statement should be null");
    
    return TestResult(true);
}

// Test if-else statement parsing
TestResult test_if_else_statement() {
    RecursiveParserTestEnv env("if (false) {} else {}");
    
    auto result = env.parser->parse_if_statement();
    ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse if-else statement");
    
    auto if_stmt = result.value();
    ASSERT_AST_NOT_NULL(if_stmt, if_stmt, "If statement should not be null");
    ASSERT_AST_NOT_NULL(if_stmt->condition, if_stmt, "Condition should not be null");
    ASSERT_AST_NOT_NULL(if_stmt->thenStatement, if_stmt, "Then statement should not be null");
    ASSERT_AST_NOT_NULL(if_stmt->elseKeyword, if_stmt, "Else keyword should not be null");
    ASSERT_AST_NOT_NULL(if_stmt->elseStatement, if_stmt, "Else statement should not be null");
    
    return TestResult(true);
}

// Test compilation unit parsing
TestResult test_compilation_unit_basic() {
    RecursiveParserTestEnv env("fn main() {}");
    
    auto result = env.parser->parse_compilation_unit();
    ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse basic compilation unit");
    
    auto unit = result.value();
    ASSERT_AST_NOT_NULL(unit, unit, "Compilation unit should not be null");
    ASSERT_AST_EQ(1, unit->statements.size, unit, "Should have one statement");
    auto func ASSERT_NODE_TYPE(unit->statements[0], FunctionDeclarationNode, unit, "Statement should be function declaration");
    
    return TestResult(true);
}

// Test multiple declarations in compilation unit
TestResult test_compilation_unit_multiple_declarations() {
    RecursiveParserTestEnv env("fn first() {} type MyType {} fn second() {}");
    
    auto result = env.parser->parse_compilation_unit();
    ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse compilation unit with multiple declarations");
    
    auto unit = result.value();
    ASSERT_AST_NOT_NULL(unit, unit, "Compilation unit should not be null");
    ASSERT_AST_EQ(3, unit->statements.size, unit, "Should have three statements");
    auto first_func ASSERT_NODE_TYPE(unit->statements[0], FunctionDeclarationNode, unit, "First should be function");
    auto type_decl ASSERT_NODE_TYPE(unit->statements[1], TypeDeclarationNode, unit, "Second should be type");
    auto second_func ASSERT_NODE_TYPE(unit->statements[2], FunctionDeclarationNode, unit, "Third should be function");
    
    return TestResult(true);
}

// Test error recovery in function declaration
TestResult test_function_declaration_error_recovery() {
    RecursiveParserTestEnv env("fn invalid syntax here");
    
    auto result = env.parser->parse_function_declaration();
    
    // Should have some form of recovery - either partial success with errors or complete failure
    bool has_errors = result.has_errors() || !result.has_value();
    ASSERT_AST_TRUE(has_errors, nullptr, "Should report errors for invalid syntax");
    
    return TestResult(true);
}

// Test variable declaration detection
TestResult test_variable_declaration_detection() {
    RecursiveParserTestEnv env("x: i32 = 5");
    
    bool is_var_decl = env.parser->is_variable_declaration_start();
    ASSERT_AST_TRUE(is_var_decl, nullptr, "Should detect variable declaration with type");
    
    return TestResult(true);
}

// Test type inference variable declaration detection  
TestResult test_type_inference_variable_detection() {
    RecursiveParserTestEnv env("x = 42");
    
    bool is_var_decl = env.parser->is_variable_declaration_start();
    ASSERT_AST_TRUE(is_var_decl, nullptr, "Should detect variable declaration with type inference");
    
    return TestResult(true);
}

// Test property declaration detection
TestResult test_property_declaration_detection() {
    RecursiveParserTestEnv env("health: prop u32");
    
    bool is_prop_decl = env.parser->is_property_declaration_start();
    ASSERT_AST_TRUE(is_prop_decl, nullptr, "Should detect property declaration");
    
    return TestResult(true);
}

// Test parsing context management
TestResult test_parsing_context_management() {
    RecursiveParserTestEnv env("{}");
    
    // Test that parsing context is properly managed during block parsing
    auto result = env.parser->parse_block_statement();
    ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse block with proper context management");
    
    return TestResult(true);
}

// Test complex nested structure
TestResult test_complex_nested_structure() {
    RecursiveParserTestEnv env(R"(
        fn outer() {
            if (true) {
                fn inner() {}
            }
        }
    )");
    
    auto result = env.parser->parse_function_declaration();
    ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse complex nested structure");
    
    auto func = result.value();
    ASSERT_AST_NOT_NULL(func, func, "Function should not be null");
    ASSERT_AST_NOT_NULL(func->body, func, "Function body should not be null");
    
    return TestResult(true);
}

// Test combination of multiple recursive parser features
TestResult test_recursive_combinations() {
    // Test 1: Function with complex body - multiple statements and nested blocks
    {
        RecursiveParserTestEnv env(R"(
            fn calculateScore(player: Player, bonus: i32) -> i32 {
                if (player.level > 10) {
                    return player.score * 2 + bonus
                } else {
                    if (bonus > 0) {
                        return player.score + bonus
                    }
                }
                return player.score
            }
        )");
        
        auto result = env.parser->parse_function_declaration();
        ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse function with complex body");
        
        auto func = result.value();
        ASSERT_AST_NOT_NULL(func, func, "Function should not be null");
        ASSERT_AST_NOT_NULL(func->name, func, "Should have function name");
        ASSERT_IDENTIFIER_NAME(func->name, "calculateScore", func, "Function name should match");
        ASSERT_AST_EQ(2, func->parameters.size, func, "Should have 2 parameters");
        ASSERT_AST_NOT_NULL(func->returnType, func, "Should have return type");
        ASSERT_AST_NOT_NULL(func->body, func, "Should have body");
        
        // Verify body has statements (at least the if statement)
        ASSERT_AST_TRUE(func->body->statements.size >= 1, func, "Body should have at least 1 statement");
    }
    
    // Test 2: Type declaration with multiple member types
    {
        RecursiveParserTestEnv env(R"(
            type GameState {
                players: ref Array<Player>
                score: i32
                level: u8
                isActive: bool
                
                fn reset() {
                    score = 0
                    level = 1
                    isActive = true
                }
                
                fn addPlayer(p: Player) {
                    players.push(p)
                }
            }
        )");
        
        auto result = env.parser->parse_type_declaration();
        ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse type with mixed members");
        
        auto type_decl = result.value();
        ASSERT_AST_NOT_NULL(type_decl, type_decl, "Type declaration should not be null");
        ASSERT_IDENTIFIER_NAME(type_decl->name, "GameState", type_decl, "Type name should match");
        
        // Note: Since parse_member_declaration_list() returns empty, 
        // we can't verify members yet, but structure should parse
    }
    
    // Test 3: Nested if-else with complex conditions
    {
        RecursiveParserTestEnv env(R"(
            if (x > 0 && y < 10) {
                if (z == 5) {
                    doSomething()
                } else {
                    doSomethingElse()
                }
            } else if (x < 0) {
                handleNegative()
            } else {
                handleZero()
            }
        )");
        
        auto result = env.parser->parse_if_statement();
        ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse nested if-else");
        
        auto if_stmt = result.value();
        ASSERT_AST_NOT_NULL(if_stmt->condition, if_stmt, "Should have condition");
        ASSERT_AST_NOT_NULL(if_stmt->thenStatement, if_stmt, "Should have then statement");
        ASSERT_AST_NOT_NULL(if_stmt->elseStatement, if_stmt, "Should have else statement");
        
        // The else statement should be another if statement (else if)
        auto else_if ASSERT_NODE_TYPE(if_stmt->elseStatement, IfStatementNode, if_stmt, "Else should be if statement (else if)");
    }
    
    // Test 4: Multiple types of statements in a block
    {
        RecursiveParserTestEnv env(R"(
            {
                let x = 10
                let y: f32 = 3.14
                
                if (x > 5) {
                    process(x)
                }
                
                fn localFunc() -> bool {
                    return true
                }
                
                while (x > 0) {
                    x = x - 1
                }
                
                return x + y
            }
        )");
        
        auto result = env.parser->parse_block_statement();
        ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse block with mixed statements");
        
        auto block = result.value();
        ASSERT_AST_TRUE(block->statements.size > 0, block, "Block should have statements");
        
        // Note: Variable declarations and other statements aren't implemented yet,
        // but the block structure should parse
    }
    
    // Test 5: Compilation unit with mixed top-level declarations
    {
        RecursiveParserTestEnv env(R"(
            using System.Collections
            
            namespace MyGame {
                type Player {
                    name: string
                    score: i32
                }
                
                interface IScoreable {
                    fn getScore() -> i32
                }
                
                enum GameMode {
                    case SinglePlayer
                    case MultiPlayer(maxPlayers: i32)
                    case Tournament
                }
                
                fn main() {
                    let game = createGame()
                    game.start()
                }
            }
        )");
        
        auto result = env.parser->parse_compilation_unit();
        ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse compilation unit");
        
        auto unit = result.value();
        ASSERT_AST_TRUE(unit->statements.size > 0, unit, "Should have top-level statements");
        
        // Note: Many declaration types aren't implemented yet,
        // but basic structure should parse without crashing
    }
    
    return TestResult(true);
}

// Test all recursive parser features combined
TestResult test_recursive_all_features() {
    // Comprehensive Mycelium program using all recursive parser features
    std::string source = R"(
        // Complete Mycelium program test
        namespace GameEngine {
            using System.Math
            using System.Collections.Generic
            
            // Enum with associated data
            enum EntityType {
                case Player(health: i32, mana: i32)
                case Enemy(damage: i32)
                case NPC(dialogue: string)
            }
            
            // Interface definition
            interface IEntity {
                fn update(deltaTime: f32)
                fn render(renderer: ref Renderer)
                fn getPosition() -> Vector3
            }
            
            // Complex type with multiple features
            type GameObject {
                position: Vector3
                rotation: Quaternion
                scale: f32 = 1.0
                isActive: bool = true
                components: ref List<Component>
                
                // Property with custom getter/setter
                health: prop i32 {
                    get => field
                    set {
                        field = value.clamp(0, maxHealth)
                        onHealthChanged(field)
                    }
                }
                
                // Constructor
                new(pos: Vector3) {
                    position = pos
                    rotation = Quaternion.identity()
                    components = new List<Component>()
                }
                
                // Methods with various features
                fn addComponent(comp: ref Component) {
                    components.add(comp)
                    comp.gameObject = this
                }
                
                fn update(deltaTime: f32) virtual {
                    if (!isActive) { return }
                    
                    for (comp in components) {
                        if (comp.enabled) {
                            comp.update(deltaTime)
                        }
                    }
                }
                
                // Static method
                fn createPlayer(name: string) static -> ref GameObject {
                    let obj = new GameObject(Vector3.zero())
                    obj.addComponent(new PlayerController(name))
                    return obj
                }
            }
            
            // Main game class
            type Game {
                entities: ref List<GameObject>
                renderer: Renderer
                isRunning: bool
                
                fn start() public {
                    isRunning = true
                    
                    // Initialize game
                    entities = new List<GameObject>()
                    renderer = new Renderer()
                    
                    // Create initial entities
                    let player = GameObject.createPlayer("Hero")
                    entities.add(player)
                    
                    // Game loop
                    while (isRunning) {
                        let deltaTime = Time.deltaTime()
                        
                        // Update all entities
                        for (entity in entities) {
                            entity.update(deltaTime)
                        }
                        
                        // Render
                        renderer.begin()
                        for (entity in entities) {
                            if (entity.isActive) {
                                entity.render(renderer)
                            }
                        }
                        renderer.end()
                        
                        // Check exit condition
                        if (Input.isKeyPressed(Key.Escape)) {
                            isRunning = false
                        }
                    }
                }
                
                fn stop() public {
                    isRunning = false
                    cleanup()
                }
                
                fn cleanup() private {
                    for (entity in entities) {
                        entity.destroy()
                    }
                    entities.clear()
                }
            }
            
            // Entry point
            fn main() {
                let game = new Game()
                
                try {
                    game.start()
                } catch (e: GameException) {
                    Console.log("Game error: " + e.message)
                } finally {
                    game.cleanup()
                }
                
                return 0
            }
        }
    )";
    
    RecursiveParserTestEnv env(source);
    auto result = env.parser->parse_compilation_unit();
    
    ASSERT_AST_TRUE(result.has_value(), nullptr, "Should parse comprehensive program");
    
    auto unit = result.value();
    ASSERT_AST_NOT_NULL(unit, unit, "Compilation unit should not be null");
    ASSERT_AST_TRUE(unit->statements.size > 0, unit, "Should have statements");
    
    // Verify we parsed various declaration types
    // (Note: Many features aren't implemented yet, but we should handle them gracefully)
    
    bool has_namespace = false;
    bool has_type = false;
    bool has_function = false;
    
    for (auto stmt : unit->statements) {
        if (node_is<NamespaceDeclarationNode>(stmt)) has_namespace = true;
        if (node_is<TypeDeclarationNode>(stmt)) has_type = true;
        if (node_is<FunctionDeclarationNode>(stmt)) has_function = true;
    }
    
    // At minimum, we should parse some declarations even if not all are implemented
    ASSERT_AST_TRUE(has_namespace || has_type || has_function, unit,
                "Should parse at least some declarations");
    
    return TestResult(true);
}

void run_recursive_parser_tests() {
    TestSuite suite("Recursive Parser Tests");
    
    suite.add_test("Function Declaration Basic", test_function_declaration_basic);
    suite.add_test("Function Declaration with Return Type", test_function_declaration_with_return_type);
    suite.add_test("Function Declaration with Parameters", test_function_declaration_with_parameters);
    suite.add_test("Type Declaration Basic", test_type_declaration_basic);
    suite.add_test("Block Statement Basic", test_block_statement_basic);
    suite.add_test("If Statement Basic", test_if_statement_basic);
    suite.add_test("If-Else Statement", test_if_else_statement);
    suite.add_test("Compilation Unit Basic", test_compilation_unit_basic);
    suite.add_test("Compilation Unit Multiple Declarations", test_compilation_unit_multiple_declarations);
    suite.add_test("Function Declaration Error Recovery", test_function_declaration_error_recovery);
    suite.add_test("Variable Declaration Detection", test_variable_declaration_detection);
    suite.add_test("Type Inference Variable Detection", test_type_inference_variable_detection);
    suite.add_test("Property Declaration Detection", test_property_declaration_detection);
    suite.add_test("Parsing Context Management", test_parsing_context_management);
    suite.add_test("Complex Nested Structure", test_complex_nested_structure);
    suite.add_test("Recursive Feature Combinations", test_recursive_combinations);
    suite.add_test("Recursive All Features", test_recursive_all_features);
    
    suite.run_all();
}