Myre Language Parser Implementation Plan

Minimum Viable Product with Robust Error Recovery

Overview

This plan outlines implementing a single-class parser for Myre that can handle script-like top-level statements while maintaining robust error recovery. The parser will integrate seamlessly with the
existing AST infrastructure.

1. AST Error Handling Strategy

Approach: Minimal AST Changes

Instead of modifying every AST node, we'll use a simple, lightweight approach:

// Add to existing AstNode struct (ast.hpp:216-238)
struct AstNode {
// ... existing fields ...
bool hasError = false;        // Single bit flag for error state
// ... rest unchanged ...
};

Rationale:
- Simple: Only one additional bit per node
- Backward Compatible: Doesn't break existing code
- Fast: No pointer indirection or memory overhead
- Sufficient: Can identify which nodes have errors for later analysis

Error Reporting Strategy

- Parser maintains separate std::vector<Diagnostic> for all errors
- Each Diagnostic includes source location, error type, and message
- Error nodes marked with hasError = true and linked to diagnostics by source position

2. ParseResult Implementation

template<typename T>
struct ParseResult {
T node;                              // Always contains a node (even if error)
std::vector<Diagnostic> diagnostics; // Errors encountered during parsing

      // Constructors
      static ParseResult<T> success(T node) {
          return {node, {}};
      }

      static ParseResult<T> error(T node, Diagnostic diag) {
          node->hasError = true;
          return {node, {diag}};
      }

      // Utility methods
      bool has_errors() const { return !diagnostics.empty(); }
      void merge(const ParseResult<T>& other) {
          diagnostics.insert(diagnostics.end(), other.diagnostics.begin(), other.diagnostics.end());
      }
};

Benefits:
- Always returns valid AST node
- Accumulates all errors during parsing
- Simple to use and understand

3. Single Parser Class Design

class Parser {
private:
TokenStream& tokens;
Token current_token;
Token previous_token;

      // Simple state tracking
      bool in_loop_context = false;
      bool in_function_context = false;
      int brace_depth = 0;

      // Error collection
      std::vector<Diagnostic> all_diagnostics;

public:
// Main entry point
ParseResult<CompilationUnitNode*> parse();

      // Top-level parsing (for script support)
      ParseResult<StatementNode*> parse_global_statement();

      // Expression parsing (Pratt-style)
      ParseResult<ExpressionNode*> parse_expression(int min_precedence = 0);
      ParseResult<ExpressionNode*> parse_primary_expression();
      ParseResult<ExpressionNode*> parse_postfix_expression(ExpressionNode* left);

      // Statement parsing
      ParseResult<StatementNode*> parse_statement();
      ParseResult<BlockStatementNode*> parse_block_statement();
      ParseResult<IfStatementNode*> parse_if_statement();
      ParseResult<WhileStatementNode*> parse_while_statement();
      ParseResult<ForStatementNode*> parse_for_statement();
      ParseResult<ReturnStatementNode*> parse_return_statement();
      ParseResult<LocalVariableDeclarationNode*> parse_variable_declaration();

      // Declaration parsing
      ParseResult<FunctionDeclarationNode*> parse_function_declaration();
      ParseResult<TypeDeclarationNode*> parse_type_declaration();
      ParseResult<UsingDirectiveNode*> parse_using_directive();

private:
// Token management
void advance();
bool check(TokenType type);
bool match(TokenType type);
Token consume(TokenType type, const char* error_msg);
Token consume_or_synthetic(TokenType type, const char* error_msg);

      // Error recovery
      void synchronize_to_statement();
      void synchronize_to_declaration();
      bool at_statement_boundary();
      bool at_declaration_boundary();
      void skip_to_matching_brace();

      // Error creation
      TokenNode* create_error_token(const char* error_msg);
      ExpressionNode* create_error_expression(const char* error_msg);
      Diagnostic create_diagnostic(const char* message, DiagnosticLevel level = DiagnosticLevel::Error);
};

4. Implementation Phases

Phase 1: Core Infrastructure (Week 1)

Files to create:
- include/parser/parser.hpp
- src/parser/parser.cpp
- include/parser/parse_result.hpp

Implementation Steps:
1. Create ParseResult template
2. Implement basic Parser class structure
3. Add token management methods
4. Implement basic error recovery synchronization

Testing:
- Test token consumption and advancement
- Test error recovery at statement boundaries
- Test ParseResult error accumulation

Phase 2: Expression Parsing (Week 1-2)

