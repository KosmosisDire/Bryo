# Sharpie Language Development Plan & Survey

## Introduction

This document outlines the development plan for Sharpie, a C#-like language built on LLVM. It begins with an overview of the language's target features based on your survey responses, followed by an iterative development plan structured into "Sweeps." Each sweep aims to deliver a functional vertical slice of the language, prioritizing core functionality and foundational features.

## Language Design Survey

### 1. General Language Design
    *   Paradigm: Primarily Object-Oriented with strong functional programming support.
    *   Typing: Static Typing.
    *   Error Handling: To be brainstormed further; initial focus on robust runtime and clear diagnostics.
    *   Concurrency: Async/Await.
### 2. Syntax and Semantics
    *   Style: Closely follow C# syntax initially.
    *   Case Sensitivity: Case-sensitive.
    *   Null Handling: C# 8+ style Nullable Reference Types.
    *   Statement Termination: Semicolons required.
    *   Properties: C#-style `get; set;`.
### 3. Type System
    *   Primitives: Backed by value-type structs (e.g., `int` as `System.Int32`), with automatic boxing/unboxing.
    *   Value/Reference: `struct` for value types, `class` for reference types.
    *   Generics: Reified generics.
    *   Inheritance: Single class inheritance, multiple interface inheritance. Mixins/traits desired later. Default interface methods not initially.
    *   Operator Overloading: Yes.
    *   Extension Methods: Yes.
    *   Delegates/Events: C#-style, syntax/handling might evolve.
### 4. Memory Management
    *   Primary: Automatic Reference Counting (ARC).
    *   Cycle Detection: Weak references. Compile-time cycle detection is a future goal.
    *   Manual Memory: `unsafe` contexts desired eventually, not an initial priority.
### 5. Standard Library (Initial Priorities)
    *   Collections: `List`, `Dictionary` (High Priority). `Set`, `Queue/Stack` (Low Priority).
    *   String Manipulation: Rich library.
    *   I/O: Console I/O (High), File I/O (Medium), Network I/O (Low).
    *   Reflection: Basic initially, full reflection is a future goal.
    *   Date/Time: Yes (Low Priority).
### 6. Tooling and Ecosystem
    *   Build System (for compiler): CMake.
    *   Package Management: Eventual goal, not initial priority.
    *   Debugging Support: Basic initially, comprehensive later.
    *   Interop: C/C++ interop (P/Invoke like) is high priority.
### 7. LLVM Specifics
    *   Runtime Implementation: Mix of C++ and Sharpie (once bootstrapped).

## Initial Full Goal Outline

Based on the survey, Sharpie aims to be a modern, statically-typed, object-oriented language with strong functional influences, closely resembling C# in its core syntax and features. Key characteristics include:

*   **C#-like Core:** Familiar syntax for classes, structs, interfaces, properties, methods, control flow statements, and expressions.
*   **Robust Type System:** Static typing with nullable reference types, reified generics, primitives as structs (with auto-boxing), and support for operator overloading and extension methods.
*   **Memory Management:** Automatic Reference Counting (ARC) with weak references for cycle handling.
*   **Concurrency:** Async/Await model.
*   **Interoperability:** Strong C/C++ interoperability.
*   **Standard Library:** A foundational set of collections (List, Dictionary), rich string manipulation, and console I/O as a starting point, with plans for expansion.
*   **LLVM Backend:** Leveraging LLVM for compilation to native code.

## Development Plan: Iterative Sweeps

The development of Sharpie will proceed in iterative "Sweeps." Each sweep is designed as a **functional vertical slice**, meaning it will touch upon all necessary components (lexer, parser, AST, semantic analysis, IR generation, runtime) to deliver a demonstrable and testable increment of language functionality. This approach allows for earlier feedback and a more manageable development process.

### Foundational Work (Prerequisite for Sweep 1)

*Goal: Establish the absolute groundwork before iterative development begins.*

1.  **Solidify Core Design & Spec (Initial Draft):**
    *   Document the precise semantics for features in Sweep 1 based on survey choices.
    *   Create a working document for the language specification.
