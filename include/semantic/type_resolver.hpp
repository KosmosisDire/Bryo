#pragma once

#include "symbol_table.hpp"
#include "type_system.hpp"
#include "ast/ast.hpp"
#include "symbol.hpp"
#include <set>
#include <unordered_map>
#include <iostream>
#include <sstream>

namespace Myre
{
    class ReturnStatementCollector : public DefaultVisitor {
    private:
        std::vector<Expression*> returns;
        
    public:
        void visit(ReturnStmt* node) override {
            returns.push_back(node->value); // Can be null for void returns
        }
        
        const std::vector<Expression*>& get_returns() const {
            return returns;
        }
        
        void clear() {
            returns.clear();
        }
    };

    using TypePtr = std::shared_ptr<Type>;

    class TypeResolver : public DefaultVisitor
    {
    private:
        SymbolTable &symbolTable;
        TypeSystem &typeSystem;

        // --- Core of the Unification Solver ---
        // This map stores the learned substitutions. e.g., { var:4 -> Player }
        std::unordered_map<TypePtr, TypePtr> substitution;

        std::vector<std::string> errors;
        SymbolHandle currentScope;
        std::unordered_map<Node *, TypePtr> nodeTypes;

        // Debug settings
        bool debugEnabled = true;
        int debugIndent = 0;

        // Debug helpers
        void debug(const std::string &msg)
        {
            if (!debugEnabled) return;
            for (int i = 0; i < debugIndent; ++i) std::cout << "  ";
            std::cout << "[DEBUG] " << msg << std::endl;
        }

        void debugEnter(const std::string &context)
        {
            debug(">>> Entering: " + context);
            debugIndent++;
        }

        void debugExit(const std::string &context)
        {
            debugIndent--;
            debug("<<< Exiting: " + context);
        }

        std::string typeToString(TypePtr type)
        {
            if (!type) return "nullptr";
            return type->get_name() + " @" + std::to_string(reinterpret_cast<uintptr_t>(type.get()));
        }

    public:
        TypeResolver(SymbolTable &symbol_table, bool enable_debug = true)
            : symbolTable(symbol_table), typeSystem(symbol_table.get_type_system()), debugEnabled(enable_debug) {}

        const std::vector<std::string> &get_errors() const { return errors; }
        void set_debug(bool enabled) { debugEnabled = enabled; }

        // Main entry point
        bool resolve_types()
        {
            errors.clear();
            nodeTypes.clear();
            substitution.clear();

            debug("=== Starting Type Resolution ===");

            // 1. Generate constraints and unify them on the fly in passes until no more progress is made.
            debugEnter("generate_and_unify_all");
            generate_and_unify_all();
            debugExit("generate_and_unify_all");

            // 2. Apply the final substitution map to all symbols to set their concrete types.
            debugEnter("apply_final_solution");
            apply_final_solution();
            debugExit("apply_final_solution");
            
            // 3. Report any symbols that remain unresolved.
            report_final_errors();

            debug("=== Type Resolution Complete ===");
            return errors.empty();
        }

    private:
        // --- Core Unification and Substitution Logic ---

        /**
         * Finds the ultimate, canonical representative for a type by following the substitution chain.
         * Implements path compression for efficiency: makes every node in the chain point directly to the root.
         */
        TypePtr apply_substitution(TypePtr type)
        {
            if (!type) return nullptr;

            auto it = substitution.find(type);
            if (it == substitution.end()) {
                return type; // This type is a root (e.g., a concrete type or an un-unified variable).
            }
            
            // Path Compression: Recursively find the root and update this type's substitution to point directly to it.
            TypePtr root = apply_substitution(it->second);
            if (root != it->second) {
                substitution[type] = root; // Compress the path for future lookups.
            }
            return root;
        }

