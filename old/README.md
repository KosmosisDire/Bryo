# Old/Legacy Files

This directory contains old and legacy files that were moved out of the main codebase during the semantic analysis and code generation system cleanup in preparation for implementing the new architecture outlined in `SemanticCodegenPlan.md`.

## File Categories

### Semantic Analysis Files (old)
- `symbol_registry.hpp` - Old immutable symbol registry system
- `type_system.hpp` / `type_system.cpp` - Old type system implementation
- `error_system.hpp` / `semantic_error.hpp` - Old error handling system
- `symbol_table.hpp.old` / `symbol_table.cpp.old` - Previous symbol table backups

### Current File Backups
These are backups of current files that will be replaced during the new implementation:
- `symbol_table_current.hpp/cpp` - Current symbol table implementation (to be replaced)
- `type_current.hpp/cpp` - Current type system (to be replaced)
- `codegen_current.hpp/cpp` - Current code generator (to be updated)
- `ir_builder_current.hpp` - Current IR builder (to be replaced)
- `ir_command_current.hpp/cpp` - Current IR command system (to be updated)
- `command_processor_current.hpp/cpp` - Current command processor (to be updated)
- `jit_engine_current.hpp/cpp` - Current JIT engine (reference)

### Codegen Files (old)
- `codegen.hpp.old` / `codegen.cpp.old` - Previous codegen implementations
- `ir_builder.hpp.old` / `ir_builder.cpp.old` - Previous IR builder implementations
- `command.hpp` - Old command definitions
- `ir_command_builder.hpp` - Old IR command builder
- `ir_value.hpp` - Old IR value definitions
- `simple_ir_builder.hpp` - Old simple IR builder

## Purpose

These files are kept for reference during the implementation of the new system described in `SemanticCodegenPlan.md`. They provide:

1. **Reference implementations** - Understanding how the old system worked
2. **Backup safety** - Ability to revert if needed during development
3. **Migration guidance** - Comparison points for ensuring feature completeness
4. **Historical context** - Understanding of the evolution of the codebase

## New Architecture Goal

The new system aims to implement:
- **Type Registry** - Centralized type management with interning
- **Symbol Table** - Simplified mutable symbol management  
- **Error Collector** - Centralized error reporting
- **Compiler Context** - Single source of truth coordinating all components

Once the new system is fully implemented and tested, these old files can be archived or removed.