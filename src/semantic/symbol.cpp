#include "semantic/symbol.hpp"

namespace Myre {

std::string Symbol::get_qualified_name() const
{
    if (name_ == "global") return "";

    // use the parent to build qualified name
    if (parent) {
        return parent->build_qualified_name(name_);
    }
    
    // Otherwise just return the name
    return name_;
}

} // namespace Myre