        /**
         * Unifies two types, making them equivalent in the substitution map.
         * This is the main engine of the type inference algorithm.
         */
        void unify(TypePtr t1, TypePtr t2, const std::string& source)
        {
            if (!t1 || !t2) return;

            TypePtr root1 = apply_substitution(t1);
            TypePtr root2 = apply_substitution(t2);

            if (root1 == root2) return; // The types are already in the same set.

            bool root1_is_var = std::holds_alternative<UnresolvedType>(root1->value);
            bool root2_is_var = std::holds_alternative<UnresolvedType>(root2->value);

            // Unification strategy: always map a variable to a more concrete type if possible.
            if (root1_is_var) { // var = type OR var1 = var2
                substitution[root1] = root2;
                debug("Unified: " + typeToString(root1) + " => " + typeToString(root2) + " (from " + source + ")");
            } else if (root2_is_var) { // type = var
                substitution[root2] = root1;
                debug("Unified: " + typeToString(root2) + " => " + typeToString(root1) + " (from " + source + ")");
            } else if (root1->get_name() != root2->get_name()) {
                // Unification failure: two incompatible concrete types.
                std::string err = "Type mismatch: Cannot unify '" + root1->get_name() + "' with '" + root2->get_name() + "' (from " + source + ")";
                errors.push_back(err);
                debug("ERROR: " + err);
            }
            // If both are concrete and equal, we do nothing, as root1 == root2 would have been true.
        }
        
        // --- Phase 1: Constraint Generation & Unification ---

        void generate_and_unify_all()
        {
            int pass = 0;
            const int max_passes = 10; // Failsafe against infinite loops
            while (pass < max_passes) {
                pass++;
                debug("\n--- Generation Pass " + std::to_string(pass) + " ---");
                
                auto unresolved_symbols = symbolTable.get_unresolved_symbols();
                if (unresolved_symbols.empty()) {
                    debug("All symbols resolved, stopping generation.");
                    break;
                }
                
                size_t unresolved_before = unresolved_symbols.size();
                debug("Unresolved symbols to process: " + std::to_string(unresolved_before));

                for (auto* symbol : unresolved_symbols) {
                    generate_constraints_for_symbol(symbol);
                }
                
                // We must apply the solution after each pass to update the symbol table itself,
                // which allows subsequent passes to work with more concrete types.
                apply_final_solution();
                
                size_t unresolved_after = symbolTable.get_unresolved_symbols().size();
                if (unresolved_after == unresolved_before) {
                    debug("No progress made in this pass, stopping generation loop.");
                    break;
                }
            }
             if (pass >= max_passes) {
                debug("WARNING: Reached max generation passes.");
            }
        }

        void generate_constraints_for_symbol(Symbol* symbol)
        {
            debug("Processing symbol: " + symbol->name() + " (" + symbol->kind_name() + ")");

            if (auto* func = symbol->as<FunctionSymbol>()) {
                TypePtr return_type = func->return_type();
                auto& unresolved = std::get<UnresolvedType>(return_type->value);
                if (unresolved.body) {
                    currentScope = {func->handle.id};
                    TypePtr inferred_body_type = analyze_function_body(unresolved.body);
                    if (inferred_body_type) {
                        unify(return_type, inferred_body_type, "function body of " + func->name());
                    }
                }
            } else if (auto* typed = symbol->as<TypedSymbol>()) {
                TypePtr symbol_type = typed->type();
                auto& unresolved = std::get<UnresolvedType>(symbol_type->value);
                if (unresolved.initializer) {
                    currentScope = {unresolved.definingScope};
                    unresolved.initializer->accept(this);
                    TypePtr initializer_type = get_node_type(unresolved.initializer);
                    if (initializer_type) {
                        unify(symbol_type, initializer_type, "initializer of " + symbol->name());
                    }
                }
            }
        }

        TypePtr analyze_function_body(void* body_ptr)
        {
            auto* block = static_cast<Block*>(body_ptr);
            ReturnStatementCollector collector;
            block->accept(&collector);
            const auto& return_exprs = collector.get_returns();

            debug("Found " + std::to_string(return_exprs.size()) + " return statements");

            if (return_exprs.empty()) {
                return typeSystem.get_primitive("void");
            }

            TypePtr common_type = nullptr;

            for (Expression* expr : return_exprs) {
                if (!expr) { // Handle `return;`
                    TypePtr void_type = typeSystem.get_primitive("void");
                    if (!common_type) common_type = void_type;
                    else unify(common_type, void_type, "mixed return");
                    continue;
                }

                expr->accept(this);
                TypePtr expr_type = get_node_type(expr);
                if (!expr_type) continue;
                
                if (!common_type) {
                    common_type = expr_type;
                } else {
                    unify(common_type, expr_type, "multiple return paths");
                }
            }

            return common_type ? common_type : typeSystem.get_primitive("void");
        }

