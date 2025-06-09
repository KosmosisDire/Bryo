#pragma once

#include "../ast/ast_location.hpp" // For SourceLocation
#include <string>

namespace Mycelium::Scripting::Lang
{

/**
 * @struct MethodCallInfo
 * @brief Represents a discovered dependency between two methods.
 */
struct MethodCallInfo {
    std::string caller_class;
    std::string caller_method;
    std::string callee_class;
    std::string callee_method;
    bool is_forward_call;  // true if this is a forward declaration call
    SourceLocation call_location;
    
    std::string get_caller_qualified_name() const {
        return caller_class + "." + caller_method;
    }
    
    std::string get_callee_qualified_name() const {
        return callee_class + "." + callee_method;
    }
};

} // namespace Mycelium::Scripting::Lang