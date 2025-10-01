// hlir_builder.hpp
#pragma once

#include "binding/bound_tree.hpp"
#include "semantic/type_system.hpp"
#include "hlir_builder.hpp"
#include <unordered_map>
#include <stack>
#include <vector>

namespace Bryo::HLIR
{
    class BoundToHLIR : public BoundVisitor {
    private:
        #pragma region Core State
        HLIR::Module* module;
        HLIR::HLIRBuilder builder;
        TypeSystem* type_system;
        
        // Current context
        HLIR::Function* current_function = nullptr;
        HLIR::BasicBlock* current_block = nullptr;
        
        #pragma region SSA Value Tracking
        // Symbol to SSA value mapping
        std::unordered_map<Symbol*, HLIR::Value*> symbol_values;
        
        // Expression results cache
        std::unordered_map<BoundExpression*, HLIR::Value*> expression_values;
        
        #pragma region Control Flow Context
        struct LoopContext {
            HLIR::BasicBlock* continue_target;
            HLIR::BasicBlock* break_target;
            std::unordered_map<Symbol*, HLIR::Value*> loop_entry_values;
        };
        std::stack<LoopContext> loop_stack;
        
        // For deferred phi resolution
        struct PendingPhi {
            HLIR::PhiInst* phi;
            Symbol* symbol;
            HLIR::BasicBlock* block;
        };
        std::vector<PendingPhi> pending_phis;
        
    public:
        BoundToHLIR(HLIR::Module* mod, TypeSystem* types) 
            : module(mod), type_system(types) {}
        
        void build(BoundCompilationUnit* unit);
        
        #pragma region Visitor Methods
        // Expressions
        void visit(BoundLiteralExpression* node) override;
        void visit(BoundNameExpression* node) override;
        void visit(BoundBinaryExpression* node) override;
        void visit(BoundUnaryExpression* node) override;
        void visit(BoundAssignmentExpression* node) override;
        void visit(BoundCallExpression* node) override;
        void visit(BoundMemberAccessExpression* node) override;
        void visit(BoundIndexExpression* node) override;
        void visit(BoundNewExpression* node) override;
        void visit(BoundArrayCreationExpression* node) override;
        void visit(BoundCastExpression* node) override;
        void visit(BoundConditionalExpression* node) override;
        void visit(BoundThisExpression* node) override;
        void visit(BoundTypeOfExpression* node) override;
        void visit(BoundSizeOfExpression* node) override;
        void visit(BoundParenthesizedExpression* node) override;
        void visit(BoundConversionExpression* node) override;
        void visit(BoundTypeExpression* node) override;
        
        // Statements
        void visit(BoundBlockStatement* node) override;
        void visit(BoundExpressionStatement* node) override;
        void visit(BoundIfStatement* node) override;
        void visit(BoundWhileStatement* node) override;
        void visit(BoundForStatement* node) override;
        void visit(BoundBreakStatement* node) override;
        void visit(BoundContinueStatement* node) override;
        void visit(BoundReturnStatement* node) override;
        void visit(BoundUsingStatement* node) override;
        
        // Declarations
        void visit(BoundVariableDeclaration* node) override;
        void visit(BoundFunctionDeclaration* node) override;
        void visit(BoundPropertyDeclaration* node) override;
        void visit(BoundTypeDeclaration* node) override;
        void visit(BoundNamespaceDeclaration* node) override;
        
        // Top-level
        void visit(BoundCompilationUnit* node) override;
        
    private:
        #pragma region Helper Methods
        HLIR::Value* evaluate_expression(BoundExpression* expr);
        HLIR::Value* get_symbol_value(Symbol* sym);
        void set_symbol_value(Symbol* sym, HLIR::Value* val);
        HLIR::BasicBlock* create_block(const std::string& name);
        void resolve_pending_phis();
        HLIR::Opcode get_binary_opcode(BinaryOperatorKind kind);
        HLIR::Opcode get_unary_opcode(UnaryOperatorKind kind);

        // Property helper methods
        void generate_property_getter(BoundPropertyDeclaration* prop_decl, BoundPropertyAccessor* getter);
        void generate_property_setter(BoundPropertyDeclaration* prop_decl, BoundPropertyAccessor* setter);

        // Loop phi node helper
        class LoopPhiHelper {
        private:
            BoundToHLIR* converter;
            HLIR::BasicBlock* entry_block;
            HLIR::BasicBlock* header_block;
            std::unordered_map<Symbol*, HLIR::Value*> entry_values;
            std::unordered_map<Symbol*, HLIR::PhiInst*> phi_nodes;

        public:
            LoopPhiHelper(BoundToHLIR* conv, HLIR::BasicBlock* entry, HLIR::BasicBlock* header)
                : converter(conv), entry_block(entry), header_block(header) {
                entry_values = conv->symbol_values;
            }

            // Create phi nodes at loop header for all current variables
            void create_phis() {
                for (auto& [sym, entry_val] : entry_values) {
                    auto phi_result = converter->builder.phi(entry_val->type);
                    auto phi_inst = static_cast<HLIR::PhiInst*>(phi_result->def);
                    phi_nodes[sym] = phi_inst;

                    // Add incoming value from entry block
                    phi_inst->add_incoming(entry_val, entry_block);

                    // Update symbol_values to use the phi result
                    converter->symbol_values[sym] = phi_result;
                }
            }

            // Add back-edge values from loop body
            void add_backedge(HLIR::BasicBlock* backedge_block) {
                if (backedge_block) {
                    for (auto& [sym, phi_inst] : phi_nodes) {
                        auto body_val = converter->symbol_values[sym];
                        phi_inst->add_incoming(body_val, backedge_block);
                    }
                }
            }

            // Restore phi results after loop
            void finalize() {
                for (auto& [sym, phi_inst] : phi_nodes) {
                    converter->symbol_values[sym] = phi_inst->result;
                }
            }
        };
    };
}