        // --- Phase 2: Applying the Final Solution ---

        void apply_final_solution()
        {
            std::vector<ScopeNode*> all_symbols;
            symbolTable.get_all_symbols(all_symbols);

            for (auto* sym : all_symbols)
            {
                if (!sym || !sym->is<TypedSymbol>()) continue;

                TypePtr original_type = nullptr;
                Symbol* symbol = sym->as<Symbol>();
                TypedSymbol* typed_sym = symbol->as<TypedSymbol>();
                FunctionSymbol* func_sym = symbol->as<FunctionSymbol>();

                if (typed_sym) original_type = typed_sym->type();
                
                if (original_type) {
                    TypePtr canonical_type = apply_substitution(original_type);
                    if (original_type != canonical_type) {
                        if (typed_sym) typed_sym->set_type(canonical_type);
                        if (func_sym) func_sym->set_return_type(canonical_type);
                    }
                    
                    if (symbolTable.is_symbol_unresolved(symbol) && !std::holds_alternative<UnresolvedType>(canonical_type->value)) {
                        symbolTable.mark_symbol_resolved(symbol);
                        debug("    Symbol '" + symbol->name() + "' resolved to type '" + canonical_type->get_name() + "'");
                    }
                }
            }
        }

        void report_final_errors()
        {
            for (auto* symbol : symbolTable.get_unresolved_symbols())
            {
                if (auto* func = symbol->as<FunctionSymbol>())
                    errors.push_back("Could not infer return type for function '" + symbol->name() + "'");
                else
                    errors.push_back("Could not infer type for " + std::string(symbol->kind_name()) + " '" + symbol->name() + "'");
            }
        }

        // --- Visitor Methods for AST Traversal ---

        void visit(LiteralExpr* node) override
        {
            debugEnter("LiteralExpr");
            std::string type_name;
            switch (node->kind) {
                case LiteralExpr::Kind::Integer: type_name = "i32"; break;
                case LiteralExpr::Kind::Float:   type_name = "f64"; break;
                case LiteralExpr::Kind::String:  type_name = "string"; break;
                case LiteralExpr::Kind::Char:    type_name = "char"; break;
                case LiteralExpr::Kind::Bool:    type_name = "bool"; break;
                case LiteralExpr::Kind::Null:    type_name = "null"; break;
            }
            nodeTypes[node] = typeSystem.get_primitive(type_name);
            debug("Literal type: " + typeToString(nodeTypes[node]));
            debugExit("LiteralExpr");
        }

        void visit(NameExpr* node) override
        {
            std::string name = node->get_full_name();
            debugEnter("NameExpr: " + name);
            Symbol* symbol = symbolTable.lookup_handle(node->containingScope)->as<Scope>()->lookup(name);
            if (!symbol) {
                errors.push_back("Undefined identifier: " + name);
                debugExit("NameExpr");
                return;
            }

            if (auto* func = symbol->as<FunctionSymbol>()) {
                nodeTypes[node] = func->return_type();
            } else if (auto* typed = symbol->as<TypedSymbol>()) {
                nodeTypes[node] = typed->type();
            } else {
                errors.push_back("Symbol '" + name + "' does not have a resolvable type.");
            }
            debugExit("NameExpr");
        }

        void visit(CallExpr* node) override
        {
            debugEnter("CallExpr");
            node->callee->accept(this);
            for (auto& arg : node->arguments) {
                arg->accept(this);
            }
            nodeTypes[node] = get_node_type(node->callee);
            debugExit("CallExpr");
        }

