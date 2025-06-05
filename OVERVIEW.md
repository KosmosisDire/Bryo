# Sharpie Project Overview

This document provides an overview of the Sharpie language project, highlighting important files and common development tasks.

## Important Directories and Files

*   **`include/sharpie/ast/`**: Contains the Abstract Syntax Tree (AST) node definitions. Each file typically defines a category of AST nodes (e.g., `ast_expressions.hpp`, `ast_statements.hpp`).
    *   **`include/sharpie/script_ast.hpp`**: A master include file that conveniently pulls in all individual AST node headers.
*   **`src/sharpie/parser/`**: Houses the Lexer and Parser implementation.
    *   **`src/sharpie/parser/parser_lexer.cpp`**: Implements the lexical analysis, converting source code into tokens.
    *   **`src/sharpie/parser/script_parser.cpp`**: Contains the main parsing logic, building the AST from tokens.
    *   **`src/sharpie/parser/parser_rules.cpp`**: Defines the grammar rules and their corresponding AST node creation.
    *   **`src/sharpie/parser/parser_helpers.cpp`**: Utility functions for the parser.
*   **`include/sharpie/compiler/`** and **`src/sharpie/compiler/`**: Contain the compiler's core logic, including semantic analysis and LLVM IR generation.
    *   **`include/sharpie/compiler/script_compiler.hpp`**: Main compiler interface.
    *   **`src/sharpie/compiler/script_compiler.cpp`**: Compiler implementation.
    *   **`src/sharpie/compiler/compiler_visitors.cpp`**: Contains visitor patterns for traversing the AST to perform semantic analysis (`SemanticAnalyzerVisitor`) and LLVM IR generation (`IRGeneratorVisitor`).
*   **`lib/mycelium_runtime.cpp`** and **`lib/mycelium_runtime.h`**: The Sharpie runtime library, providing essential functions for memory management (Automatic Reference Counting - ARC), string manipulation, and other built-in functionalities.
*   **`tests/test.sp`**: Example Sharpie source code files used for testing the compiler and runtime.
*   **`CMakeLists.txt`**: The CMake build configuration file for the entire project.

## Common Development Tasks

### 1. Adding a New External Function Call (C/C++ Interop)

To expose a C/C++ function to Sharpie:

1.  **Declare in Sharpie:** In your Sharpie source file (`.sp`), declare the external function using the `extern` keyword.
    ```sharpie
    namespace System.Console {
        extern void print(string s);
        extern void printInt(int i);
    }
    ```
2.  **Implement in C++ Runtime:** Implement the corresponding C++ function in `lib/mycelium_runtime.cpp` or a similar runtime binding file. Ensure it matches the signature declared in Sharpie.
    ```cpp
    // In lib/mycelium_runtime.cpp
    #include "mycelium_runtime.h"
    #include <iostream>

    extern "C" void Mycelium_System_Console_print(MyceliumString* s) {
        std::cout << s->data << std::endl;
        Mycelium_String_release(s); // Release the string after use
    }

    extern "C" void Mycelium_System_Console_printInt(int i) {
        std::cout << i << std::endl;
    }
    ```
    *Note: External functions should typically follow a naming convention (e.g., `Mycelium_Namespace_ClassName_MethodName`) to avoid conflicts and provide clear binding.*
3.  **Declare in Runtime Header:** Add the C++ function declaration to `lib/mycelium_runtime.h` (or `lib/runtime_binding.h` if a separate binding header is used) with `extern "C"` linkage.
    ```cpp
    // In lib/mycelium_runtime.h
    #ifdef __cplusplus
    extern "C" {
    #endif

    // ... existing declarations ...
    void Mycelium_System_Console_print(MyceliumString* s);
    void Mycelium_System_Console_printInt(int i);

    #ifdef __cplusplus
    }
    #endif
    ```
4.  **Compiler Handling:** The compiler's `IRGeneratorVisitor` (in `src/sharpie/compiler/compiler_visitors.cpp`) is responsible for generating the correct LLVM IR call to these `extern` functions. This is typically handled automatically once the function is declared in Sharpie and its signature is known.

### 2. Adding a New Keyword or Token

1.  **Define Token Type:** Add a new entry to the `TokenType` enum in `include/sharpie/common/script_token_types.hpp`.
    ```cpp
    // In include/sharpie/common/script_token_types.hpp
    enum class TokenType {
        // ... existing tokens ...
        NewKeyword, // Example: for a new keyword like 'async'
        // ...
    };
    ```
2.  **Update Lexer:** Modify `src/sharpie/parser/parser_lexer.cpp` to recognize the new keyword or token pattern. This usually involves adding a new rule to the lexer's state machine or a new entry in a keyword lookup map.

### 3. Adding a New AST Node

1.  **Define AST Node Class:** Create a new class for your AST node in the appropriate header file under `include/sharpie/ast/` (e.g., `ast_statements.hpp` for a new statement, `ast_expressions.hpp` for a new expression). Inherit from `ASTNode` or a more specific base class like `ExpressionAST` or `StatementAST`.
    ```cpp
    // In include/sharpie/ast/ast_statements.hpp
    class NewStatementAST : public StatementAST {
    public:
        // ... members and constructor ...
        void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    };
    ```
2.  **Include in Master Header:** Add the new AST node's header to `include/sharpie/script_ast.hpp`.
3.  **Update Parser Rules:** Modify `src/sharpie/parser/parser_rules.cpp` and `src/sharpie/parser/script_parser.cpp` to include parsing logic that creates instances of your new AST node when the corresponding syntax is encountered.
4.  **Implement Semantic Analysis:** In `src/sharpie/compiler/compiler_visitors.cpp`, add a `visit` method for your new AST node to the `SemanticAnalyzerVisitor` class. This method will perform type checking, symbol resolution, and other semantic validations.
    ```cpp
    // In src/sharpie/compiler/compiler_visitors.cpp
    void SemanticAnalyzerVisitor::visit(NewStatementAST& node) {
        // ... semantic analysis logic for NewStatementAST ...
    }
    ```
5.  **Implement LLVM IR Generation:** Also in `src/sharpie/compiler/compiler_visitors.cpp`, add a `visit` method for your new AST node to the `IRGeneratorVisitor` class. This method will generate the appropriate LLVM IR for your new language construct.
    ```cpp
    // In src/sharpie/compiler/compiler_visitors.cpp
    llvm::Value* IRGeneratorVisitor::visit(NewStatementAST& node) {
        // ... LLVM IR generation logic for NewStatementAST ...
        return nullptr; // Or the generated value if it's an expression
    }
    ```

### 4. Adding a New Built-in Type

1.  **Define Type in AST:** If it's a primitive or a fundamental type, define its representation in `include/sharpie/ast/ast_types.hpp`. For complex types (like `MyceliumString`), you might have a corresponding C++ struct/class in the runtime.
2.  **Update Semantic Analysis:** Ensure the `SemanticAnalyzerVisitor` correctly handles the new type during type checking, assignments, and conversions.
3.  **Define LLVM IR Representation:** In the compiler's IR generation phase, define how this new type maps to an LLVM type (e.g., `llvm::Type::getInt32Ty(Context)` for `int`, or `llvm::StructType::create(...)` for custom objects).
4.  **Implement Runtime Support:** For complex types or types requiring specific memory management, implement the necessary C++ logic in `lib/mycelium_runtime.cpp` (e.g., allocation, deallocation, specific operations).

This overview should serve as a helpful guide for navigating the Sharpie codebase and contributing to its development.