Implementation Steps:
1. Implement parse_primary_expression() for literals, identifiers, parentheses
2. Add parse_expression() with precedence climbing for binary operators
3. Implement unary expression parsing
4. Add postfix expressions (calls, member access, indexing)

Error Recovery:
- Missing operands → create error expression nodes
- Unmatched parentheses → insert synthetic tokens
- Invalid operators → skip and continue

Testing:
- Test all operator precedence combinations
- Test error recovery for malformed expressions
- Test parentheses balancing

Phase 3: Statement Parsing (Week 2)

Implementation Steps:
1. Implement parse_statement() dispatcher
2. Add parse_block_statement() with brace matching
3. Implement control flow: if, while, for
4. Add return, break, continue statements
5. Implement variable declarations

Error Recovery:
- Missing semicolons → insert synthetic semicolons
- Unclosed blocks → balance braces at logical points
- Invalid control flow → skip to statement boundaries

Testing:
- Test nested block structures
- Test control flow with missing tokens
- Test variable declaration error cases

Phase 4: Declaration Parsing (Week 2-3)

Implementation Steps:
1. Implement parse_function_declaration()
2. Add parse_type_declaration() for classes
3. Implement parse_using_directive()
4. Add script-level statement support in parse()

Error Recovery:
- Missing function bodies → create empty blocks
- Invalid type syntax → create error type nodes
- Missing modifiers → continue with defaults

Testing:
- Test function declarations with various signatures
- Test type declarations with members
- Test script-style top-level statements

Phase 5: Integration & Polish (Week 3)

Implementation Steps:
1. Integrate with existing lexer and token stream
2. Add comprehensive error messages
3. Optimize error recovery heuristics
4. Add parser performance metrics

Testing:
- Integration tests with real Myre code
- Stress testing with malformed input
- Performance benchmarking

5. Error Recovery Strategies by Construct

Expression Errors

// Missing operand
var x = 5 +;        // Insert error expression, continue
var y = * 3;        // Skip invalid operator, continue

// Unmatched parentheses  
var z = (5 + 3;     // Insert closing paren at statement end

Statement Errors

// Missing semicolon
var x = 5          // Insert semicolon, continue
if (true) {        // Continue normally

// Unclosed block
if (true) {
var y = 10;
// Missing }        // Insert } before next statement
while (false) { }

Declaration Errors

// Missing function body
fn test()          // Insert empty block: fn test() { }

// Invalid type syntax
type ;;            // Skip to next declaration

6. Script-Level Support

The parser will handle script-like execution by treating the compilation unit as containing any valid statement:

// Top-level statements (valid in scripts)
using System;
var x = 5;
Console.Log("Hello");

fn helper() { return 42; }

type Point { var x: i32; var y: i32; }

if (x > 0) {
Console.Log("Positive");
}

Implementation:
- CompilationUnitNode.statements can contain any StatementNode
- Declarations inherit from StatementNode (already done in AST)
- Parser attempts to parse each top-level construct as a statement

7. Testing Strategy

Unit Tests

- Token Management: Verify advance, consume, match operations
- Expression Parsing: Test precedence, associativity, error cases
- Statement Parsing: Test control flow, variable declarations
- Declaration Parsing: Test functions, types, using directives
- Error Recovery: Test each recovery strategy independently

Integration Tests

- Complete Programs: Parse valid Myre programs end-to-end
- Error Scenarios: Parse malformed code, verify error quality
- Script Execution: Parse script-style code with top-level statements

Error Quality Tests

- Verify error messages are helpful and accurate
- Test that error recovery allows continued parsing
- Ensure no cascading errors from single mistakes

8. Performance Considerations

Memory Efficiency

- Single allocation for each AST node via existing allocator
- Minimal error overhead (1 bit per node)
- Error diagnostics stored separately, not in every node

Parsing Speed

- Single-pass parsing with no backtracking
- Precedence climbing for optimal expression parsing
- Minimal virtual function calls

Error Recovery Speed

- Fast synchronization to known recovery points
- Limited lookahead to avoid expensive searching
- Fail-fast for unrecoverable errors

9. Success Criteria

Functional Requirements

✅ Parse valid Myre programs completely✅ Support script-style top-level statements✅ Recover from all common syntax errors✅ Produce usable AST even with errors✅ Generate helpful error messages

Quality Requirements

✅ No crashes on malformed input✅ Predictable error recovery behavior✅ Performance comparable to hand-written parsers✅ Maintainable single-class design

This plan provides a robust foundation for Myre's parser while maintaining simplicity and following proven industry patterns. The focus on error recovery ensures the parser can handle any input gracefully     
while producing useful results for both compilation and IDE scenarios.
