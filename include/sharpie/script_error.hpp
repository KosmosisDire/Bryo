#pragma once
#include <string>
#include "script_ast_location.hpp" // Assuming SourceLocation is here or in script_ast.hpp

namespace Mycelium::Scripting::Lang
{
    struct ParseError
    {
        std::string message;
        SourceLocation location;

        ParseError(std::string msg, const SourceLocation& loc)
            : message(std::move(msg)), location(loc) {}
    };
}