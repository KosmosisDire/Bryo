# Sharpie Language Development Plan Checklist

## Introduction

This document outlines the development plan for Sharpie, a C#-like language built on LLVM. It begins with an overview of the language's target features based on your survey responses, followed by an iterative development plan structured into "Sweeps." Each sweep aims to deliver a functional vertical slice of the language, prioritizing core functionality and foundational features.
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

### Sweep 1: Minimal Viable Language Core

*Goal: Implement the bare essentials to write and execute very simple programs, focusing on core syntax, ARC, basic types, and function calls. This establishes the end-to-end pipeline.*

1.  [x] **Lexer & Parser (Minimal):**
    *   [x] **Tokens:** Keywords (`namespace`, `class`, `public`, `static`, `void`, `int`, `string`, `bool`, `return`, `new`, `if`, `true`, `false`, `extern`), basic operators (`=`, `+`, `==`, `()`, `{}`), identifiers, integer/string/bool literals, semicolon.
    *   [x] **AST Nodes:** `CompilationUnit`, `NamespaceDeclaration`, `ClassDeclaration` (empty body initially), `MethodDeclaration` (static only, simple params), `ExternalMethodDeclaration`, `ReturnStatement`, `LocalVariableDeclarationStatement` (`int`, `bool`, `string`), `ExpressionStatement`, `LiteralExpression`, `IdentifierExpression`, `MethodCallExpression` (static/extern only), `AssignmentExpression`, `BinaryExpression` (`+` for strings, `==` for bool/int).
    *   [x] **Parser Rules:** For the above minimal set.
2.  [x] **Semantic Analysis (Minimal):**
    *   [x] **Symbol Table:** Basic global and class-level scope for static methods/externs. Local scope for variables.
    *   [x] **Declaration Processing:** Register functions and variables.
    *   [x] **Type Checking:** For `int`, `bool`, `string` in assignments, returns, parameters, and basic expressions.
3.  [x] **LLVM IR Generation (Minimal):**
    *   [x] **Functions:** For static methods and externs.
    *   [x] **Variables:** `alloca` for local variables.
    *   [x] **Control Flow:** Basic `if` (no `else` yet).
    *   [x] **Expressions:** Literals, variable load/store, static/extern calls, string concatenation (via runtime), basic `int`/`bool` operations.
    *   [x] **ARC (String only initially):**
        *   [x] Calls to `Mycelium_String_new_from_literal`, `Mycelium_String_concat`, `Mycelium_String_delete` (manual calls in generated code for now, or very basic compiler-inserted calls for local string variables).
4.  [x] **Runtime Library (Minimal ARC & String):**
    *   [x] **`MyceliumString`:** Ensure existing `Mycelium_String_new_from_literal`, `Mycelium_String_concat`, `Mycelium_String_print`, `Mycelium_String_delete` are robust.
    *   [x] **Runtime Bindings:** For the above string functions and any `extern` print functions.
5.  [x] **C/C++ Interop (Basic):**
    *   [x] Support for `extern` function declarations and calls (e.g., `extern void print(string s);`).
6.  [x] **Testing:**
    *   [x] Write simple Sharpie programs that declare variables, call extern print functions, use basic `if`, and perform string concatenation. Test JIT execution.

---

### Sweep 2: Core Object-Oriented Features & ARC Expansion

*Goal: Introduce classes with members, constructors, `this` pointer, basic inheritance, and expand ARC to cover custom objects. Implement more control flow.*

1.  **Lexer & Parser (OO Core):**
    *   **Tokens:**
        *   [x] Keywords (`struct`, `private`, `this`)
        *   [ ] Keywords (`interface`, `get`, `set`)
        *   [x] More operators (`-`, `*`, `/`, `%`, `!=`, `<`, `>`, `<=`, `>=`)
    *   **AST Nodes:**
        *   [x] `FieldDeclarationNode`
        *   [ ] `PropertyDeclaration` (simple auto-properties)
        *   [x] `ConstructorDeclarationNode`
        *   [x] `ThisExpressionNode`
        *   [x] `MemberAccessExpressionNode` (fields/methods)
        *   [x] `IfStatementNode` (with `else`)
        *   [x] `WhileStatementNode`
        *   [x] Update `ClassDeclarationNode` to include fields, methods, constructors
        *   [x] `StructDeclarationNode` (AST node exists)
        *   [ ] `InterfaceDeclaration` (method signatures only)
    *   **Parser Rules:**
        *   [x] For class members (fields, methods, constructors)
        *   [ ] For `struct` declarations
        *   [ ] For `interface` declarations
        *   [x] For `this`
        *   [ ] For properties
        *   [x] For `while`, `if-else`

2.  **Semantic Analysis (OO Core):**
    *   **Symbol Table:**
        *   [x] Basic global and class-level scope for static methods/externs. Local scope for variables.
        *   [x] `this` keyword resolution.
    *   **Declaration Processing:**
        *   [x] Fields, constructors, methods within classes.
        *   [ ] Properties, structs, interface method signatures.
    *   **Type Checking:**
        *   [x] Member access, constructor calls, `this` usage.
        *   [x] Type checking for more operators.
    *   **Access Modifiers:**
        *   [ ] Enforcement of `public`, `private` for members.
    *   **Inheritance (Single Class, Multiple Interfaces - Basic):**
        *   [ ] Check for method overrides (basic, no `virtual` keyword yet), interface implementation (method signature matching).

