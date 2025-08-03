#include "semantic/type.hpp"
#include "semantic/type_definition.hpp"

namespace Myre {

bool Type::is_value_type() const {
    return std::visit([](const auto& v) -> bool {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, PrimitiveType>) {
            return !v.isRefType;
        } else if constexpr (std::is_same_v<T, DefinedType>) {
            if (auto def = v.definition.lock()) {
                return !def->is_ref_type();
            }
            return true; // Default to value type if definition is missing
        } else if constexpr (std::is_same_v<T, InstantiatedType>) {
            if (auto def = v.generic_definition.lock()) {
                return !def->is_ref_type();
            }
            return true; // Default to value type if definition is missing
        } else {
            return false; // Arrays, functions are reference types
        }
    }, value);
}

std::shared_ptr<TypeDefinition> Type::get_type_definition() const {
    return std::visit([](const auto& v) -> std::shared_ptr<TypeDefinition> {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, PrimitiveType>) {
            return v.definition.lock(); // i32 -> System.Int32, bool -> System.Boolean
        } else if constexpr (std::is_same_v<T, DefinedType>) {
            return v.definition.lock();
        } else if constexpr (std::is_same_v<T, InstantiatedType>) {
            return v.generic_definition.lock();
        } else {
            return nullptr; // Arrays, functions don't have TypeDefinitions (they're built-in)
        }
    }, value);
}

} // namespace Myre