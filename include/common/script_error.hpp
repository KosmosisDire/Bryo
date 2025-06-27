#pragma once
#include <string>
#include "../ast/ast_location.hpp" // Updated path for SourceLocation

namespace Mycelium::Scripting::Lang
{
    struct ParseError
    {
        std::string message;
        SourceLocation location; // SourceLocation is defined in ast_location.hpp

        ParseError(std::string msg, const SourceLocation& loc)
            : message(std::move(msg)), location(loc) {}
    };
}