3.  **LLVM IR Generation (OO Core):**
    *   **Types:**
        *   [x] LLVM struct types for classes.
        *   [ ] LLVM struct types for structs.
    *   **Object Allocation:**
        *   [x] `new ClassName()` calls runtime allocator.
    *   **Constructors:**
        *   [x] Generate IR for constructors, `this` pointer handling.
    *   **Members:**
        *   [x] IR for field access (GEP), method calls (static dispatch for now, or direct if non-virtual).
        *   [ ] Property getter/setter IR.
    *   **ARC (Custom Objects):**
        *   [x] Compiler inserts `retain`/`release` calls for class instances (e.g., upon assignment, passing/returning, and critically at **scope end**).
        *   [x] `new` expressions call runtime allocator which initializes ref count.
    *   **Control Flow:**
        *   [x] Basic `if-else`.
        *   [ ] `while` loop. (Parser rule exists, but `visit` method is missing in compiler_visitors.cpp)
    *   **Boxing/Unboxing (Primitives):**
        *   [ ] Implement for `int`, `bool` when assigned to a generic `object` type (if `object` base class is introduced).

4.  **Runtime Library (Object Model & ARC):**
    *   **Core Object Model:**
        *   [x] Define `MyceliumObjectHeader` (ref_count, type_id).
        *   [ ] Add `vtable_ptr` to `MyceliumObjectHeader`.
    *   **ARC Implementation:**
        *   [x] `Mycelium_Object_alloc(size, type_id)`, `Mycelium_Object_retain(obj)`, `Mycelium_Object_release(obj)`.
        *   [ ] `Mycelium_Object_release` handles calling destructors.
    *   **Type Information:**
        *   [x] Basic RTTI mechanism to store type IDs.
    *   **Primitive Structs:**
        *   [ ] Define `System.Int32`, `System.Boolean` etc. as structs in runtime.

5.  **Standard Library (Primitives):**
    *   [x] Implement `ToString()` methods for primitive types (e.g., `int.ToString()`).

6.  **Testing:**
    *   [x] Write simple Sharpie programs that declare variables, call extern print functions, use basic `if`, and perform string concatenation. Test JIT execution.
    *   [x] Programs with classes, objects, fields, methods, constructors. Test ARC for these objects.
    *   [ ] Test `while` loops, `if-else`. (Partial, as `while` is not fully implemented in compiler)

---

### Sweep 3: Advanced Types, Generics (Initial), and Core Usability

*Goal: Introduce reified generics (for collections), delegates, events, operator overloading, extension methods, and more robust standard library collections.*

1.  **Lexer & Parser (Advanced Types):**
    *   **Tokens:**
        *   [ ] `enum`, `delegate`, `event`, `operator`, `ref`, `out`, `weak`
        *   [x] Generic syntax (`<`, `>`) (for type names and method calls)
    *   **AST Nodes:**
        *   [ ] `EnumDeclaration`, `DelegateDeclaration`, `EventDeclaration`, `OperatorDeclaration`, `ExtensionMethodDeclaration` (syntax marker), `WeakReferenceNode` (conceptual)
        *   [x] `GenericTypeParameterNode` (`TypeParameterNode` exists)
        *   [x] `GenericTypeArgumentNode` (handled by `TypeNameNode::typeArguments`)
        *   [x] `MethodDeclaration`, `ClassDeclaration`, `StructDeclaration` updated for generics (they have `typeParameters` fields)
        *   [ ] `InterfaceDeclaration` updated for generics (InterfaceDeclarationNode itself is missing)
    *   **Parser Rules:**
        *   [x] For generics in type usage (`parse_type_name`, generic method calls)
        *   [ ] For generics in method/class/struct declarations (TODO comments exist)
        *   [ ] For delegate, event, operator syntax
        *   [ ] For `ref`, `out`, `weak` keywords

2.  **Semantic Analysis (Advanced Types):**
    *   **Generics (Reified - Phase 1):**
        *   [ ] Type parameterization, basic constraint checking (e.g., `where T : class`), type argument deduction for methods.
    *   **Delegates & Events:**
        *   [ ] Type checking for signatures, assignments, `+=`, `-=`.
    *   **Operator Overloading:**
        *   [ ] Resolution and validation.
    *   **Extension Methods:**
        *   [ ] Resolution (static binding).
    *   **Enums:**
        *   [ ] Processing and type checking.
    *   **Weak References:**
        *   [ ] Semantic rules for `weak` keyword.

3.  **LLVM IR Generation (Advanced Types):**
    *   **Generics (Reified - Phase 1):**
        *   [ ] Monomorphization for simple cases or pass type information to runtime for generic collections.
    *   **Delegates:**
        *   [ ] Represent as object+function pointer pair. IR for invocation, `+=`, `-=`.
    *   **Events:**
        *   [ ] Backed by delegates, IR for add/remove accessors.
    *   **Operator Overloading:**
        *   [ ] Generate calls to static operator methods.
    *   **Extension Methods:**
        *   [ ] Generate static calls.
    *   **Weak References:**
        *   [ ] Calls to runtime weak reference functions.

4.  **Runtime Library (Advanced Support):**
    *   **Generics:**
        *   [ ] Runtime support for type information for reified generic types/methods.
    *   **Weak References:**
        *   [ ] Implement `Mycelium_WeakRef_create`, `Mycelium_WeakRef_get`, `Mycelium_WeakRef_destroy`, `Mycelium_WeakRef_assign`.
    *   **Delegates:**
        *   [ ] Helper functions if needed for delegate combination/removal.

5.  **Standard Library (Core Generic Collections):**
    *   [ ] `List<T>`: Implement using reified generics. Methods: `Add`, `Remove`, `Count`, indexer.
    *   [ ] `Dictionary<K,V>`: Implement. Methods: `Add`, `Remove`, `ContainsKey`, indexer.
    *   [ ] Ensure collections work correctly with ARC for their elements.

6.  **Testing:**
    *   [ ] Programs using generic `List<T>` and `Dictionary<K,V>`. Test delegates, events, operator overloading, and extension methods. Test weak references.
