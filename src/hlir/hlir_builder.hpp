

#include "hlir.hpp"

namespace Bryo::HLIR
{
    class HLIRBuilder {
        Function* current_func = nullptr;
        BasicBlock* current_block = nullptr;
        
    public:
        void set_function(Function* f) { current_func = f; }
        void set_block(BasicBlock* b) { current_block = b; }
        
        Value* const_int(int64_t val, TypePtr type) {
            auto result = current_func->create_value(type);
            auto inst = std::make_unique<ConstIntInst>(result, val);
            result->def = inst.get();
            current_block->add_inst(std::move(inst));
            return result;
        }
        
        Value* const_bool(bool val, TypePtr type) {
            auto result = current_func->create_value(type);
            auto inst = std::make_unique<ConstBoolInst>(result, val);
            result->def = inst.get();
            current_block->add_inst(std::move(inst));
            return result;
        }
        
        Value* const_float(double val, TypePtr type) {
            auto result = current_func->create_value(type);
            auto inst = std::make_unique<ConstFloatInst>(result, val);
            result->def = inst.get();
            current_block->add_inst(std::move(inst));
            return result;
        }
        
        Value* const_string(const std::string& val, TypePtr type) {
            auto result = current_func->create_value(type);
            auto inst = std::make_unique<ConstStringInst>(result, val);
            result->def = inst.get();
            current_block->add_inst(std::move(inst));
            return result;
        }
        
        Value* const_null(TypePtr type) {
            // Create a zero value for the given type
            if (auto prim = type->as<PrimitiveType>()) {
                switch (prim->kind) {
                    case PrimitiveKind::Bool:
                        return const_bool(false, type);
                    case PrimitiveKind::I8:
                    case PrimitiveKind::I16:
                    case PrimitiveKind::I32:
                    case PrimitiveKind::I64:
                    case PrimitiveKind::U8:
                    case PrimitiveKind::U16:
                    case PrimitiveKind::U32:
                    case PrimitiveKind::U64:
                    case PrimitiveKind::Char:
                        return const_int(0, type);
                    case PrimitiveKind::F32:
                    case PrimitiveKind::F64:
                        return const_float(0.0, type);
                    default:
                        break;
                }
            }
            
            // For pointers, arrays, and complex types, use nullptr/zero
            auto result = current_func->create_value(type);
            auto inst = std::make_unique<ConstIntInst>(result, 0);
            result->def = inst.get();
            current_block->add_inst(std::move(inst));
            return result;
        }
        
        Value* alloc(TypePtr type, bool stack = false) {
            auto ptr_type = type; // Should be pointer type
            auto result = current_func->create_value(ptr_type);
            auto inst = std::make_unique<AllocInst>(result, type);
            inst->on_stack = stack;
            result->def = inst.get();
            current_block->add_inst(std::move(inst));
            return result;
        }
        
        Value* load(Value* addr, TypePtr type) {
            auto result = current_func->create_value(type);
            auto inst = std::make_unique<LoadInst>(result, addr);
            result->def = inst.get();
            addr->uses.push_back(inst.get());
            current_block->add_inst(std::move(inst));
            return result;
        }
        
        void store(Value* val, Value* addr) {
            auto inst = std::make_unique<StoreInst>(val, addr);
            val->uses.push_back(inst.get());
            addr->uses.push_back(inst.get());
            current_block->add_inst(std::move(inst));
        }
        
        Value* binary(Opcode op, Value* left, Value* right) {
            auto result = current_func->create_value(left->type);
            auto inst = std::make_unique<BinaryInst>(op, result, left, right);
            result->def = inst.get();
            left->uses.push_back(inst.get());
            right->uses.push_back(inst.get());
            current_block->add_inst(std::move(inst));
            return result;
        }
        
        Value* unary(Opcode op, Value* operand) {
            auto result = current_func->create_value(operand->type);
            auto inst = std::make_unique<UnaryInst>(op, result, operand);
            result->def = inst.get();
            operand->uses.push_back(inst.get());
            current_block->add_inst(std::move(inst));
            return result;
        }
        
        Value* cast(Value* value, TypePtr target_type) {
            auto result = current_func->create_value(target_type);
            auto inst = std::make_unique<CastInst>(result, value, target_type);
            result->def = inst.get();
            value->uses.push_back(inst.get());
            current_block->add_inst(std::move(inst));
            return result;
        }
        
        Value* call(Function* func, std::vector<Value*> args) {
            Value* result = nullptr;
            if (func->return_type() && !func->return_type()->is_void()) {
                result = current_func->create_value(func->return_type());
            }
            auto inst = std::make_unique<CallInst>(result, func, args);
            if (result) result->def = inst.get();
            for (auto arg : args) {
                arg->uses.push_back(inst.get());
            }
            current_block->add_inst(std::move(inst));
            return result;
        }
        
        void ret(Value* val = nullptr) {
            auto inst = std::make_unique<RetInst>(val);
            if (val) val->uses.push_back(inst.get());
            current_block->add_inst(std::move(inst));
        }
        
        void br(BasicBlock* target) {
            auto inst = std::make_unique<BrInst>(target);
            current_block->add_inst(std::move(inst));
            current_block->successors.push_back(target);
            target->predecessors.push_back(current_block);
        }
        
        void cond_br(Value* cond, BasicBlock* t, BasicBlock* f) {
            auto inst = std::make_unique<CondBrInst>(cond, t, f);
            cond->uses.push_back(inst.get());
            current_block->add_inst(std::move(inst));
            current_block->successors.push_back(t);
            current_block->successors.push_back(f);
            t->predecessors.push_back(current_block);
            f->predecessors.push_back(current_block);
        }
        
        Value* phi(TypePtr type) {
            auto result = current_func->create_value(type);
            auto inst = std::make_unique<PhiInst>(result);
            result->def = inst.get();
            current_block->add_inst(std::move(inst));
            return result;
        }
    };
}