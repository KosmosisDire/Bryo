#pragma once
#include <string>
#include <sstream>

namespace Mycelium::Scripting::Lang
{
    struct SourceLocation
    {
        int lineStart = 0;
        int lineEnd = 0;
        int columnStart = 0;
        int columnEnd = 0;
        std::string fileName = "unknown"; // Added fileName

        std::string to_string() const {
            return fileName + ":(" + 
            std::to_string(lineStart) + "," + 
            std::to_string(columnStart) + ")"; 
        }
    };
}