2.  **Formal Grammar (Sweep 1 Features):**
    *   Define/refine the grammar (e.g., ANTLR, LBNF) specifically for the syntax targeted in Sweep 1.
3.  **Core Type System & Memory Management Design:**
    *   Detail the representation of primitive structs and the basic object model for ARC (header layout for ref count).
    *   Define signatures for core ARC runtime functions (`allocate`, `retain`, `release`).
4.  **LLVM IR Mapping Strategy (Sweep 1 Features):**
    *   Outline how the initial set of language constructs will map to LLVM IR.

---

### Sweep 1: Minimal Viable Language Core

*Goal: Implement the bare essentials to write and execute very simple programs, focusing on core syntax, ARC, basic types, and function calls. This establishes the end-to-end pipeline.*

1.  **Lexer & Parser (Minimal):**
    *   **Tokens:** Keywords (`namespace`, `class`, `public`, `static`, `void`, `int`, `string`, `bool`, `return`, `new`, `if`, `true`, `false`, `extern`), basic operators (`=`, `+`, `==`, `()`, `{}`), identifiers, integer/string/bool literals, semicolon.
    *   **AST Nodes:** `CompilationUnit`, `NamespaceDeclaration`, `ClassDeclaration` (empty body initially), `MethodDeclaration` (static only, simple params), `ExternalMethodDeclaration`, `ReturnStatement`, `LocalVariableDeclarationStatement` (`int`, `bool`, `string`), `ExpressionStatement`, `LiteralExpression`, `IdentifierExpression`, `MethodCallExpression` (static/extern only), `AssignmentExpression`, `BinaryExpression` (`+` for strings, `==` for bool/int).
    *   **Parser Rules:** For the above minimal set.
2.  **Semantic Analysis (Minimal):**
    *   **Symbol Table:** Basic global and class-level scope for static methods/externs. Local scope for variables.
    *   **Declaration Processing:** Register functions and variables.
    *   **Type Checking:** For `int`, `bool`, `string` in assignments, returns, parameters, and basic expressions.
3.  **LLVM IR Generation (Minimal):**
    *   **Functions:** For static methods and externs.
    *   **Variables:** `alloca` for local variables.
    *   **Control Flow:** Basic `if` (no `else` yet).
    *   **Expressions:** Literals, variable load/store, static/extern calls, string concatenation (via runtime), basic `int`/`bool` operations.
    *   **ARC (String only initially):**
        *   Calls to `Mycelium_String_new_from_literal`, `Mycelium_String_concat`, `Mycelium_String_delete` (manual calls in generated code for now, or very basic compiler-inserted calls for local string variables).
4.  **Runtime Library (Minimal ARC & String):**
    *   **`MyceliumString`:** Ensure existing `Mycelium_String_new_from_literal`, `Mycelium_String_concat`, `Mycelium_String_print`, `Mycelium_String_delete` are robust.
    *   **Runtime Bindings:** For the above string functions and any `extern` print functions.
5.  **C/C++ Interop (Basic):**
    *   Support for `extern` function declarations and calls (e.g., `extern void print(string s);`).
6.  **Testing:**
    *   Write simple Sharpie programs that declare variables, call extern print functions, use basic `if`, and perform string concatenation. Test JIT execution.

---

### Sweep 2: Core Object-Oriented Features & ARC Expansion

*Goal: Introduce classes with members, constructors, `this` pointer, basic inheritance, and expand ARC to cover custom objects. Implement more control flow.*

1.  **Lexer & Parser (OO Core):**
    *   **Tokens:** `struct`, `interface`, `private`, `this`, `get`, `set`, more operators (`-`, `*`, `/`, `%`, `!=`, `<`, `>`, `<=`, `>=`).
    *   **AST Nodes:** `FieldDeclaration`, `PropertyDeclaration` (simple auto-properties), `ConstructorDeclaration`, `ThisExpression`, `MemberAccessExpression` (fields/methods), `IfStatement` (with `else`), `WhileStatement`. Update `ClassDeclaration` to include fields, properties, methods, constructors. `StructDeclaration`, `InterfaceDeclaration` (method signatures only).
    *   **Parser Rules:** For class/struct/interface members, `this`, properties, `while`, `if-else`.
