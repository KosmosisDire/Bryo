# Semantic Analysis & Code Generation Plan

## Overview

This document outlines the simplified semantic analysis and code generation system for the Myre compiler. The design emphasizes simplicity, performance, and maintainability while providing clean separation of concerns.

## Core Architecture

The system consists of four main components with clear separation of concerns:
1. **Type Registry** - Manages and interns all type objects for optimal performance
2. **Symbol Table** - Tracks symbols across scopes with O(1) operations  
3. **Error Collector** - Centralized error reporting and management
4. **Compiler Context** - Coordinates all components as single source of truth

## Component Details

### 1. Type Registry (`semantic/type_system.hpp`, `semantic/type_registry.cpp`)

#### Purpose
Manages and interns all type objects to ensure uniqueness and enable O(1) pointer-based type equality comparison.

#### Classes

**`Type` (Abstract Base Class)**
- **Purpose**: Base class for all types in the system
- **Key Members**:
  - `TypeKind kind_` - Enum indicating the specific type
  - `template<typename T> bool is<T>()` - Fast type checking
  - `template<typename T> T* as<T>()` - Safe type casting
- **Virtual Methods**:
  - `std::string to_string()` - String representation
  - `bool equals(const Type& other)` - Type equality comparison
  - `size_t hash()` - Hash for use in containers

**`PrimitiveType : public Type`**
- **Purpose**: Represents built-in types (i32, bool, f64, etc.)
- **Key Members**:
  - `std::string name_` - Type name
- **Main Functions**:
  - Constructor with name only
  - `size()` - Calculated from static lookup table based on name
  - Type comparison by name

**`StructType : public Type`**
- **Purpose**: Represents user-defined struct types with fields and methods
- **Key Members**:
  - `std::string name_` - Struct name
  - `std::vector<Field> fields_` - Field information (offsets calculated lazily)
  - `std::vector<Method> methods_` - Method signatures
  - `mutable std::optional<LayoutInfo> cached_layout_` - Lazy-calculated layout
- **Main Functions**:
  - `add_field(name, type)` - Add field during construction
  - `add_method(name, func_type)` - Add method during construction
  - `find_field(name)` - O(n) linear search (faster for small structs)
  - `find_method(name)` - O(n) linear search (faster for small structs)
  - `size()` - Lazy calculation: computes layout on first access
  - `alignment()` - Lazy calculation: computes layout on first access
  - `field_offset(name)` - Lazy calculation: computes layout on first access

**`FunctionType : public Type`**
- **Purpose**: Represents function signatures
- **Key Members**:
  - `Type* return_type_` - Return type (non-owning pointer)
  - `std::vector<Type*> parameter_types_` - Parameter types
  - `bool is_varargs_` - Variadic function flag
- **Main Functions**:
  - Constructor with return type and parameters
  - String representation: `(param1, param2) -> return_type`

**`PointerType : public Type`**
- **Purpose**: Represents pointer types
- **Key Members**:
  - `Type* pointee_type_` - Type being pointed to
- **Main Functions**:
  - Constructor with pointee type
  - String representation: `pointee_type*`

**`ArrayType : public Type`**
- **Purpose**: Represents array types (fixed-size and dynamic)
- **Key Members**:
  - `Type* element_type_` - Array element type
  - `size_t size_` - Array size (0 for dynamic arrays)
- **Main Functions**:
  - Constructor with element type and size
  - `is_dynamic()` - Check if array is dynamically sized

**`TypeRegistry` (Main Class)**
- **Purpose**: Manages all type objects and ensures uniqueness through interning
- **Key Members**:
  - `std::unordered_map<size_t, std::unique_ptr<Type>> interned_types_` - All unique types
- **Main Functions**:
  - `template<typename T, typename... Args> T* get_or_create(Args&&... args)` - Get/create unique type
  - `get_primitive(name)` - Find primitive by linear search through interned types
  - `create_struct(name)` - Create new struct type with interning
  - `create_function(return_type, params)` - Create function type with interning
  - `create_pointer(pointee)` - Create pointer type with interning
  - `create_array(element, size)` - Create array type with interning

