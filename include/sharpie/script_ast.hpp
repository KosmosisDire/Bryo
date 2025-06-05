#pragma once

// This file serves as a convenient single include for all AST node types.
// Individual AST component headers include their own necessary standard library headers.

#include "ast/ast_base.hpp"
#include "ast/ast_enums.hpp" // Though included by ast_base, explicit can be fine for clarity or direct use.
#include "ast/ast_location.hpp" // Though included by ast_base, explicit can be fine.
#include "ast/ast_types.hpp"
#include "ast/ast_expressions.hpp"
#include "ast/ast_statements.hpp"
#include "ast/ast_declarations.hpp" // Added
#include "ast/primitive_structs.hpp" // Added for primitive struct backing

// The Mycelium::Scripting::Lang namespace is opened in each individual ast_*.hpp file.
// No need to open it here if this file only contains includes.

// All specific AST node definitions and forward declarations have been moved
// to their respective files in the ast/ directory.
// This file is now just a master include.
