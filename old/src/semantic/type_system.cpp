#include "semantic/type_system.hpp"

namespace Myre {

// Static member initialization
std::unordered_map<std::string, std::shared_ptr<PrimitiveType>> TypeFactory::primitive_cache_;

} // namespace Myre