#### Integration Points
- **With Parser**: Parser requests types from registry during type resolution
- **With Symbol Table**: Symbol table references types from registry (non-owning pointers)
- **With Code Generator**: Code generator queries type information for LLVM IR generation

#### Performance Characteristics
- Type equality: O(1) via pointer comparison (after interning)
- Type checking: O(1) via enum comparison
- Type casting: O(1) with safety checks
- Type creation: O(1) average (hash map lookup + possible creation)
- Struct member lookup: O(f) where f = fields per struct (typically <10, faster than hash overhead)
- Struct layout calculation: O(f) lazy calculation (only computed when needed)
- Primitive type size: O(1) via static lookup table
- Memory usage: O(t) where t = number of unique types (reduced by removing caches)

### 2. Symbol Table (`semantic/symbol_table.hpp`, `semantic/symbol_table.cpp`)

#### Purpose
Manages symbols across scopes with efficient lookup. References types from TypeRegistry without owning them.

#### Classes

**`Symbol` (Plain Struct)**
- **Purpose**: Represents a single symbol in the program
- **Key Members**:
  - `std::string name` - Symbol name
  - `SymbolKind kind` - Variable, Function, Type, Parameter, or Constant
  - `Type* type` - Symbol's type (non-owning pointer)
  - `SourceLocation location` - Source location for error reporting (replaces AST node)
- **Main Functions**:
  - Constructor with name, kind, type, and location

**`SymbolTable` (Main Class)**
- **Purpose**: Manages all symbols across scopes
- **Key Members**:
  - `std::vector<std::unordered_map<std::string, Symbol>> scope_stack_` - Stack of scopes
  - `TypeRegistry& type_registry_` - Reference to type registry for type creation

**Main Functions**:

*Scope Management*:
- `enter_scope()` - Push new scope onto stack
- `exit_scope()` - Pop current scope (keeps global scope)
- `current_scope_depth()` - Get current nesting level

*Symbol Operations*:
- `add_symbol(name, kind, type, ast_node)` - Add symbol to current scope (O(1))
- `lookup(name)` - Find symbol in any scope, inner-to-outer search (O(s) where s = number of scopes)
- `lookup_current_scope(name)` - Find symbol only in current scope (O(1))
- `lookup_member_function(type_name, method_name)` - Qualified method lookup (O(s))

*Type Management* (delegates to TypeRegistry):
- `get_primitive_type(name)` - Get primitive type via registry (O(1))
- `create_struct_type(name)` - Create new struct type via registry (O(1))
- `create_function_type(return_type, params)` - Create function type via registry (O(1))
- `create_pointer_type(pointee)` - Create pointer type via registry (O(1))
- `create_array_type(element, size)` - Create array type via registry (O(1))
- `register_struct_type(struct_type)` - Register struct and its methods as symbols

*Utility Functions*:
- `lookup_type(name)` - Find type by name
- `get_current_scope_symbols()` - Get all symbols in current scope
- `dump_symbols()` - Debug print all symbols

#### Integration Points
- **With Parser**: Parser calls `add_symbol` for declarations, `lookup` for references
- **With Type Registry**: Symbol table delegates type creation to registry
- **With Code Generator**: Code generator queries symbols for LLVM value generation
- **With Error Collector**: Symbol table reports undefined/redefined symbol errors

#### Performance Characteristics
- Symbol addition: O(1) average case (hash map insertion)
- Symbol lookup: O(s) where s is number of scopes (typically 2-5)
- Type creation: O(1) for all types
- Memory usage: O(n) where n is total number of symbols

### 3. Error Collector (`semantic/error_collector.hpp`)

#### Purpose
Centralized error collection and reporting system with clear error categorization.

#### Classes

