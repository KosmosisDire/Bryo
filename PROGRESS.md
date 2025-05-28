# Sharpie Language - Object Model & ARC Implementation Progress

This document tracks the progress of implementing the object model and Automatic Reference Counting (ARC) for memory management in the Sharpie language.

## Current State (As of May 26, 2025, ~9:45 PM ET)

### Core Infrastructure:
- **LLVM Types:**
    - `MyceliumString` struct type defined and used for string operations.
    - `MyceliumObjectHeader` struct type (`{ref_count, type_id}`) defined.
- **Runtime Functions (C++ `mycelium_runtime`):**
    - `Mycelium_Object_alloc(size_t data_size, uint32_t type_id)`: Allocates memory for `MyceliumObjectHeader` + data, initializes header (ref_count=1, type_id), and returns `MyceliumObjectHeader*`.
    - `Mycelium_Object_retain(MyceliumObjectHeader* obj)`: Increments `ref_count`.
    - `Mycelium_Object_release(MyceliumObjectHeader* obj)`: Decrements `ref_count`. (Full deallocation logic when ref_count hits 0 is pending).
- **Compiler (`ScriptCompiler`):**
    - **Class Registration:**
        - `visit(ClassDeclarationNode)` collects field types, names, and indices.
        - Creates an LLVM struct type for class fields (e.g., `%MyClass_FieldsType`).
        - Stores `ClassTypeInfo` (name, type_id, fieldsType, field_names_in_order, field_indices) in `classTypeRegistry`.
    - **Object Creation (`ObjectCreationExpressionNode`):**
        - Looks up `ClassTypeInfo`.
        - Calculates size of the fields struct.
        - Calls runtime `Mycelium_Object_alloc` to get a pointer to the `MyceliumObjectHeader`.
        - Derives a pointer to the fields struct (e.g., `%MyClass_FieldsType*`) from the header pointer using GEP.
        - **Calls the appropriate constructor**, passing the fields pointer as `this` and any arguments.
    - **ARC Helper:**
        - `getHeaderPtrFromFieldsPtr(llvm::Value* fieldsPtr, llvm::StructType* fieldsLLVMType)`: Implemented.
    - **ARC for Local Variables (`LocalVariableDeclarationStatementNode`):**
        - When a local variable of a class type is initialized, the compiler tracks its `header_ptr` in `current_function_arc_locals`.
    - **ARC for Assignments (`AssignmentExpressionNode`):**
        - Handles assignments to local variables and fields.
        - **Corrected order:** Retains new value *before* releasing old value to handle self-assignment.
        - **Null checks implemented** before retain/release calls.
    - **Scope Exit for ARC:**
        - In `visit_method_declaration` and `visit(ReturnStatementNode)`, before any `ret` instruction, iterates through `current_function_arc_locals` and inserts calls to `Mycelium_Object_release`.
    - **Field Access (`MemberAccessExpressionNode`):**
        - Implemented for reading fields (GEP and load).
    - **`this` Pointer:**
        - Instance methods implicitly receive `this` (pointer to `FieldsType`).
        - `visit_method_declaration` adapted for `this`.
        - `visit(ThisExpressionNode)` resolves to the `this` pointer.
        - `visit(MethodCallExpressionNode)` passes `this` for instance calls.
    - **Constructors (`ConstructorDeclarationNode`):**
        - `visit(ConstructorDeclarationNode)` implemented to generate LLVM IR for constructors, including `this` pointer and void return.

### Known Issues / Pending Fixes:
- (None currently identified from the last set of changes, pending testing)

## Remaining Tasks (For this ARC/Object Model Slice - Sweep 2 Focus)

1.  **~~Fix C++ Compilation Errors:~~** (Completed)
    *   ~~Correct all instances of `someType->getPointerElementType()` to `static_cast<llvm::PointerType*>(someType)->getElementType()` or similar, where `someType` is an `llvm::Type*` that is known to be a pointer.~~ (Resolved by using `getContainedType(0)`)

2.  **~~Refine ARC Logic - Assignments:~~** (Largely Completed)
    *   **~~Null Checks:~~** ~~Implement robust null checks before calling `Mycelium_Object_retain` or `Mycelium_Object_release` on object pointers (both old and new values in an assignment).~~ (Completed)
    *   **~~Self-Assignment:~~** ~~Consider if `target = target;` needs special handling to avoid premature release if the naive release-then-retain is used.~~ (Completed by reordering retain/release)

3.  **Refine ARC Logic - Scope Management & Local Variables:**
    *   **Robust Scope Exit:** The current naive release at function/return exit is insufficient for nested scopes or complex control flow. Explore strategies for more precise `release` calls at the end of actual variable scopes (e.g., end of a block). This might involve tracking scopes more explicitly or using LLVM's lifetime intrinsics if applicable. (Partially improved, but true lexical scope cleanup is pending).
    *   Ensure `current_function_arc_locals` is consistently and correctly updated for all relevant operations (declarations, assignments, potentially parameter passing). (Ongoing review needed as more features are added).

