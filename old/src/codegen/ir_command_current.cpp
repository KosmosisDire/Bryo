#include "codegen/ir_command.hpp"
#include <sstream>

namespace Myre {

std::string Command::to_string() const {
    std::stringstream ss;
    
    // Handle result
    if (result.is_valid()) {
        ss << "%" << result.id << " = ";
    }
    
    // Handle operation
    switch (op) {
        case Op::Const:
            ss << "const ";
            if (std::holds_alternative<int64_t>(data)) {
                ss << result.type.to_string() << " " << std::get<int64_t>(data);
            } else if (std::holds_alternative<bool>(data)) {
                ss << result.type.to_string() << " " << (std::get<bool>(data) ? "true" : "false");
            } else if (std::holds_alternative<double>(data)) {
                ss << result.type.to_string() << " " << std::get<double>(data);
            }
            break;
            
        case Op::Add:
            ss << "add " << args[0].type.to_string() << " %" << args[0].id << ", %" << args[1].id;
            break;
            
        case Op::Sub:
            ss << "sub " << args[0].type.to_string() << " %" << args[0].id << ", %" << args[1].id;
            break;
            
        case Op::Mul:
            ss << "mul " << args[0].type.to_string() << " %" << args[0].id << ", %" << args[1].id;
            break;
            
        case Op::Div:
            ss << "sdiv " << args[0].type.to_string() << " %" << args[0].id << ", %" << args[1].id;
            break;
            
        case Op::And:
            ss << "and " << args[0].type.to_string() << " %" << args[0].id << ", %" << args[1].id;
            break;
            
        case Op::Or:
            ss << "or " << args[0].type.to_string() << " %" << args[0].id << ", %" << args[1].id;
            break;
            
        case Op::Not:
            ss << "xor " << args[0].type.to_string() << " %" << args[0].id << ", 1";
            break;
            
        case Op::ICmp: {
            ss << "icmp ";
            if (std::holds_alternative<ICmpPredicate>(data)) {
                auto pred = std::get<ICmpPredicate>(data);
                switch (pred) {
                    case ICmpPredicate::Eq: ss << "eq"; break;
                    case ICmpPredicate::Ne: ss << "ne"; break;
                    case ICmpPredicate::Slt: ss << "slt"; break;
                    case ICmpPredicate::Sle: ss << "sle"; break;
                    case ICmpPredicate::Sgt: ss << "sgt"; break;
                    case ICmpPredicate::Sge: ss << "sge"; break;
                    case ICmpPredicate::Ult: ss << "ult"; break;
                    case ICmpPredicate::Ule: ss << "ule"; break;
                    case ICmpPredicate::Ugt: ss << "ugt"; break;
                    case ICmpPredicate::Uge: ss << "uge"; break;
                }
            }
            ss << " " << args[0].type.to_string() << " %" << args[0].id << ", %" << args[1].id;
            break;
        }
            
        case Op::Alloca:
            if (std::holds_alternative<std::string>(data)) {
                ss << "alloca " << std::get<std::string>(data);
            } else {
                ss << "alloca " << result.type.to_string(); // fallback
            }
            break;
            
        case Op::Load:
            ss << "load " << result.type.to_string() << ", ptr %" << args[0].id;
            break;
            
        case Op::Store:
            ss << "store " << args[0].type.to_string() << " %" << args[0].id << ", ptr %" << args[1].id;
            break;
            
        case Op::GEP:
            ss << "getelementptr ";
            if (std::holds_alternative<std::string>(data)) {
                std::string indices = std::get<std::string>(data);
                // TODO: Add proper type information for GEP
                ss << "ptr %" << args[0].id << ", " << indices;
            }
            break;
            
        case Op::Label:
            if (std::holds_alternative<std::string>(data)) {
                ss << std::get<std::string>(data) << ":";
            }
            break;
            
        case Op::Br:
            if (std::holds_alternative<std::string>(data)) {
                ss << "br label %" << std::get<std::string>(data);
            }
            break;
            
        case Op::BrCond:
            if (std::holds_alternative<std::string>(data)) {
                // Extract true and false labels from string (format: "true_label,false_label")
                std::string labels = std::get<std::string>(data);
                size_t comma = labels.find(',');
                if (comma != std::string::npos) {
                    std::string true_label = labels.substr(0, comma);
                    std::string false_label = labels.substr(comma + 1);
                    ss << "br i1 %" << args[0].id << ", label %" << true_label << ", label %" << false_label;
                }
            }
            break;
            
        case Op::Ret:
            ss << "ret " << args[0].type.to_string() << " %" << args[0].id;
            break;
            
        case Op::RetVoid:
            ss << "ret void";
            break;
            
        case Op::FunctionBegin:
            if (std::holds_alternative<std::string>(data)) {
                // Parse the function signature: "name:returntype" or "name:returntype:param1,param2,..."
                std::string func_info = std::get<std::string>(data);
                size_t first_colon = func_info.find(':');
                if (first_colon != std::string::npos) {
                    std::string name = func_info.substr(0, first_colon);
                    std::string remainder = func_info.substr(first_colon + 1);
                    
                    size_t second_colon = remainder.find(':');
                    std::string return_type_str;
                    std::string param_types_str;
                    
                    if (second_colon != std::string::npos) {
                        return_type_str = remainder.substr(0, second_colon);
                        param_types_str = remainder.substr(second_colon + 1);
                    } else {
                        return_type_str = remainder;
                    }
                    
                    // Format the function signature properly
                    ss << "define " << return_type_str << " @" << name << "(";
                    
                    // Add parameter types
                    if (!param_types_str.empty()) {
                        std::string current_param;
                        bool first_param = true;
                        for (char c : param_types_str) {
                            if (c == ',') {
                                if (!current_param.empty()) {
                                    if (!first_param) ss << ", ";
                                    ss << current_param;
                                    first_param = false;
                                    current_param.clear();
                                }
                            } else {
                                current_param += c;
                            }
                        }
                        // Handle last parameter
                        if (!current_param.empty()) {
                            if (!first_param) ss << ", ";
                            ss << current_param;
                        }
                    }
                    
                    ss << ") {";
                } else {
                    // Fallback for invalid signature
                    ss << "define void @" << func_info << "() {";
                }
            }
            break;
            
        case Op::FunctionEnd:
            ss << "}";
            break;
            
        case Op::Call:
            if (std::holds_alternative<std::string>(data)) {
                ss << "call " << result.type.to_string() << " @" << std::get<std::string>(data) << "(";
                for (size_t i = 0; i < args.size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << args[i].type.to_string() << " %" << args[i].id;
                }
                ss << ")";
            }
            break;
            
        default:
            ss << "unknown_op(" << static_cast<int>(op) << ")";
            break;
    }
    
    return ss.str();
}

// IRType implementation
std::string IRType::to_string() const {
    switch (kind) {
        case Kind::Void: return "void";
        case Kind::I32: return "i32";
        case Kind::I64: return "i64";
        case Kind::I8: return "i8";
        case Kind::I16: return "i16";
        case Kind::Bool: return "i1";
        case Kind::F32: return "f32";
        case Kind::F64: return "f64";
        case Kind::Ptr: return "ptr";
        case Kind::Struct: 
            if (struct_layout && !struct_layout->name.empty()) {
                return "struct." + struct_layout->name;
            }
            return "struct";
        default: return "unknown";
    }
}

IRType IRType::ptr_to(IRType pointee) {
    IRType result(Kind::Ptr);
    result.pointee_type = std::make_shared<IRType>(pointee);
    return result;
}

IRType IRType::struct_(std::shared_ptr<StructLayout> layout) {
    IRType result(Kind::Struct);
    result.struct_layout = layout;
    return result;
}

size_t IRType::size_in_bytes() const {
    switch (kind) {
        case Kind::Void: return 0;
        case Kind::I8: return 1;
        case Kind::I16: return 2;
        case Kind::I32: return 4;
        case Kind::I64: return 8;
        case Kind::Bool: return 1;  // i1 in LLVM
        case Kind::F32: return 4;
        case Kind::F64: return 8;
        case Kind::Ptr: return 8;  // 64-bit pointers
        case Kind::Struct:
            return struct_layout ? struct_layout->total_size : 0;
        default: return 0;
    }
}

size_t IRType::alignment() const {
    switch (kind) {
        case Kind::Void: return 1;
        case Kind::I8: return 1;
        case Kind::I16: return 2;
        case Kind::I32: return 4;
        case Kind::I64: return 8;
        case Kind::Bool: return 1;
        case Kind::F32: return 4;
        case Kind::F64: return 8;
        case Kind::Ptr: return 8;  // 64-bit alignment
        case Kind::Struct:
            return struct_layout ? struct_layout->alignment : 1;
        default: return 1;
    }
}

bool IRType::operator==(const IRType& other) const {
    if (kind != other.kind) return false;
    
    // Compare pointee types for pointer types
    if (kind == Kind::Ptr) {
        if (!pointee_type && !other.pointee_type) return true;
        if (!pointee_type || !other.pointee_type) return false;
        return *pointee_type == *other.pointee_type;
    }
    
    // Compare struct layouts for struct types
    if (kind == Kind::Struct) {
        if (!struct_layout && !other.struct_layout) return true;
        if (!struct_layout || !other.struct_layout) return false;
        return struct_layout->name == other.struct_layout->name;
    }
    
    return true;
}

// StructLayout implementation
void StructLayout::calculate_layout() {
    size_t current_offset = 0;
    alignment = 1;
    
    for (auto& field : fields) {
        // Get field alignment requirement
        size_t field_align = field.type.alignment();
        
        // Update struct alignment to be at least as strict as field
        if (field_align > alignment) {
            alignment = field_align;
        }
        
        // Align current offset to field alignment
        if (current_offset % field_align != 0) {
            current_offset = ((current_offset / field_align) + 1) * field_align;
        }
        
        // Set field offset
        field.offset = current_offset;
        
        // Advance offset by field size
        current_offset += field.type.size_in_bytes();
    }
    
    // Pad struct to its alignment
    if (current_offset % alignment != 0) {
        current_offset = ((current_offset / alignment) + 1) * alignment;
    }
    
    total_size = current_offset;
}

} // namespace Myre