**`SourceLocation` (Struct)**
- **Purpose**: Represents a location in source code
- **Key Members**:
  - `std::string filename` - Source file name
  - `int line, column` - Position in file
- **Main Functions**:
  - `to_string()` - Format as "filename:line:column"

**`Error` (Struct)**
- **Purpose**: Represents a single compiler error
- **Key Members**:
  - `ErrorKind kind` - Category of error
  - `std::string message` - Error description
  - `SourceLocation location` - Where error occurred
- **Main Functions**:
  - `to_string()` - Format error with kind and location

**`ErrorCollector` (Main Class)**
- **Purpose**: Collects and manages compilation errors and warnings
- **Key Members**:
  - `std::vector<Error> errors_` - All errors
  - `std::vector<Error> warnings_` - All warnings
  - `size_t max_errors_` - Maximum errors before stopping

**Main Functions**:
- `add_error(kind, message, location)` - Add compilation error
- `add_warning(kind, message, location)` - Add warning
- `has_errors()`, `has_warnings()` - Check if any errors/warnings exist
- `clear()` - Clear all errors and warnings
- `print_all()` - Print all errors and warnings to stderr
- `error_count()`, `warning_count()` - Get counts

**Convenience Functions**:
- `report_undefined_symbol(collector, name, location)` - Common error reporter
- `report_redefined_symbol(collector, name, location)` - Common error reporter  
- `report_type_mismatch(collector, expected, actual, location)` - Common error reporter

#### Integration Points
- **With Symbol Table**: Symbol table reports symbol-related errors
- **With Parser**: Parser reports syntax and semantic errors
- **With Code Generator**: Code generator reports code generation errors
- **With Main Compiler**: Main compiler checks error count to determine compilation success

#### Performance Characteristics
- Error addition: O(1) 
- Error reporting: O(n) where n is number of errors
- Memory usage: O(n) where n is number of errors

### 4. Compiler Context (`semantic/compiler_context.hpp`)

#### Purpose
Single source of truth that coordinates all compiler components and manages their lifetimes.

#### Classes

**`CompilerContext` (Main Class)**
- **Purpose**: Coordinates all semantic analysis components
- **Key Members**:
  - `TypeRegistry type_registry_` - Owns and manages all types
  - `SymbolTable symbol_table_` - Manages symbols (references types from registry)
  - `ErrorCollector error_collector_` - Collects all compilation errors
- **Main Functions**:
  - Constructor initializes all components with proper dependencies
  - `type_registry()` - Get reference to type registry
  - `symbol_table()` - Get reference to symbol table
  - `error_collector()` - Get reference to error collector
  - `has_errors()` - Check if compilation should stop
  - `print_errors()` - Print all collected errors

#### Integration Points
- **With Parser**: Parser takes CompilerContext and uses all components
- **With Code Generator**: Code generator takes CompilerContext for all semantic info
- **With Main Compiler**: Main creates single CompilerContext for entire compilation

#### Performance Characteristics
- Component access: O(1) via references
- Lifetime management: RAII ensures proper cleanup order
- Memory usage: Sum of all component memory usage

## Integration with Existing System

### Phase 1: Core Integration

**Parser Integration**:
- Update `build_symbol_table()` function to use new `SymbolTable` class
- Modify declaration parsing to call `symbol_table.add_symbol()`
- Update expression parsing to call `symbol_table.lookup()`
- Add `ErrorCollector` parameter to all parsing functions

**AST Integration**:
- AST nodes remain unchanged - they continue to be created by parser
- Symbol table builders traverse AST and populate symbol table
- Type resolution happens during symbol table building phase

### Phase 2: Code Generation

**Code Generator Updates**:
- Update `CodeGenerator` constructor to take `SymbolTable&` and `ErrorCollector&`
- Modify visitor methods to query symbol table instead of old symbol registry
- Update member function resolution to use `lookup_member_function()`
- Add error reporting for type mismatches and undefined symbols