        void visit(BinaryExpr* node) override
        {
            debugEnter("BinaryExpr");
            node->left->accept(this);
            node->right->accept(this);

            TypePtr left_type = get_node_type(node->left);
            TypePtr right_type = get_node_type(node->right);

            if (!left_type || !right_type) {
                errors.push_back("Could not resolve one or more operand types in binary expression.");
                debugExit("BinaryExpr");
                return;
            }
            
            unify(left_type, right_type, "binary op");
            TypePtr unified_type = apply_substitution(left_type);

            switch (node->op) {
                case BinaryOperatorKind::Equals:
                case BinaryOperatorKind::NotEquals:
                case BinaryOperatorKind::LessThan:
                case BinaryOperatorKind::LessThanOrEqual:
                case BinaryOperatorKind::GreaterThan:
                case BinaryOperatorKind::GreaterThanOrEqual:
                case BinaryOperatorKind::LogicalAnd:
                case BinaryOperatorKind::LogicalOr:
                    nodeTypes[node] = typeSystem.get_primitive("bool");
                    break;
                default: // Arithmetic operators
                    nodeTypes[node] = unified_type;
                    break;
            }
            debugExit("BinaryExpr");
        }
        
        void visit(UnaryExpr* node) override
        {
            debugEnter("UnaryExpr");
            node->operand->accept(this);
            TypePtr operand_type = get_node_type(node->operand);
            switch (node->op) {
                case UnaryOperatorKind::Not:
                    nodeTypes[node] = typeSystem.get_primitive("bool");
                    break;
                default:
                    nodeTypes[node] = operand_type; // For +/-/! etc., type is preserved.
                    break;
            }
            debugExit("UnaryExpr");
        }

        void visit(AssignmentExpr* node) override
        {
            debugEnter("AssignmentExpr");
            node->target->accept(this);
            node->value->accept(this);
            TypePtr target_type = get_node_type(node->target);
            TypePtr value_type = get_node_type(node->value);
            if (target_type && value_type) {
                unify(target_type, value_type, "assignment");
                nodeTypes[node] = apply_substitution(value_type);
            }
            debugExit("AssignmentExpr");
        }
        
        void visit(NewExpr* node) override
        {
            debugEnter("NewExpr");
            if (auto* named_type = node->type->as<NamedTypeRef>()) {
                nodeTypes[node] = resolve_type_ref(named_type);
            }
            debugExit("NewExpr");
        }
        
        void visit(RangeExpr* node) override
        {
            debugEnter("RangeExpr");
            if (node->start) node->start->accept(this);
            if (node->end) node->end->accept(this);
            if (node->step) node->step->accept(this);
            nodeTypes[node] = typeSystem.get_primitive("Range");
            debugExit("RangeExpr");
        }

        void visit(MemberAccessExpr* node) override
        {
            debugEnter("MemberAccessExpr");
            node->object->accept(this);
            TypePtr object_type = apply_substitution(get_node_type(node->object));
            std::string member_name(node->member->text);

            if (!object_type) {
                errors.push_back("Could not resolve member access target");
                debugExit("MemberAccessExpr");
                return;
            }

            auto* type_symbol = object_type->get_type_symbol();
            if (!type_symbol || !type_symbol->is<Scope>()) {
                errors.push_back("Type '" + object_type->get_name() + "' does not have members.");
                debugExit("MemberAccessExpr");
                return;
            }
            
            Symbol* member_symbol = type_symbol->as<Scope>()->lookup_local(member_name);
            if (!member_symbol) {
                errors.push_back("Member '" + member_name + "' not found in type '" + object_type->get_name() + "'.");
                debugExit("MemberAccessExpr");
                return;
            }
            
            if (auto* typed = member_symbol->as<TypedSymbol>()) {
                nodeTypes[node] = typed->type();
            }
            debugExit("MemberAccessExpr");
        }

        // --- Helper Methods ---
        TypePtr get_node_type(Node* node) {
            auto it = nodeTypes.find(node);
            return (it != nodeTypes.end()) ? it->second : nullptr;
        }

        TypePtr resolve_type_ref(NamedTypeRef* type_ref) {
            std::string name;
            for (size_t i = 0; i < type_ref->path.size(); ++i) {
                if (i > 0) name += ".";
                name += std::string(type_ref->path[i]->text);
            }
            debug("Resolving type ref: " + name);
            if (currentScope.id != 0) {
                return symbolTable.resolve_type_name(name, symbolTable.lookup_handle(currentScope));
            }
            return symbolTable.resolve_type_name(name, nullptr);
        }
    };

} // namespace Myre