# Known Issues in Myre Compiler

This document tracks critical issues identified during comprehensive testing of the member function implementation.

## Critical Issues

### 1. Member Function Call Resolution Issues
**Status**: 🔴 Critical  
**Error Messages**:
```
Error: Function 'isAlive' not found
Error: Function 'getHealth' not found  
Error: Function 'castSpell' not found
```

**Description**: Member functions calling other member functions within the same type are not being resolved correctly. The code generation shows these functions exist, but the lookup mechanism fails during execution.

**Example**:
```cpp
type GameCharacter {
    fn isAlive(): bool { return health > 0; }
    
    fn heal(i32 amount): i32 {
        if (isAlive()) {  // ❌ Function 'isAlive' not found
            health = health + amount;
        }
        return getHealth();  // ❌ Function 'getHealth' not found
    }
}
```

**Impact**: Prevents object-oriented programming patterns where methods call other methods on the same object.

---

### 2. Variable Declaration Type Inference Failures
**Status**: 🔴 Critical  
**Error Messages**:
```
[ERROR] Failed to resolve type for symbol: damage
[ERROR] Failed to resolve type for symbol: i  
[ERROR] Failed to resolve type for symbol: transactions
```

**Description**: Variables declared within member functions (especially in complex control flow) cannot have their types inferred by the semantic analyzer.

**Example**:
```cpp
fn combatRound(): i32 {
    var damage = 0;  // ❌ Cannot infer type for symbol: damage
    
    if (condition) {
        var result = someFunction();  // ❌ Type inference fails
    }
    
    return damage;
}
```

**Impact**: Prevents use of local variables in member functions, severely limiting functionality.

---

### 3. Circular Dependency Detection False Positives
**Status**: 🟡 High Priority  
**Error Message**:
```
[ERROR] Type resolution exceeded maximum iterations - possible circular dependencies
```

**Description**: The semantic analyzer incorrectly detects circular dependencies when member functions call each other, causing type resolution to fail prematurely.

**Example**:
```cpp
type Character {
    fn heal(): i32 { return getHealth(); }     // These mutual calls
    fn getHealth(): i32 { return health; }    // trigger false positive
}
```

**Impact**: Blocks compilation of legitimate inter-method call patterns.

---

### 4. Basic Block Termination Issues
**Status**: 🔴 Critical  
**Error Message**:
```
Module verification failed:
Basic Block in function 'GameCharacter::heal' does not have terminator!
label %GameCharacter__heal_if_end_0
```

**Description**: Some basic blocks are missing terminators, particularly in functions with complex control flow and member function calls.

**Impact**: Causes LLVM module verification to fail, preventing code execution.

---

### 5. Parser Expression Errors
**Status**: 🟡 High Priority  
**Error Messages**:
```
[ERROR] Error at offset 1350: Expected expression
[ERROR] Error at offset 2779: Expected expression
```

**Description**: The parser fails to handle certain complex expression patterns, particularly in for loops or complex member function calls.

**Impact**: Limits the complexity of code that can be written, especially iterative patterns.

---

### 6. For Loop Support Issues
**Status**: 🟡 High Priority  

**Description**: For loops have multiple problems:
- Variable initialization in for loop headers fails type resolution
- Loop variable scope management is broken
- Loop body execution with member function calls fails

**Example**:
```cpp
fn processItems(): i32 {
    for (var i = 0; i < 10; i = i + 1) {  // ❌ Parser/type errors
        // Loop body with member calls fails
    }
}
```

**Impact**: Prevents iterative algorithms and bulk processing operations.

---

### 7. Inter-Member Function Call Context Issues
**Status**: 🔴 Critical  

**Description**: When a member function calls another member function of the same type, the context management fails to properly resolve the target function. The `this` parameter context is not properly maintained across function call boundaries.

**Technical Details**:
- Function lookup uses qualified names like "Type::method"
- Within member functions, unqualified calls like `otherMethod()` should resolve to "Type::otherMethod"
- Current implementation loses the type context

**Impact**: Breaks encapsulation and object-oriented design patterns.

---

### 8. Value Reference Management
**Status**: 🟡 Medium Priority  
**Error Messages**:
```
Value with ID 23 not found
Value with ID 44 not found
```

**Description**: The IR generation system loses track of value references, particularly when member functions are called from within other member functions.

**Impact**: Causes runtime errors and unpredictable behavior in complex member function interactions.

---

## What Currently Works ✅

- **Simple member functions** with basic logic and field access
- **Member functions that don't call other member functions**
- **Basic if statements** within member functions  
- **Type inference for member function calls from main()** 
- **Struct field access and modification**
- **Member function calls from external functions**
- **Simple control flow** without inter-method calls

## What's Broken ❌

- **Member functions calling other member functions** within the same type
- **Complex variable declarations** within member functions
- **For loops** in any context
- **Nested control flow** with member function calls
- **Advanced type inference** scenarios
- **Complex expression parsing**

## Priority Ranking

### 🔴 Critical (Blocks Core Functionality)
1. **Member-to-member function calls** - Fundamental for OOP
2. **Variable type inference in member functions** - Prevents local variables
3. **Basic block termination** - Causes LLVM verification failures

### 🟡 High Priority (Limits Language Features)
4. **For loop support** - Important for iteration patterns
5. **Circular dependency detection** - Too aggressive currently
6. **Parser expression handling** - Limits code complexity

### 🟢 Medium Priority (Quality of Life)
7. **Value reference cleanup** - Optimization issue
8. **Error message improvements** - Better debugging experience

## Test Cases

### Working Test Case
```cpp
// ✅ This works
type Simple {
    var value = 42;
    fn getValue(): i32 { return value; }
}

fn main(): i32 {
    var obj = new Simple();
    return obj.getValue();  // Works fine
}
```

### Failing Test Case  
```cpp
// ❌ This fails
type Complex {
    var health = 100;
    
    fn isAlive(): bool { return health > 0; }
    
    fn heal(): i32 {
        if (isAlive()) {  // Function not found
            var amount = 10;  // Type inference fails
            health = health + amount;
        }
        return health;
    }
}
```

## Notes for Developers

- The member function implementation is **partially functional** but needs significant work for production use
- Focus should be on fixing the **function lookup mechanism** within member function contexts
- The **semantic analyzer's type resolution** needs to handle member function scoping better
- **Basic block management** in the code generator needs review for complex control flow
- Consider implementing a **method resolution table** for each type to handle inter-method calls

---

*Last Updated: 2025-01-23*  
*Compiler Version: Development*  
*Test Files: `test_comprehensive_final.myre`, `test_working_comprehensive.myre`*