4.  **Implement Field Access (`MemberAccessExpressionNode`):**
    *   **~~GEP for Fields:~~** ~~When accessing `obj.field`:~~ (Completed for read)
        - ~~Load the `fields_ptr` for `obj`.~~
        - ~~Determine the index of `field` within the class's `FieldsType` struct (requires storing field layout/indices in `ClassTypeInfo`).~~
        - ~~Use `GetElementPtrInst` (GEP) to get a pointer to the specific field.~~
    *   **~~Load/Store:~~** ~~Perform `load` or `store` operations on the field pointer.~~ (Load completed, Store is part of Assignment)
    *   **~~ARC for Field Assignments:~~** ~~If a field is of a class type:~~ (Completed within AssignmentExpressionNode)
        - ~~When assigning to `obj.field = new_value;`:~~
            - ~~Release the old object `obj.field` was pointing to (if any, and not null).~~
            - ~~Retain `new_value` (if not null).~~
            - ~~Store `new_value` into the field.~~

5.  **~~Implement `this` Pointer for Instance Methods:~~** (Completed)
    *   **~~Method Signature:~~** ~~Instance methods should implicitly receive a pointer to the object's `FieldsType` struct as their first argument (conventionally named `this`).~~
    *   **~~`visit_method_declaration`~~:** ~~Adapt to add the `this` parameter for instance methods. Store it in `namedValues`.~~
    *   **~~`MethodCallExpressionNode`~~:** ~~When calling an instance method (`obj.method()`), pass `obj` (the `fields_ptr`) as the first argument.~~
    *   **~~`ThisExpressionNode`~~:** ~~Should resolve to the `this` pointer (the `fields_ptr`) available in the current instance method's scope.~~

6.  **~~Constructors (`ConstructorDeclarationNode` & `ObjectCreationExpressionNode`):~~** (Completed)
    *   **~~`visit_constructor_declaration`~~:**
        - ~~Similar to method declaration but with no return type.~~
        - ~~Implicitly receives `this` (the `fields_ptr`).~~
        - ~~Responsible for initializing fields.~~
    *   **~~`ObjectCreationExpressionNode` (`new`)~~:**
        - ~~After `Mycelium_Object_alloc` and getting the `fields_ptr`.~~
        - ~~Generate a call to the appropriate constructor, passing the `fields_ptr` as `this`, along with any arguments from the `new` expression.~~

7.  **Runtime Deallocation Logic (`Mycelium_Object_release`):**
    *   When `ref_count` reaches 0 in `Mycelium_Object_release`:
        - **Call Destructor:** Invoke the object's specific destructor (if one exists, identified by `type_id`). The destructor is responsible for user-defined cleanup and the compiler ensures it also releases managed fields. (See Task 9 for destructor implementation details).
        - **Free Memory:** After the destructor has run, `free()` the memory for the object (header and fields).

8.  **Testing:** (Partially completed - initial test script created)
    *   Create comprehensive `test.sp` scripts to cover:
        - Object creation and assignment.
        - ARC behavior for local variables going in and out of scope.
        - ARC behavior for assignments (old object released, new object retained).
        - Basic field access (read/write) for primitive types.
        - Field access for object types with correct ARC.
        - Instance method calls using `this`.
        - Constructor calls.
    *   **Update `main.cpp` to run the new test script and handle its return type.** (Completed)

9.  **Implement Destructors:**
    *   **Syntax & AST (`script_ast.hpp`, `script_parser.hpp`):**
        *   Define syntax: `~ClassName() { /* body */ }`.
        *   Add `DestructorDeclarationNode` to AST.
        *   Update parser to handle destructor syntax.
    *   **Semantic Analysis (`ScriptCompiler`):**
        *   Register destructor in `ClassTypeInfo`.
        *   Validate destructor (parameterless, no return type, name matches class).
    *   **LLVM IR Generation (`ScriptCompiler`):**
        *   Generate LLVM function for destructor body (e.g., `ClassName.%dtor`).
        *   Implicitly release ARC-managed instance fields at the end of the destructor.
    *   **Runtime Support (`mycelium_runtime.cpp`, `mycelium_runtime.h`, `runtime_binding.h`):**
        *   Implement a mechanism for `Mycelium_Object_release` to call the correct destructor based on `type_id` before freeing memory. This could involve a destructor function table or similar.
        *   The generated LLVM destructor functions need to be callable by this runtime mechanism.
    *   **Compiler Integration:**
        *   Ensure the compiler registers generated destructor functions with the runtime's invocation mechanism.

This provides a clearer path forward for the immediate next steps.
