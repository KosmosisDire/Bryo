#include "sharpie/ast/ast_base.hpp"

namespace Mycelium::Scripting::Lang
{

// AstNode constructor is defined in ast_base.hpp if it's simple enough (like default)
// or here if it has more complex logic.
// The original _script_ast.cpp had: AstNode::AstNode() : id(idCounter++) {}
// This definition needs to be here if AstNode() was only declared in the header.
// Checking include/sharpie/ast/ast_base.hpp, AstNode() is declared.
AstNode::AstNode() : id(idCounter++) {}

} // namespace Mycelium::Scripting::Lang
