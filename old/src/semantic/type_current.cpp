#include "semantic/type.hpp"
#include <algorithm>

namespace Myre {

// === STRUCT TYPE IMPLEMENTATION ===

void StructType::add_field(const std::string& name, Type* type) {
    fields_.emplace_back(name, type);
}

void StructType::add_method(const std::string& name, FunctionType* type) {
    methods_.emplace_back(name, type);
}

void StructType::finalize_layout() {
    size_t current_offset = 0;
    alignment_ = 1;
    
    for (auto& field : fields_) {
        // For simplicity, assume alignment = size for primitives
        size_t field_align = 1;
        if (auto* prim = field.type->as<PrimitiveType>()) {
            field_align = std::min(prim->size(), size_t(8));  // Max 8-byte alignment
        } else if (field.type->is<PointerType>()) {
            field_align = 8;  // Pointers are 8 bytes
        }
        
        // Update struct alignment
        alignment_ = std::max(alignment_, field_align);
        
        // Align current offset
        if (current_offset % field_align != 0) {
            current_offset = ((current_offset / field_align) + 1) * field_align;
        }
        
        field.offset = current_offset;
        
        // Advance offset by field size
        if (auto* prim = field.type->as<PrimitiveType>()) {
            current_offset += prim->size();
        } else if (field.type->is<PointerType>()) {
            current_offset += 8;
        } else if (auto* struct_type = field.type->as<StructType>()) {
            current_offset += struct_type->size();
        }
        // Arrays and functions as fields would need special handling
    }
    
    // Pad struct to its alignment
    if (current_offset % alignment_ != 0) {
        current_offset = ((current_offset / alignment_) + 1) * alignment_;
    }
    
    size_ = current_offset;
}

Field* StructType::find_field(const std::string& name) {
    auto it = std::find_if(fields_.begin(), fields_.end(),
        [&name](const Field& f) { return f.name == name; });
    return it != fields_.end() ? &(*it) : nullptr;
}

Method* StructType::find_method(const std::string& name) {
    auto it = std::find_if(methods_.begin(), methods_.end(),
        [&name](const Method& m) { return m.name == name; });
    return it != methods_.end() ? &(*it) : nullptr;
}

// === FUNCTION TYPE IMPLEMENTATION ===

std::string FunctionType::to_string() const {
    std::string result = "(";
    for (size_t i = 0; i < parameter_types_.size(); ++i) {
        if (i > 0) result += ", ";
        result += parameter_types_[i]->to_string();
    }
    if (is_varargs_) {
        if (!parameter_types_.empty()) result += ", ";
        result += "...";
    }
    result += ") -> ";
    result += return_type_->to_string();
    return result;
}

bool FunctionType::equals(const Type& other) const {
    if (other.kind() != TypeKind::Function) return false;
    const auto& other_func = static_cast<const FunctionType&>(other);
    
    if (!return_type_->equals(*other_func.return_type_)) return false;
    if (is_varargs_ != other_func.is_varargs_) return false;
    if (parameter_types_.size() != other_func.parameter_types_.size()) return false;
    
    for (size_t i = 0; i < parameter_types_.size(); ++i) {
        if (!parameter_types_[i]->equals(*other_func.parameter_types_[i])) {
            return false;
        }
    }
    
    return true;
}

size_t FunctionType::hash() const {
    size_t h = return_type_->hash();
    for (auto* param : parameter_types_) {
        h ^= param->hash() + 0x9e3779b9 + (h << 6) + (h >> 2);  // Good hash mixing
    }
    if (is_varargs_) h ^= 0xDEADBEEF;  // Mix in varargs flag
    return h;
}

} // namespace Myre