**LLVM Integration**:
- Code generator queries `Type` objects for LLVM type generation
- Uses `Type::to_string()` for debugging and error messages
- Leverages `Type::is<T>()` and `Type::as<T>()` for safe type handling

### Phase 3: Error Handling

**Centralized Error Reporting**:
- Replace scattered error handling with `ErrorCollector` usage
- Update all compiler phases to report to same collector
- Main compiler checks `collector.has_errors()` before proceeding

**Source Location Tracking**:
- Parser tracks source locations and passes to error reporting
- Symbol table and code generator include locations in error reports

## Key Design Principles

### 1. Simplicity First
- Use proven patterns (RTTI from AST system)
- Avoid complex template metaprogramming
- Choose straightforward data structures (hash maps, vectors)

### 2. Clear Ownership
- TypeRegistry owns all Type objects via `std::unique_ptr`
- SymbolTable references types via non-owning pointers
- CompilerContext manages component lifetimes via RAII

### 3. Performance Focus
- O(1) symbol addition (no copying like immutable design)
- O(1) type equality via pointer comparison (after interning)
- O(1) type checking via enum comparison
- Lazy calculation of expensive operations (struct layout)
- Minimal memory usage by removing redundant caches
- Calculate-on-demand for infrequently used data

### 4. Testability
- Each component can be tested independently
- Clear interfaces with minimal dependencies
- Simple mocking for unit tests

### 5. Extensibility
- Adding new types requires minimal changes
- New symbol kinds easily added to enum
- Error categories can be extended without breaking existing code

## Storage vs Calculation Trade-offs

The architecture prioritizes **memory efficiency** and **simplicity** over micro-optimizations:

### ✅ **Removed Redundant Storage**:

| **Removed Data** | **Replacement** | **Reasoning** |
|------------------|-----------------|---------------|
| Field/method indices hash maps | Linear search through vectors | For typical structs (<10 fields), linear search is faster than hash overhead |
| Struct size/alignment cache | Lazy calculation with `std::optional` | Layout only needed during code generation (infrequent) |
| Primitive type cache | Linear search through interned types | Small number of primitives (~12), lookup table sufficient |
| AST node pointers in Symbol | Direct SourceLocation storage | Cleaner design, avoids dangling pointers |
| Primitive size storage | Static lookup table by name | Reduces per-instance storage, size rarely needed |

### 📊 **Performance Impact Analysis**:

| **Operation** | **Before** | **After** | **Real-World Impact** |
|---------------|------------|-----------|----------------------|
| Struct field lookup | O(1) hash | O(f) linear | Faster for f<10 (typical case) |
| Struct layout access | O(1) cached | O(f) lazy | Only computed when needed (rare) |
| Primitive type lookup | O(1) cached | O(p) linear | p≈12 primitives, negligible |
| Memory per struct | ~200 bytes overhead | ~50 bytes overhead | 75% memory reduction |

### 🎯 **Key Insight**: 
**For small datasets (typical compiler sizes), linear algorithms often outperform hash-based ones due to cache locality and reduced overhead.**

## Migration Strategy

### Step 1: Replace Symbol System
1. Remove old `semantic/symbol_registry.hpp` and related files
2. Update CMakeLists.txt to use new files
3. Fix compilation errors in existing code

### Step 2: Update Parser Integration
1. Modify `build_symbol_table()` to use new `SymbolTable`
2. Update declaration parsing to register symbols
3. Add error collection to parsing phases

### Step 3: Update Code Generation
1. Modify `CodeGenerator` to use new symbol table
2. Update type queries to use new type system
3. Add proper error reporting

### Step 4: Testing and Validation
1. Ensure all existing tests pass with new system
2. Add comprehensive tests for new components
3. Performance testing to verify O(1) operations

This plan provides a clear path to a maintainable, high-performance semantic analysis system that solves the original member function resolution issues while being much simpler than the previous "optimal" design.