2.  **Semantic Analysis (OO Core):**
    *   **Symbol Table:** Class scopes, `this` keyword resolution.
    *   **Declaration Processing:** Fields, properties, constructors, methods within classes/structs. Interface method signatures.
    *   **Type Checking:** Member access, constructor calls, `this` usage. Type checking for more operators.
    *   **Access Modifiers:** `public`, `private` for members.
    *   **Inheritance (Single Class, Multiple Interfaces - Basic):** Check for method overrides (basic, no `virtual` keyword yet), interface implementation (method signature matching).
3.  **LLVM IR Generation (OO Core):**
    *   **Types:** LLVM struct types for classes/structs.
    *   **Object Allocation:** `new ClassName()` calls runtime allocator.
    *   **Constructors:** Generate IR for constructors, `this` pointer handling.
    *   **Members:** IR for field access (GEP), method calls (static dispatch for now, or direct if non-virtual). Property getter/setter IR.
    *   **ARC (Custom Objects):**
        *   Compiler inserts `retain`/`release` calls for class instances (assignments, params, returns, scope end).
        *   `new` expressions call runtime allocator which initializes ref count.
    *   **Control Flow:** `if-else`, `while`.
    *   **Boxing/Unboxing (Primitives):** Implement for `int`, `bool` when assigned to a generic `object` type (if `object` base class is introduced).
4.  **Runtime Library (Object Model & ARC):**
    *   **Core Object Model:** Define `MyceliumObject` header (ref_count, type_id/vtable_ptr).
    *   **ARC Implementation:** `Mycelium_Object_alloc(size, type_id)`, `Mycelium_Object_retain(obj)`, `Mycelium_Object_release(obj)`. `Mycelium_Object_release` should handle calling destructors (placeholder for now) and freeing memory.
    *   **Type Information:** Basic RTTI mechanism to store type IDs.
    *   **Primitive Structs:** Define `System.Int32`, `System.Boolean` etc. as structs in runtime if methods like `ToString()` are needed.
5.  **Standard Library (Primitives):**
    *   Implement `ToString()` methods for primitive types (e.g., `int.ToString()`).
6.  **Testing:**
    *   Programs with classes, objects, fields, methods, constructors. Test ARC for these objects. Test `while` loops, `if-else`.

---

### Sweep 3: Advanced Types, Generics (Initial), and Core Usability

*Goal: Introduce reified generics (for collections), delegates, events, operator overloading, extension methods, and more robust standard library collections.*

1.  **Lexer & Parser (Advanced Types):**
    *   **Tokens:** `enum`, `delegate`, `event`, `operator`, generic syntax (`<`, `>`), `ref`, `out`, `weak`.
    *   **AST Nodes:** `EnumDeclaration`, `DelegateDeclaration`, `EventDeclaration`, `OperatorDeclaration`, `ExtensionMethodDeclaration` (syntax marker), `GenericTypeParameterNode`, `GenericTypeArgumentNode`. Update `MethodDeclaration`, `ClassDeclaration`, `InterfaceDeclaration`, `StructDeclaration` for generics. `WeakReferenceNode` (conceptual).
    *   **Parser Rules:** For generics in type/method declarations and usage. Delegate, event, operator syntax.
2.  **Semantic Analysis (Advanced Types):**
    *   **Generics (Reified - Phase 1):** Type parameterization, basic constraint checking (e.g., `where T : class`), type argument deduction for methods.
    *   **Delegates & Events:** Type checking for signatures, assignments, `+=`, `-=`.
    *   **Operator Overloading:** Resolution and validation.
    *   **Extension Methods:** Resolution (static binding).
    *   **Enums:** Processing and type checking.
    *   **Weak References:** Semantic rules for `weak` keyword.
