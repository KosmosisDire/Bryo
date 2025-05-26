#include "script_ast.hpp"

namespace Mycelium::Scripting::Lang
{

AstNode::AstNode() : id(idCounter++) {}

} // namespace Mycelium::Scripting::Lang