3.  **LLVM IR Generation (Advanced Types):**
    *   **Generics (Reified - Phase 1):** Monomorphization for simple cases or pass type information to runtime for generic collections.
    *   **Delegates:** Represent as object+function pointer pair. IR for invocation, `+=`, `-=`.
    *   **Events:** Backed by delegates, IR for add/remove accessors.
    *   **Operator Overloading:** Generate calls to static operator methods.
    *   **Extension Methods:** Generate static calls.
    *   **Weak References:** Calls to runtime weak reference functions.
4.  **Runtime Library (Advanced Support):**
    *   **Generics:** Runtime support for type information for reified generic types/methods.
    *   **Weak References:** Implement `Mycelium_WeakRef_create`, `Mycelium_WeakRef_get`, `Mycelium_WeakRef_destroy`, `Mycelium_WeakRef_assign`.
    *   **Delegates:** Helper functions if needed for delegate combination/removal.
5.  **Standard Library (Core Generic Collections):**
    *   **`List<T>`:** Implement using reified generics. Methods: `Add`, `Remove`, `Count`, indexer.
    *   **`Dictionary<K,V>`:** Implement. Methods: `Add`, `Remove`, `ContainsKey`, indexer.
    *   Ensure collections work correctly with ARC for their elements.
6.  **Testing:**
    *   Programs using generic `List<T>` and `Dictionary<K,V>`. Test delegates, events, operator overloading, and extension methods. Test weak references.

---

### Sweep 4: Concurrency, Error Handling, and Further Standard Library

*Goal: Implement async/await, a chosen error handling mechanism (even if basic initially), and expand I/O and reflection capabilities.*

1.  **Lexer & Parser:**
    *   **Tokens:** `async`, `await`, `try`, `catch`, `finally`, `throw`.
    *   **AST Nodes:** `AsyncModifierNode`, `AwaitExpressionNode`, `TryCatchStatementNode`, `ThrowStatementNode`.
2.  **Semantic Analysis:**
    *   **Async/Await:** Transform async methods into state machines (design phase, may not be full IR gen yet).
    *   **Error Handling (e.g., Exceptions):** Semantic checks for `try-catch-finally`, `throw`. Type matching for `catch` clauses.
3.  **LLVM IR Generation:**
    *   **Async/Await (Initial):** Generate state machine logic.
    *   **Error Handling (e.g., Exceptions):** Utilize LLVM's exception handling (invoke, landingpad) or chosen mechanism.
4.  **Runtime Library:**
    *   **Async/Await:** Task scheduling, synchronization primitives (e.g., event loop, thread pool integration).
    *   **Error Handling:** Runtime support for throwing/catching exceptions (e.g., personality routines, exception object management).
5.  **Standard Library:**
    *   **File I/O:** `File.ReadAllText()`, `File.WriteAllText()`, basic stream operations.
    *   **Reflection (Basic):** `object.GetType()`, `Type.Name`, `Type.FullName`.
6.  **Testing:**
    *   Async operations, exception handling tests, file I/O tests, basic reflection tests.

---

### Subsequent Sweeps (Future Goals - Higher Level)

*These will be broken down further as earlier sweeps complete.*

*   **Sweep 5: Advanced Language Features & Polish**
    *   Mixins/Traits, Default Interface Methods (if design solidifies).
    *   LINQ-like features (query syntax, core Enumerable methods).
    *   Full Reflection capabilities.
    *   Refine error handling mechanisms based on initial feedback and brainstorming.
    *   Compile-time cycle detection research and potential PoC.
    *   `unsafe` contexts and manual memory management features.
*   **Sweep 6: Tooling, Ecosystem & Performance**
    *   Advanced Debugging Support (rich DWARF, expression evaluation).
    *   Language Server Protocol (LSP) for IDEs.
    *   Package Manager.
    *   Advanced LLVM Optimizations, language-specific optimizations.
    *   Performance benchmarking and tuning.
*   **Sweep 7: Broader Standard Library & Finalization**
    *   Networking library.
    *   Date/Time library.
    *   More collections (`Set<T>`, `Queue<T>`, `Stack<T>`).
    *   Comprehensive language specification and user documentation.

This iterative plan provides a structured approach to building Sharpie, ensuring that foundational elements are in place before tackling more complex features. Each sweep delivers tangible progress and allows for adjustments based on learnings.
