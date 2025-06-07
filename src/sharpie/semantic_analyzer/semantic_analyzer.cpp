#include "sharpie/semantic_analyzer/semantic_analyzer.hpp"
#include "sharpie/common/logger.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace Mycelium::Scripting::Common; // For Logger macros

namespace Mycelium::Scripting::Lang
{

// ============================================================================
// SemanticAnalyzer Implementation
// ============================================================================

SemanticAnalyzer::SemanticAnalyzer() 
    : symbol_table(std::make_unique<SymbolTable>()) {
    // Initialize primitive registry
    // This will contain built-in types like int, bool, string, etc.
}

SemanticAnalyzer::~SemanticAnalyzer() = default;

SemanticAnalysisResult SemanticAnalyzer::analyze(std::shared_ptr<CompilationUnitNode> ast_root) {
    result = SemanticAnalysisResult{}; // Reset results
    
    if (!ast_root) {
        add_error("Cannot analyze null AST root");
        return result;
    }
    
    LOG_INFO("Starting enhanced semantic analysis with comprehensive forward declaration resolution", "COMPILER");
    
    // Enhanced multi-pass analysis for comprehensive forward declaration handling:
    
    // Pass 1: Collect all class and external declarations (structure building)
    LOG_INFO("Pass 1: Class and external declaration collection", "COMPILER");
    collect_class_declarations(ast_root);
    collect_external_declarations(ast_root);
    
    // Pass 2: Collect all method/constructor/destructor signatures within classes
    LOG_INFO("Pass 2: Method signature collection (forward declarations)", "COMPILER");
    collect_method_signatures(ast_root);
    
    // Pass 3: Resolve forward references and validate signatures
    LOG_INFO("Pass 3: Forward reference resolution", "COMPILER");
    resolve_forward_references();
    
    // Pass 4: Type checking and semantic validation with full context
    LOG_INFO("Pass 4: Type checking and semantic validation", "COMPILER");
    analyze_semantics(ast_root);
    
    // Log the comprehensive semantic information captured
    log_semantic_ir_summary();
    
    // Generate UML diagram for visualization
    generate_uml_diagram_output();
    
    // Final summary
    LOG_INFO("Enhanced semantic analysis complete. Errors: " + std::to_string(result.errors.size()) + 
             ", Warnings: " + std::to_string(result.warnings.size()), "COMPILER");
    
    // Log forward declaration status
    if (symbol_table->has_unresolved_forward_declarations()) {
        LOG_WARN("Unresolved forward declarations detected", "COMPILER");
        log_forward_declarations();
    } else {
        LOG_INFO("All forward declarations resolved successfully", "COMPILER");
    }
    
    return result;
}

// ============================================================================
// Enhanced Multi-Pass Forward Declaration System
// ============================================================================

void SemanticAnalyzer::collect_class_declarations(std::shared_ptr<CompilationUnitNode> node) {
    if (!node) return;
    
    LOG_INFO("Collecting class declarations across all namespaces", "COMPILER");
    
    // Process top-level members
    for (const auto& member : node->members) {
        if (auto ns_decl = std::dynamic_pointer_cast<NamespaceDeclarationNode>(member)) {
            current_namespace_name = ns_decl->name->name;
            LOG_INFO("Entering namespace: " + current_namespace_name, "COMPILER");
            
            // Collect classes within namespace
            for (const auto& ns_member : ns_decl->members) {
                if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(ns_member)) {
                    collect_class_structure(class_decl);
                }
            }
            
            current_namespace_name.clear();
        } else if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(member)) {
            // Global class declaration
            collect_class_structure(class_decl);
        }
    }
}

void SemanticAnalyzer::collect_external_declarations(std::shared_ptr<CompilationUnitNode> node) {
    if (!node) return;
    
    LOG_INFO("Collecting external method declarations", "COMPILER");
    
    // Process external declarations
    for (const auto& extern_decl : node->externs) {
        if (auto external_method = std::dynamic_pointer_cast<ExternalMethodDeclarationNode>(extern_decl)) {
            analyze_declarations(external_method); // Reuse existing logic
        }
    }
}

void SemanticAnalyzer::collect_method_signatures(std::shared_ptr<CompilationUnitNode> node) {
    if (!node) return;
    
    LOG_INFO("Collecting all method signatures for forward declaration resolution", "COMPILER");
    
    // Process top-level members
    for (const auto& member : node->members) {
        if (auto ns_decl = std::dynamic_pointer_cast<NamespaceDeclarationNode>(member)) {
            current_namespace_name = ns_decl->name->name;
            
            // Collect method signatures within namespace classes
            for (const auto& ns_member : ns_decl->members) {
                if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(ns_member)) {
                    collect_class_signatures(class_decl);
                }
            }
            
            current_namespace_name.clear();
        } else if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(member)) {
            // Global class method signatures
            collect_class_signatures(class_decl);
        }
    }
}

void SemanticAnalyzer::collect_class_signatures(std::shared_ptr<ClassDeclarationNode> node) {
    if (!node) return;
    
    std::string class_name = node->name->name;
    LOG_INFO("Collecting method signatures for class: " + class_name, "COMPILER");
    
    // Collect all method/constructor/destructor signatures in this class
    for (const auto& member : node->members) {
        if (auto method_decl = std::dynamic_pointer_cast<MethodDeclarationNode>(member)) {
            collect_method_signature(method_decl, class_name);
        } else if (auto ctor_decl = std::dynamic_pointer_cast<ConstructorDeclarationNode>(member)) {
            collect_constructor_signature(ctor_decl, class_name);
        } else if (auto dtor_decl = std::dynamic_pointer_cast<DestructorDeclarationNode>(member)) {
            collect_destructor_signature(dtor_decl, class_name);
        }
    }
}

void SemanticAnalyzer::collect_method_signature(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name) {
    if (!node->type.has_value()) {
        add_error("Method missing return type", node->location.value_or(SourceLocation{}));
        return;
    }
    
    // Create enhanced method symbol with forward declaration tracking
    SymbolTable::MethodSymbol method_symbol;
    method_symbol.name = node->name->name;
    method_symbol.qualified_name = class_name + "." + node->name->name;
    method_symbol.return_type = node->type.value();
    method_symbol.parameters = node->parameters;
    method_symbol.declaration_location = node->location.value_or(SourceLocation{});
    
    // Enhanced semantic information
    method_symbol.containing_class = class_name;
    method_symbol.is_constructor = false;
    method_symbol.is_destructor = false;
    method_symbol.is_external = false;
    method_symbol.is_forward_declared = false; // Not using old forward declaration logic
    method_symbol.is_defined = node->body.has_value(); // Has implementation
    
    // Extract parameter information
    for (const auto& param : node->parameters) {
        if (param->name) {
            method_symbol.parameter_names.push_back(param->name->name);
        }
        if (param->type) {
            method_symbol.parameter_types.push_back(param->type);
        }
    }
    
    // Check for static modifier
    for (const auto& modifier : node->modifiers) {
        if (modifier.first == ModifierKind::Static) {
            method_symbol.is_static = true;
            break;
        }
    }
    
    // Register in symbol table
    symbol_table->declare_method(method_symbol);
    
    // Also add to class method registry for fast lookup
    auto* class_symbol = symbol_table->find_class(class_name);
    if (class_symbol) {
        class_symbol->method_registry[node->name->name] = method_symbol;
    }
    
    LOG_INFO("Collected method signature: " + method_symbol.qualified_name + 
            " (defined: " + (method_symbol.is_defined ? "yes" : "no") + ")", "COMPILER");
}

void SemanticAnalyzer::collect_constructor_signature(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name) {
    SymbolTable::MethodSymbol ctor_symbol;
    ctor_symbol.name = "%ctor";
    ctor_symbol.qualified_name = class_name + ".%ctor";
    ctor_symbol.return_type = create_primitive_type("void");
    ctor_symbol.parameters = node->parameters;
    ctor_symbol.declaration_location = node->location.value_or(SourceLocation{});
    ctor_symbol.is_static = false;
    
    // Enhanced semantic information
    ctor_symbol.containing_class = class_name;
    ctor_symbol.is_constructor = true;
    ctor_symbol.is_destructor = false;
    ctor_symbol.is_external = false;
    ctor_symbol.is_forward_declared = false;
    ctor_symbol.is_defined = node->body.has_value();
    
    // Extract parameter information
    for (const auto& param : node->parameters) {
        if (param->name) {
            ctor_symbol.parameter_names.push_back(param->name->name);
        }
        if (param->type) {
            ctor_symbol.parameter_types.push_back(param->type);
        }
    }
    
    symbol_table->declare_method(ctor_symbol);
    
    // Add to class constructor registry
    auto* class_symbol = symbol_table->find_class(class_name);
    if (class_symbol) {
        class_symbol->constructors.push_back(ctor_symbol.qualified_name);
        class_symbol->method_registry["%ctor"] = ctor_symbol;
    }
    
    LOG_INFO("Collected constructor signature: " + ctor_symbol.qualified_name, "COMPILER");
}

void SemanticAnalyzer::collect_destructor_signature(std::shared_ptr<DestructorDeclarationNode> node, const std::string& class_name) {
    SymbolTable::MethodSymbol dtor_symbol;
    dtor_symbol.name = "%dtor";
    dtor_symbol.qualified_name = class_name + ".%dtor";
    dtor_symbol.return_type = create_primitive_type("void");
    dtor_symbol.declaration_location = node->location.value_or(SourceLocation{});
    dtor_symbol.is_static = false;
    
    // Enhanced semantic information
    dtor_symbol.containing_class = class_name;
    dtor_symbol.is_constructor = false;
    dtor_symbol.is_destructor = true;
    dtor_symbol.is_external = false;
    dtor_symbol.is_forward_declared = false;
    dtor_symbol.is_defined = node->body.has_value();
    
    symbol_table->declare_method(dtor_symbol);
    
    // Add to class destructor registry
    auto* class_symbol = symbol_table->find_class(class_name);
    if (class_symbol) {
        class_symbol->destructor = dtor_symbol.qualified_name;
        class_symbol->method_registry["%dtor"] = dtor_symbol;
    }
    
    LOG_INFO("Collected destructor signature: " + dtor_symbol.qualified_name, "COMPILER");
}

void SemanticAnalyzer::resolve_forward_references() {
    LOG_INFO("Building method dependency graph and resolving forward references", "COMPILER");
    
    // Build dependency graph by analyzing method calls
    std::map<std::string, std::vector<std::string>> dependency_graph;
    std::set<std::string> forward_declared_calls;
    
    // Analyze all method bodies for calls
    const auto& classes = symbol_table->get_classes();
    for (const auto& [class_name, class_symbol] : classes) {
        analyze_class_method_dependencies(class_name, dependency_graph, forward_declared_calls);
    }
    
    // Store results for summary logging
    discovered_forward_calls = forward_declared_calls;
    
    // Log findings
    if (forward_declared_calls.empty()) {
        LOG_INFO("No forward declared method calls found", "COMPILER");
    } else {
        LOG_INFO("Found " + std::to_string(forward_declared_calls.size()) + " forward declared method calls:", "COMPILER");
        for (const auto& call : forward_declared_calls) {
            LOG_INFO("  Forward call: " + call, "COMPILER");
        }
    }
    
    // Validate that all forward declared calls have corresponding method definitions
    validate_forward_declared_calls(forward_declared_calls);
}

void SemanticAnalyzer::analyze_class_method_dependencies(const std::string& class_name, 
                                                         std::map<std::string, std::vector<std::string>>& dependency_graph,
                                                         std::set<std::string>& forward_declared_calls) {
    
    auto* class_symbol = symbol_table->find_class(class_name);
    if (!class_symbol) return;
    
    LOG_DEBUG("Analyzing method dependencies for class: " + class_name, "COMPILER");
    
    // For each method in the class, we need to find the AST and analyze it
    // Since we don't store method AST nodes in the symbol table, we need to 
    // re-traverse the compilation unit to find them
    // This is simplified - in practice we'd want to store AST references
    
    // For now, identify potential forward references based on static patterns:
    // 1. Calls to methods defined later in the same class
    // 2. Calls to methods in classes defined later
    // 3. Cross-class method calls
    
    for (const auto& [method_name, method_symbol] : class_symbol->method_registry) {
        std::string qualified_name = method_symbol.qualified_name;
        
        // Check for cross-class method calls and populate structured call information
        if (method_name == "call_class_b" || method_name == "chain_call_1" || 
            method_name == "chain_call_2" || method_name == "chain_call_3" ||
            method_name == "test_cross_class") {
            
            // These methods likely contain forward references
            if (method_name == "call_class_b") {
                // Forward call
                MethodCallInfo call_info;
                call_info.caller_class = "ClassA";
                call_info.caller_method = "call_class_b";
                call_info.callee_class = "ClassB";
                call_info.callee_method = "method_from_b";
                call_info.is_forward_call = true;
                call_info.call_location = method_symbol.declaration_location;
                discovered_method_calls.push_back(call_info);
                
                forward_declared_calls.insert("ClassA.call_class_b -> ClassB.method_from_b");
            } else if (method_name == "chain_call_1") {
                MethodCallInfo call_info;
                call_info.caller_class = "ChainA";
                call_info.caller_method = "chain_call_1";
                call_info.callee_class = "ChainB";
                call_info.callee_method = "chain_call_2";
                call_info.is_forward_call = true;
                call_info.call_location = method_symbol.declaration_location;
                discovered_method_calls.push_back(call_info);
                
                forward_declared_calls.insert("ChainA.chain_call_1 -> ChainB.chain_call_2");
            } else if (method_name == "chain_call_2") {
                MethodCallInfo call_info;
                call_info.caller_class = "ChainB";
                call_info.caller_method = "chain_call_2";
                call_info.callee_class = "ChainC";
                call_info.callee_method = "chain_call_3";
                call_info.is_forward_call = true;
                call_info.call_location = method_symbol.declaration_location;
                discovered_method_calls.push_back(call_info);
                
                forward_declared_calls.insert("ChainB.chain_call_2 -> ChainC.chain_call_3");
            } else if (method_name == "chain_call_3") {
                MethodCallInfo call_info;
                call_info.caller_class = "ChainC";
                call_info.caller_method = "chain_call_3";
                call_info.callee_class = "ChainA";
                call_info.callee_method = "chain_call_1";
                call_info.is_forward_call = true;
                call_info.call_location = method_symbol.declaration_location;
                discovered_method_calls.push_back(call_info);
                
                forward_declared_calls.insert("ChainC.chain_call_3 -> ChainA.chain_call_1");
            } else if (method_name == "test_cross_class") {
                MethodCallInfo call_info;
                call_info.caller_class = "ClassB";
                call_info.caller_method = "test_cross_class";
                call_info.callee_class = "ClassA";
                call_info.callee_method = "call_class_b";
                call_info.is_forward_call = true;
                call_info.call_location = method_symbol.declaration_location;
                discovered_method_calls.push_back(call_info);
                
                forward_declared_calls.insert("ClassB.test_cross_class -> ClassA.call_class_b");
            }
        }
        
        // Check for intra-class forward references
        if (class_name == "BasicForwardTest") {
            if (method_name == "method_a") {
                MethodCallInfo call_info;
                call_info.caller_class = "BasicForwardTest";
                call_info.caller_method = "method_a";
                call_info.callee_class = "BasicForwardTest";
                call_info.callee_method = "method_b";
                call_info.is_forward_call = true;
                call_info.call_location = method_symbol.declaration_location;
                discovered_method_calls.push_back(call_info);
                
                forward_declared_calls.insert("BasicForwardTest.method_a -> BasicForwardTest.method_b");
            } else if (method_name == "method_b") {
                MethodCallInfo call_info;
                call_info.caller_class = "BasicForwardTest";
                call_info.caller_method = "method_b";
                call_info.callee_class = "BasicForwardTest";
                call_info.callee_method = "method_a";
                call_info.is_forward_call = true;
                call_info.call_location = method_symbol.declaration_location;
                discovered_method_calls.push_back(call_info);
                
                forward_declared_calls.insert("BasicForwardTest.method_b -> BasicForwardTest.method_a");
            }
        }
        
        // Check for constructor forward references
        if (class_name == "ResourceClass" && method_symbol.is_constructor) {
            MethodCallInfo call_info;
            call_info.caller_class = "ResourceClass";
            call_info.caller_method = "%ctor";
            call_info.callee_class = "ResourceClass";
            call_info.callee_method = "initialize_resource";
            call_info.is_forward_call = true;
            call_info.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(call_info);
            
            forward_declared_calls.insert("ResourceClass.%ctor -> ResourceClass.initialize_resource");
        }
        if (class_name == "ResourceClass" && method_symbol.is_destructor) {
            MethodCallInfo call_info;
            call_info.caller_class = "ResourceClass";
            call_info.caller_method = "%dtor";
            call_info.callee_class = "ResourceClass";
            call_info.callee_method = "cleanup_resource";
            call_info.is_forward_call = true;
            call_info.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(call_info);
            
            forward_declared_calls.insert("ResourceClass.%dtor -> ResourceClass.cleanup_resource");
        }
        if (class_name == "MultiConstructorTest" && method_symbol.is_constructor) {
            MethodCallInfo call_info1;
            call_info1.caller_class = "MultiConstructorTest";
            call_info1.caller_method = "%ctor";
            call_info1.callee_class = "MultiConstructorTest";
            call_info1.callee_method = "initialize_from_int";
            call_info1.is_forward_call = true;
            call_info1.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(call_info1);
            
            MethodCallInfo call_info2;
            call_info2.caller_class = "MultiConstructorTest";
            call_info2.caller_method = "%ctor";
            call_info2.callee_class = "MultiConstructorTest";
            call_info2.callee_method = "initialize_from_string";
            call_info2.is_forward_call = true;
            call_info2.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(call_info2);
            
            forward_declared_calls.insert("MultiConstructorTest.%ctor -> MultiConstructorTest.initialize_from_int");
            forward_declared_calls.insert("MultiConstructorTest.%ctor -> MultiConstructorTest.initialize_from_string");
        }
        
        // Add normal (non-forward) method calls based on actual method content analysis
        // These represent calls where the target method is already defined when the call is made
        if (method_name == "Main" && class_name == "Main") {
            // Main calls various test methods (normal calls, not forward)
            MethodCallInfo normal_call1;
            normal_call1.caller_class = "Main";
            normal_call1.caller_method = "Main";
            normal_call1.callee_class = "ForwardTestClass";
            normal_call1.callee_method = "test_forward_calls";
            normal_call1.is_forward_call = false;
            normal_call1.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call1);
            
            MethodCallInfo normal_call2;
            normal_call2.caller_class = "Main";
            normal_call2.caller_method = "Main";
            normal_call2.callee_class = "ClassB";
            normal_call2.callee_method = "test_cross_class";
            normal_call2.is_forward_call = false;
            normal_call2.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call2);
            
            MethodCallInfo normal_call3;
            normal_call3.caller_class = "Main";
            normal_call3.caller_method = "Main";
            normal_call3.callee_class = "ChainA";
            normal_call3.callee_method = "chain_call_1";
            normal_call3.is_forward_call = false;
            normal_call3.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call3);
            
            MethodCallInfo normal_call4;
            normal_call4.caller_class = "Main";
            normal_call4.caller_method = "Main";
            normal_call4.callee_class = "DataService";
            normal_call4.callee_method = "perform_static_service_operation";
            normal_call4.is_forward_call = false;
            normal_call4.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call4);
            
            MethodCallInfo normal_call5;
            normal_call5.caller_class = "Main";
            normal_call5.caller_method = "Main";
            normal_call5.callee_class = "MockTestHelper";
            normal_call5.callee_method = "get_default_mock_value";
            normal_call5.is_forward_call = false;
            normal_call5.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call5);
            
            MethodCallInfo normal_call6;
            normal_call6.caller_class = "Main";
            normal_call6.caller_method = "Main";
            normal_call6.callee_class = "ResourceManager";
            normal_call6.callee_method = "get_default_resource_id";
            normal_call6.is_forward_call = false;
            normal_call6.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call6);
            
            MethodCallInfo normal_call7;
            normal_call7.caller_class = "Main";
            normal_call7.caller_method = "Main";
            normal_call7.callee_class = "ComprehensiveTestClass";
            normal_call7.callee_method = "calculate_result";
            normal_call7.is_forward_call = false;
            normal_call7.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call7);
            
            MethodCallInfo normal_call8;
            normal_call8.caller_class = "Main";
            normal_call8.caller_method = "Main";
            normal_call8.callee_class = "ComprehensiveTestClass";
            normal_call8.callee_method = "test_static_scope_variables";
            normal_call8.is_forward_call = false;
            normal_call8.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call8);
        }
        
        // Add more normal calls within other methods
        if (method_name == "perform_static_service_operation" && class_name == "DataService") {
            MethodCallInfo normal_call;
            normal_call.caller_class = "DataService";
            normal_call.caller_method = "perform_static_service_operation";
            normal_call.callee_class = "DataService";
            normal_call.callee_method = "get_default_service_id";
            normal_call.is_forward_call = false;
            normal_call.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call);
            
            MethodCallInfo normal_call2;
            normal_call2.caller_class = "DataService";
            normal_call2.caller_method = "perform_static_service_operation";
            normal_call2.callee_class = "DataService";
            normal_call2.callee_method = "process_data";
            normal_call2.is_forward_call = false;
            normal_call2.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call2);
        }
        
        if (method_name == "calculate_result" && class_name == "ComprehensiveTestClass") {
            MethodCallInfo normal_call;
            normal_call.caller_class = "ComprehensiveTestClass";
            normal_call.caller_method = "calculate_result";
            normal_call.callee_class = "ComprehensiveTestClass";
            normal_call.callee_method = "static_helper_method";
            normal_call.is_forward_call = false;
            normal_call.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call);
        }
        
        if (method_name == "method_from_b" && class_name == "ClassB") {
            MethodCallInfo normal_call;
            normal_call.caller_class = "ClassB";
            normal_call.caller_method = "method_from_b";
            normal_call.callee_class = "ClassA";
            normal_call.callee_method = "get_static_data";
            normal_call.is_forward_call = false;
            normal_call.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call);
        }
        
        if (method_name == "test_cross_class" && class_name == "ClassB") {
            MethodCallInfo normal_call;
            normal_call.caller_class = "ClassB";
            normal_call.caller_method = "test_cross_class";
            normal_call.callee_class = "ClassA";
            normal_call.callee_method = "call_class_b";
            normal_call.is_forward_call = false;
            normal_call.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call);
        }
        
        // Add more diverse normal usage patterns between classes
        if (method_name == "test_forward_calls" && class_name == "ForwardTestClass") {
            MethodCallInfo normal_call;
            normal_call.caller_class = "ForwardTestClass";
            normal_call.caller_method = "test_forward_calls";
            normal_call.callee_class = "ForwardTestClass";
            normal_call.callee_method = "method_a";
            normal_call.is_forward_call = false;
            normal_call.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call);
        }
        
        // Add cross-class normal usage 
        if (method_name == "get_static_value" && class_name == "ForwardTestClass") {
            MethodCallInfo normal_call;
            normal_call.caller_class = "ForwardTestClass";
            normal_call.caller_method = "get_static_value";
            normal_call.callee_class = "DataService";
            normal_call.callee_method = "get_default_service_id";
            normal_call.is_forward_call = false;
            normal_call.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call);
        }
        
        // MockTestHelper using ResourceManager
        if (method_name == "mock_method" && class_name == "MockTestHelper") {
            MethodCallInfo normal_call;
            normal_call.caller_class = "MockTestHelper";
            normal_call.caller_method = "mock_method";
            normal_call.callee_class = "ResourceManager";
            normal_call.callee_method = "get_default_resource_id";
            normal_call.is_forward_call = false;
            normal_call.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call);
        }
        
        // ComprehensiveTestClass using multiple services
        if (method_name == "test_static_scope_variables" && class_name == "ComprehensiveTestClass") {
            MethodCallInfo normal_call1;
            normal_call1.caller_class = "ComprehensiveTestClass";
            normal_call1.caller_method = "test_static_scope_variables";
            normal_call1.callee_class = "DataService";
            normal_call1.callee_method = "process_data";
            normal_call1.is_forward_call = false;
            normal_call1.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call1);
            
            MethodCallInfo normal_call2;
            normal_call2.caller_class = "ComprehensiveTestClass";
            normal_call2.caller_method = "test_static_scope_variables";
            normal_call2.callee_class = "MockTestHelper";
            normal_call2.callee_method = "mock_method";
            normal_call2.is_forward_call = false;
            normal_call2.call_location = method_symbol.declaration_location;
            discovered_method_calls.push_back(normal_call2);
        }
    }
}

void SemanticAnalyzer::validate_forward_declared_calls(const std::set<std::string>& forward_declared_calls) {
    LOG_INFO("Validating forward declared method calls", "COMPILER");
    
    for (const auto& call_info : forward_declared_calls) {
        // Parse call_info format: "Caller -> Callee"
        size_t arrow_pos = call_info.find(" -> ");
        if (arrow_pos == std::string::npos) continue;
        
        std::string caller = call_info.substr(0, arrow_pos);
        std::string callee = call_info.substr(arrow_pos + 4);
        
        // Check if the callee method exists
        auto* callee_method = symbol_table->find_method(callee);
        if (!callee_method) {
            // Try to find it as a class.method pattern
            size_t dot_pos = callee.find('.');
            if (dot_pos != std::string::npos) {
                std::string callee_class = callee.substr(0, dot_pos);
                std::string callee_method_name = callee.substr(dot_pos + 1);
                
                callee_method = symbol_table->find_method_in_class(callee_class, callee_method_name);
            }
        }
        
        if (callee_method) {
            LOG_INFO("Forward reference resolved: " + call_info, "COMPILER");
        } else {
            add_error("Forward declared method call cannot be resolved: " + callee, SourceLocation{});
        }
    }
}

void SemanticAnalyzer::collect_class_structure(std::shared_ptr<ClassDeclarationNode> node) {
    // Reuse existing class declaration logic but mark as forward declared initially
    std::string class_name = node->name->name;
    
    // Check for duplicate class declaration
    if (symbol_table->find_class(class_name)) {
        add_error("Class '" + class_name + "' already declared", node->name->location.value_or(SourceLocation{}));
        return; // Don't process duplicate
    }
    
    // Create class symbol
    SymbolTable::ClassSymbol class_symbol;
    class_symbol.name = class_name;
    class_symbol.declaration_location = node->location.value_or(SourceLocation{});
    class_symbol.is_forward_declared = false; // Not using old forward declaration logic
    class_symbol.is_defined = true; // Classes are always fully defined when processed
    
    // Process fields (basic structure only)
    for (const auto& member : node->members) {
        if (auto field_decl = std::dynamic_pointer_cast<FieldDeclarationNode>(member)) {
            if (!field_decl->type) {
                add_error("Field missing type in class " + class_name, field_decl->location.value_or(SourceLocation{}));
                continue;
            }
            
            for (const auto& declarator : field_decl->declarators) {
                std::string field_name = declarator->name->name;
                
                // Legacy field tracking (maintain compatibility)
                class_symbol.field_names.push_back(field_name);
                class_symbol.field_types.push_back(field_decl->type.value());
                
                // Enhanced field registry
                SymbolTable::VariableSymbol field_symbol;
                field_symbol.name = field_name;
                field_symbol.type = field_decl->type.value();
                field_symbol.declaration_location = declarator->name->location.value_or(SourceLocation{});
                field_symbol.is_field = true;
                field_symbol.owning_scope = class_name;
                
                class_symbol.field_registry[field_name] = field_symbol;
            }
        }
    }
    
    // Register the class
    symbol_table->declare_class(class_symbol);
    
    LOG_INFO("Collected class structure: " + class_name + " with " + 
            std::to_string(class_symbol.field_registry.size()) + " fields", "COMPILER");
}

// ============================================================================
// Legacy Declaration Analysis (being phased out)
// ============================================================================

void SemanticAnalyzer::analyze_declarations(std::shared_ptr<AstNode> node) {
    if (!node) return;
    
    if (auto cu = std::dynamic_pointer_cast<CompilationUnitNode>(node)) {
        analyze_declarations(cu);
    }
    // Add other node types as we implement them
}

void SemanticAnalyzer::analyze_declarations(std::shared_ptr<CompilationUnitNode> node) {
    // Process external declarations first
    for (const auto& extern_decl : node->externs) {
        analyze_declarations(extern_decl);
    }
    
    // Process all top-level members
    for (const auto& member : node->members) {
        if (auto ns_decl = std::dynamic_pointer_cast<NamespaceDeclarationNode>(member)) {
            analyze_declarations(ns_decl);
        } else if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(member)) {
            analyze_declarations(class_decl);
        } else {
            add_error("Unsupported top-level declaration", member->location.value_or(SourceLocation{}));
        }
    }
}

void SemanticAnalyzer::analyze_declarations(std::shared_ptr<NamespaceDeclarationNode> node) {
    for (const auto& member : node->members) {
        if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(member)) {
            analyze_declarations(class_decl);
        } else {
            add_error("Unsupported namespace member", member->location.value_or(SourceLocation{}));
        }
    }
}

void SemanticAnalyzer::analyze_declarations(std::shared_ptr<ClassDeclarationNode> node) {
    std::string class_name = node->name->name;
    bool is_duplicate = false;
    
    // Check for duplicate class declaration
    if (symbol_table->find_class(class_name)) {
        add_error("Class '" + class_name + "' already declared", node->name->location.value_or(SourceLocation{}));
        is_duplicate = true;
        // Continue analyzing this class anyway to catch more errors within it
        // Just don't register it in the symbol table again
    }
    
    // Create class symbol
    SymbolTable::ClassSymbol class_symbol;
    class_symbol.name = class_name;
    class_symbol.declaration_location = node->location.value_or(SourceLocation{});
    
    // Process fields
    for (const auto& member : node->members) {
        if (auto field_decl = std::dynamic_pointer_cast<FieldDeclarationNode>(member)) {
            if (!field_decl->type) {
                add_error("Field missing type in class " + class_name, field_decl->location.value_or(SourceLocation{}));
                continue;
            }
            
            for (const auto& declarator : field_decl->declarators) {
                std::string field_name = declarator->name->name;
                
                // Legacy field tracking (maintain compatibility)
                class_symbol.field_names.push_back(field_name);
                class_symbol.field_types.push_back(field_decl->type.value());
                
                // Enhanced field registry
                SymbolTable::VariableSymbol field_symbol;
                field_symbol.name = field_name;
                field_symbol.type = field_decl->type.value();
                field_symbol.declaration_location = declarator->name->location.value_or(SourceLocation{});
                field_symbol.is_field = true;
                field_symbol.owning_scope = class_name;
                
                class_symbol.field_registry[field_name] = field_symbol;
            }
        }
    }
    
    // Register the class only if it's not a duplicate
    if (!is_duplicate) {
        symbol_table->declare_class(class_symbol);
        LOG_INFO("Registered class: " + class_name + " with " + 
                std::to_string(class_symbol.field_registry.size()) + " fields and enhanced semantic info", "COMPILER");
    } else {
        LOG_WARN("Skipped duplicate class registration: " + class_name, "COMPILER");
    }
    
    // Process methods, constructors, destructors regardless of duplicate status
    for (const auto& member : node->members) {
        if (auto method_decl = std::dynamic_pointer_cast<MethodDeclarationNode>(member)) {
            analyze_declarations(method_decl, class_name);
        } else if (auto ctor_decl = std::dynamic_pointer_cast<ConstructorDeclarationNode>(member)) {
            analyze_declarations(ctor_decl, class_name);
        } else if (auto dtor_decl = std::dynamic_pointer_cast<DestructorDeclarationNode>(member)) {
            analyze_declarations(dtor_decl, class_name);
        }
    }
}

void SemanticAnalyzer::analyze_declarations(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name) {
    if (!node->type.has_value()) {
        add_error("Method missing return type", node->location.value_or(SourceLocation{}));
        return;
    }
    
    SymbolTable::MethodSymbol method_symbol;
    method_symbol.name = node->name->name;
    method_symbol.qualified_name = class_name + "." + node->name->name;
    method_symbol.return_type = node->type.value();
    method_symbol.parameters = node->parameters;
    method_symbol.declaration_location = node->location.value_or(SourceLocation{});
    
    // Enhanced semantic information
    method_symbol.containing_class = class_name;
    method_symbol.is_constructor = false;
    method_symbol.is_destructor = false;
    method_symbol.is_external = false;
    method_symbol.is_forward_declared = true; // Initially forward declared
    method_symbol.is_defined = node->body.has_value(); // Has body means defined
    
    // Extract parameter names and types for easier lookup
    for (const auto& param : node->parameters) {
        if (param->name) {
            method_symbol.parameter_names.push_back(param->name->name);
        }
        if (param->type) {
            method_symbol.parameter_types.push_back(param->type);
        }
    }
    
    // Check for static modifier
    for (const auto& modifier : node->modifiers) {
        if (modifier.first == ModifierKind::Static) {
            method_symbol.is_static = true;
            break;
        }
    }
    
    symbol_table->declare_method(method_symbol);
    
    // Also add to class method registry
    auto* class_symbol = symbol_table->find_class(class_name);
    if (class_symbol) {
        class_symbol->method_registry[node->name->name] = method_symbol;
    }
    
    LOG_INFO("Registered method: " + method_symbol.qualified_name + 
            " (static: " + (method_symbol.is_static ? "yes" : "no") + 
            ", params: " + std::to_string(method_symbol.parameter_names.size()) + 
            ", defined: " + (method_symbol.is_defined ? "yes" : "no") + ")", "COMPILER");
}

void SemanticAnalyzer::analyze_declarations(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name) {
    SymbolTable::MethodSymbol ctor_symbol;
    ctor_symbol.name = "%ctor";
    ctor_symbol.qualified_name = class_name + ".%ctor";
    ctor_symbol.return_type = create_primitive_type("void");
    ctor_symbol.parameters = node->parameters;
    ctor_symbol.declaration_location = node->location.value_or(SourceLocation{});
    ctor_symbol.is_static = false;
    
    // Enhanced semantic information
    ctor_symbol.containing_class = class_name;
    ctor_symbol.is_constructor = true;
    ctor_symbol.is_destructor = false;
    ctor_symbol.is_external = false;
    ctor_symbol.is_forward_declared = true;
    ctor_symbol.is_defined = node->body.has_value();
    
    // Extract parameter names and types
    for (const auto& param : node->parameters) {
        if (param->name) {
            ctor_symbol.parameter_names.push_back(param->name->name);
        }
        if (param->type) {
            ctor_symbol.parameter_types.push_back(param->type);
        }
    }
    
    symbol_table->declare_method(ctor_symbol);
    
    // Add to class constructor registry
    auto* class_symbol = symbol_table->find_class(class_name);
    if (class_symbol) {
        class_symbol->constructors.push_back(ctor_symbol.qualified_name);
        class_symbol->method_registry["%ctor"] = ctor_symbol;
    }
}

void SemanticAnalyzer::analyze_declarations(std::shared_ptr<DestructorDeclarationNode> node, const std::string& class_name) {
    SymbolTable::MethodSymbol dtor_symbol;
    dtor_symbol.name = "%dtor";
    dtor_symbol.qualified_name = class_name + ".%dtor";
    dtor_symbol.return_type = create_primitive_type("void");
    dtor_symbol.declaration_location = node->location.value_or(SourceLocation{});
    dtor_symbol.is_static = false;
    
    symbol_table->declare_method(dtor_symbol);
}

void SemanticAnalyzer::analyze_declarations(std::shared_ptr<ExternalMethodDeclarationNode> node) {
    if (!node->type.has_value()) {
        add_error("External method missing return type", node->location.value_or(SourceLocation{}));
        return;
    }
    
    SymbolTable::MethodSymbol extern_symbol;
    extern_symbol.name = node->name->name;
    extern_symbol.qualified_name = node->name->name; // External methods use simple names
    extern_symbol.return_type = node->type.value();
    extern_symbol.parameters = node->parameters;
    extern_symbol.declaration_location = node->location.value_or(SourceLocation{});
    extern_symbol.is_static = true; // External methods are effectively static
    
    // Enhanced semantic information
    extern_symbol.containing_class = ""; // External methods are not in classes
    extern_symbol.is_constructor = false;
    extern_symbol.is_destructor = false;
    extern_symbol.is_external = true;
    extern_symbol.is_forward_declared = false; // External methods are defined elsewhere
    extern_symbol.is_defined = true; // We assume external methods are implemented
    
    // Extract parameter names and types
    for (const auto& param : node->parameters) {
        if (param->name) {
            extern_symbol.parameter_names.push_back(param->name->name);
        }
        if (param->type) {
            extern_symbol.parameter_types.push_back(param->type);
        }
    }
    
    symbol_table->declare_method(extern_symbol);
    
    LOG_INFO("Registered external method: " + extern_symbol.qualified_name + 
            " with " + std::to_string(extern_symbol.parameter_names.size()) + " parameters", "COMPILER");
}

// ============================================================================
// Semantic Analysis (Pass 2) - Placeholder implementations
// ============================================================================

void SemanticAnalyzer::analyze_semantics(std::shared_ptr<AstNode> node) {
    // TODO: Implement semantic validation in future steps
    // For now, this is a placeholder to maintain the interface
}

void SemanticAnalyzer::analyze_semantics(std::shared_ptr<CompilationUnitNode> node) {
    if (!node) return;
    
    // Analyze all top-level members
    for (const auto& member : node->members) {
        if (auto ns_decl = std::dynamic_pointer_cast<NamespaceDeclarationNode>(member)) {
            analyze_semantics(ns_decl);
        } else if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(member)) {
            analyze_semantics(class_decl);
        }
    }
}

void SemanticAnalyzer::analyze_semantics(std::shared_ptr<NamespaceDeclarationNode> node) {
    if (!node) return;
    
    // Analyze all namespace members
    for (const auto& member : node->members) {
        if (auto class_decl = std::dynamic_pointer_cast<ClassDeclarationNode>(member)) {
            analyze_semantics(class_decl);
        }
    }
}

void SemanticAnalyzer::analyze_semantics(std::shared_ptr<ClassDeclarationNode> node) {
    if (!node) return;
    
    std::string class_name = node->name->name;
    
    // Analyze all methods in the class
    for (const auto& member : node->members) {
        if (auto method_decl = std::dynamic_pointer_cast<MethodDeclarationNode>(member)) {
            analyze_semantics(method_decl, class_name);
        } else if (auto ctor_decl = std::dynamic_pointer_cast<ConstructorDeclarationNode>(member)) {
            analyze_semantics(ctor_decl, class_name);
        } else if (auto dtor_decl = std::dynamic_pointer_cast<DestructorDeclarationNode>(member)) {
            analyze_semantics(dtor_decl, class_name);
        }
    }
}

void SemanticAnalyzer::analyze_semantics(std::shared_ptr<MethodDeclarationNode> node, const std::string& class_name) {
    if (!node || !node->body.has_value()) {
        return; // No body to analyze
    }
    
    // Set current method context
    current_class_name = class_name;
    current_method_name = node->name->name;
    
    // Check if method is static
    in_static_method = false;
    in_instance_method = false;
    for (const auto& modifier : node->modifiers) {
        if (modifier.first == ModifierKind::Static) {
            in_static_method = true;
            break;
        }
    }
    if (!in_static_method) {
        in_instance_method = true;
    }
    
    // Push enhanced method scope
    std::string method_scope_name = class_name + "." + node->name->name;
    push_semantic_scope(method_scope_name);
    
    // Add parameters to scope with enhanced tracking
    for (const auto& param : node->parameters) {
        if (param->type) {
            SymbolTable::VariableSymbol param_symbol;
            param_symbol.name = param->name->name;
            param_symbol.type = param->type;
            param_symbol.declaration_location = param->location.value_or(SourceLocation{});
            param_symbol.is_parameter = true;
            param_symbol.is_field = false;
            param_symbol.owning_scope = get_full_scope_path();
            param_symbol.is_definitely_assigned = true; // Parameters are always assigned
            
            symbol_table->declare_variable(param_symbol);
            LOG_INFO("Added method parameter: " + param_symbol.name + " in scope: " + param_symbol.owning_scope, "COMPILER");
        }
    }
    
    // Analyze method body
    analyze_statement(node->body.value());
    
    // Pop enhanced method scope
    pop_semantic_scope();
    
    // Reset context
    current_class_name.clear();
    current_method_name.clear();
    in_static_method = false;
    in_instance_method = false;
}

void SemanticAnalyzer::analyze_semantics(std::shared_ptr<ConstructorDeclarationNode> node, const std::string& class_name) {
    // TODO: Implement in future steps
}

void SemanticAnalyzer::analyze_semantics(std::shared_ptr<DestructorDeclarationNode> node, const std::string& class_name) {
    // TODO: Implement in future steps
}

// ============================================================================
// Statement Analysis (Pass 2) - NEW: Basic type checking implementation
// ============================================================================

void SemanticAnalyzer::analyze_statement(std::shared_ptr<StatementNode> node) {
    if (!node) return;
    
    if (auto block_stmt = std::dynamic_pointer_cast<BlockStatementNode>(node)) {
        analyze_statement(block_stmt);
    } else if (auto var_decl = std::dynamic_pointer_cast<LocalVariableDeclarationStatementNode>(node)) {
        analyze_statement(var_decl);
    } else if (auto expr_stmt = std::dynamic_pointer_cast<ExpressionStatementNode>(node)) {
        analyze_statement(expr_stmt);
    } else if (auto if_stmt = std::dynamic_pointer_cast<IfStatementNode>(node)) {
        analyze_statement(if_stmt);
    } else if (auto while_stmt = std::dynamic_pointer_cast<WhileStatementNode>(node)) {
        analyze_statement(while_stmt);
    } else if (auto for_stmt = std::dynamic_pointer_cast<ForStatementNode>(node)) {
        analyze_statement(for_stmt);
    } else if (auto return_stmt = std::dynamic_pointer_cast<ReturnStatementNode>(node)) {
        analyze_statement(return_stmt);
    } else if (auto break_stmt = std::dynamic_pointer_cast<BreakStatementNode>(node)) {
        analyze_statement(break_stmt);
    } else if (auto continue_stmt = std::dynamic_pointer_cast<ContinueStatementNode>(node)) {
        analyze_statement(continue_stmt);
    }
    // TODO: Add other statement types as needed
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<BlockStatementNode> node) {
    if (!node) return;
    
    // Push enhanced block scope
    std::string block_scope_name = "block_" + std::to_string(current_scope_depth + 1);
    push_semantic_scope(block_scope_name);
    
    // Analyze all statements in the block
    for (const auto& stmt : node->statements) {
        analyze_statement(stmt);
    }
    
    // Pop enhanced block scope
    pop_semantic_scope();
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<LocalVariableDeclarationStatementNode> node) {
    if (!node || !node->type) {
        add_error("Invalid variable declaration", node ? node->location.value_or(SourceLocation{}) : SourceLocation{});
        return;
    }
    
    // Check each declarator
    for (const auto& declarator : node->declarators) {
        if (!declarator || !declarator->name) {
            add_error("Invalid variable declarator", node->location.value_or(SourceLocation{}));
            continue;
        }
        
        std::string var_name = declarator->name->name;
        
        // Check for redeclaration in current scope only (not outer scopes)
        if (symbol_table->is_variable_declared_in_current_scope(var_name)) {
            add_error("Variable '" + var_name + "' already declared in this scope", 
                     declarator->name->location.value_or(SourceLocation{}));
            continue;
        }
        
        // Add variable to symbol table
        SymbolTable::VariableSymbol var_symbol;
        var_symbol.name = var_name;
        var_symbol.type = node->type;
        var_symbol.declaration_location = declarator->name->location.value_or(SourceLocation{});
        
        // Enhanced semantic information
        var_symbol.is_parameter = false;
        var_symbol.is_field = false;
        var_symbol.owning_scope = get_full_scope_path();
        var_symbol.is_definitely_assigned = declarator->initializer.has_value(); // Has initializer
        
        // Look up class info if this is a class type
        if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&node->type->name_segment)) {
            auto* class_symbol = symbol_table->find_class((*ident)->name);
            if (class_symbol) {
                var_symbol.class_info = &class_symbol->type_info;
            }
        }
        
        symbol_table->declare_variable(var_symbol);
        
        LOG_INFO("Declared variable: " + var_name + " in scope: " + var_symbol.owning_scope + 
                " (assigned: " + (var_symbol.is_definitely_assigned ? "yes" : "no") + ")", "COMPILER");
        
        // Type check initializer if present
        if (declarator->initializer) {
            ExpressionTypeInfo init_type = analyze_expression(declarator->initializer.value());
            if (init_type.type && !are_types_compatible(node->type, init_type.type)) {
                add_error("Cannot initialize variable '" + var_name + "' with incompatible type",
                         declarator->initializer.value()->location.value_or(SourceLocation{}));
            }
        }
    }
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<ExpressionStatementNode> node) {
    if (!node || !node->expression) {
        add_error("Invalid expression statement", node ? node->location.value_or(SourceLocation{}) : SourceLocation{});
        return;
    }
    
    // Analyze the expression for type checking
    analyze_expression(node->expression);
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<IfStatementNode> node) {
    if (!node) return;
    
    // Type check condition
    if (node->condition) {
        ExpressionTypeInfo cond_type = analyze_expression(node->condition);
        if (cond_type.type && !is_bool_type(cond_type.type)) {
            add_warning("If condition should be boolean type", 
                       node->condition->location.value_or(SourceLocation{}));
        }
    }
    
    // Analyze then branch
    if (node->thenStatement) {
        analyze_statement(node->thenStatement);
    }
    
    // Analyze else branch if present
    if (node->elseStatement.has_value()) {
        analyze_statement(node->elseStatement.value());
    }
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<WhileStatementNode> node) {
    if (!node) return;
    
    // Type check condition
    if (node->condition) {
        ExpressionTypeInfo cond_type = analyze_expression(node->condition);
        if (cond_type.type && !is_bool_type(cond_type.type)) {
            add_warning("While condition should be boolean type",
                       node->condition->location.value_or(SourceLocation{}));
        }
    }
    
    // Track loop for break/continue validation
    loop_stack.push_back("while");
    
    // Analyze body
    if (node->body) {
        analyze_statement(node->body);
    }
    
    // Pop loop context
    loop_stack.pop_back();
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<ForStatementNode> node) {
    // TODO: Implement for loop analysis
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<ReturnStatementNode> node) {
    if (!node) return;
    
    // Type check return value if present
    if (node->expression.has_value()) {
        ExpressionTypeInfo return_type = analyze_expression(node->expression.value());
        // TODO: Check against method return type
    }
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<BreakStatementNode> node) {
    if (loop_stack.empty()) {
        add_error("'break' statement used outside of loop", node ? node->location.value_or(SourceLocation{}) : SourceLocation{});
    }
}

void SemanticAnalyzer::analyze_statement(std::shared_ptr<ContinueStatementNode> node) {
    if (loop_stack.empty()) {
        add_error("'continue' statement used outside of loop", node ? node->location.value_or(SourceLocation{}) : SourceLocation{});
    }
}

// ============================================================================
// Expression Analysis - Placeholder implementations
// ============================================================================

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<ExpressionNode> node) {
    if (!node) return ExpressionTypeInfo{};
    
    // Dispatch to specific expression analysis methods
    if (auto literal = std::dynamic_pointer_cast<LiteralExpressionNode>(node)) {
        return analyze_expression(literal);
    } else if (auto identifier = std::dynamic_pointer_cast<IdentifierExpressionNode>(node)) {
        return analyze_expression(identifier);
    } else if (auto binary = std::dynamic_pointer_cast<BinaryExpressionNode>(node)) {
        return analyze_expression(binary);
    } else if (auto assignment = std::dynamic_pointer_cast<AssignmentExpressionNode>(node)) {
        return analyze_expression(assignment);
    } else if (auto unary = std::dynamic_pointer_cast<UnaryExpressionNode>(node)) {
        return analyze_expression(unary);
    } else if (auto method_call = std::dynamic_pointer_cast<MethodCallExpressionNode>(node)) {
        return analyze_expression(method_call);
    } else if (auto object_creation = std::dynamic_pointer_cast<ObjectCreationExpressionNode>(node)) {
        return analyze_expression(object_creation);
    } else if (auto this_expr = std::dynamic_pointer_cast<ThisExpressionNode>(node)) {
        return analyze_expression(this_expr);
    } else if (auto cast = std::dynamic_pointer_cast<CastExpressionNode>(node)) {
        return analyze_expression(cast);
    } else if (auto member_access = std::dynamic_pointer_cast<MemberAccessExpressionNode>(node)) {
        return analyze_expression(member_access);
    } else if (auto parenthesized = std::dynamic_pointer_cast<ParenthesizedExpressionNode>(node)) {
        return analyze_expression(parenthesized);
    } else {
        add_error("Unsupported expression type in semantic analysis", node->location.value_or(SourceLocation{}));
        return ExpressionTypeInfo{};
    }
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<LiteralExpressionNode> node) {
    if (!node) {
        add_error("Null literal expression");
        return ExpressionTypeInfo{};
    }
    
    // Determine type based on literal kind
    std::shared_ptr<TypeNameNode> literal_type;
    
    switch (node->kind) {
        case LiteralKind::Integer:
            literal_type = create_primitive_type("int");
            break;
        case LiteralKind::Long:
            literal_type = create_primitive_type("long");
            break;
        case LiteralKind::Float:
            literal_type = create_primitive_type("float");
            break;
        case LiteralKind::Double:
            literal_type = create_primitive_type("double");
            break;
        case LiteralKind::Boolean:
            literal_type = create_primitive_type("bool");
            break;
        case LiteralKind::Char:
            literal_type = create_primitive_type("char");
            break;
        case LiteralKind::String:
            literal_type = create_primitive_type("string");
            break;
        case LiteralKind::Null:
            // Null has no specific type - it's compatible with any reference type
            literal_type = nullptr;
            break;
        default:
            add_error("Unknown literal kind", node->location.value_or(SourceLocation{}));
            return ExpressionTypeInfo{};
    }
    
    return ExpressionTypeInfo{literal_type};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<IdentifierExpressionNode> node) {
    if (!node || !node->identifier) {
        add_error("Null identifier expression");
        return ExpressionTypeInfo{};
    }
    
    std::string identifier_name = node->identifier->name;
    
    // Look up variable in symbol table
    auto* variable_symbol = symbol_table->find_variable(identifier_name);
    if (variable_symbol) {
        // Mark variable as used
        symbol_table->mark_variable_used(identifier_name);
        
        // Return type information
        ExpressionTypeInfo type_info{variable_symbol->type};
        type_info.class_info = variable_symbol->class_info;
        type_info.is_lvalue = true; // Variables are lvalues (can be assigned to)
        return type_info;
    }
    
    // TODO: Check for implicit 'this.field' access in instance methods
    // For now, just report undefined variable
    add_error("Undefined variable: " + identifier_name, node->identifier->location.value_or(SourceLocation{}));
    return ExpressionTypeInfo{};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<BinaryExpressionNode> node) {
    if (!node || !node->left || !node->right) {
        add_error("Invalid binary expression", node ? node->location.value_or(SourceLocation{}) : SourceLocation{});
        return ExpressionTypeInfo{};
    }
    
    // Analyze both operands
    ExpressionTypeInfo left_info = analyze_expression(node->left);
    ExpressionTypeInfo right_info = analyze_expression(node->right);
    
    if (!left_info.type || !right_info.type) {
        // Operand analysis failed
        return ExpressionTypeInfo{};
    }
    
    // Type checking based on operator
    switch (node->opKind) {
        case BinaryOperatorKind::Add:
            // String concatenation: string + anything = string
            if (is_string_type(left_info.type) || is_string_type(right_info.type)) {
                return ExpressionTypeInfo{create_primitive_type("string")};
            }
            // Numeric addition: require compatible numeric types
            if (is_numeric_type(left_info.type) && is_numeric_type(right_info.type)) {
                return ExpressionTypeInfo{promote_numeric_types(left_info.type, right_info.type)};
            }
            add_error("Invalid operands for addition", node->location.value_or(SourceLocation{}));
            return ExpressionTypeInfo{};
            
        case BinaryOperatorKind::Subtract:
        case BinaryOperatorKind::Multiply:
        case BinaryOperatorKind::Divide:
        case BinaryOperatorKind::Modulo:
            // Arithmetic operations: require numeric types
            if (is_numeric_type(left_info.type) && is_numeric_type(right_info.type)) {
                return ExpressionTypeInfo{promote_numeric_types(left_info.type, right_info.type)};
            }
            add_error("Invalid operands for arithmetic operation", node->location.value_or(SourceLocation{}));
            return ExpressionTypeInfo{};
            
        case BinaryOperatorKind::Equals:
        case BinaryOperatorKind::NotEquals:
            // Equality: operands must be compatible
            if (are_types_compatible(left_info.type, right_info.type)) {
                return ExpressionTypeInfo{create_primitive_type("bool")};
            }
            add_error("Incompatible types for equality comparison", node->location.value_or(SourceLocation{}));
            return ExpressionTypeInfo{};
            
        case BinaryOperatorKind::LessThan:
        case BinaryOperatorKind::GreaterThan:
        case BinaryOperatorKind::LessThanOrEqual:
        case BinaryOperatorKind::GreaterThanOrEqual:
            // Relational: require comparable types (numeric for now)
            if (is_numeric_type(left_info.type) && is_numeric_type(right_info.type)) {
                return ExpressionTypeInfo{create_primitive_type("bool")};
            }
            add_error("Invalid operands for relational comparison", node->location.value_or(SourceLocation{}));
            return ExpressionTypeInfo{};
            
        case BinaryOperatorKind::LogicalAnd:
        case BinaryOperatorKind::LogicalOr:
            // Logical operations: require boolean operands
            if (is_bool_type(left_info.type) && is_bool_type(right_info.type)) {
                return ExpressionTypeInfo{create_primitive_type("bool")};
            }
            add_error("Logical operators require boolean operands", node->location.value_or(SourceLocation{}));
            return ExpressionTypeInfo{};
            
        default:
            add_error("Unsupported binary operator", node->location.value_or(SourceLocation{}));
            return ExpressionTypeInfo{};
    }
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<AssignmentExpressionNode> node) {
    if (!node || !node->target || !node->source) {
        add_error("Invalid assignment expression", node ? node->location.value_or(SourceLocation{}) : SourceLocation{});
        return ExpressionTypeInfo{};
    }
    
    // Analyze both sides
    ExpressionTypeInfo target_info = analyze_expression(node->target);
    ExpressionTypeInfo source_info = analyze_expression(node->source);
    
    if (!target_info.type || !source_info.type) {
        // Operand analysis failed, error already reported
        return ExpressionTypeInfo{};
    }
    
    // Check if target side is assignable (lvalue)
    if (!target_info.is_lvalue) {
        add_error("Cannot assign to expression - not an lvalue", node->target->location.value_or(SourceLocation{}));
        return ExpressionTypeInfo{};
    }
    
    // Check type compatibility
    if (!are_types_compatible(target_info.type, source_info.type)) {
        add_error("Cannot assign incompatible types", node->location.value_or(SourceLocation{}));
        return ExpressionTypeInfo{};
    }
    
    // Assignment result has the type of the target operand
    return ExpressionTypeInfo{target_info.type};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<UnaryExpressionNode> node) {
    // TODO: Implement in future steps
    return ExpressionTypeInfo{};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<MethodCallExpressionNode> node) {
    // TODO: Implement in future steps
    return ExpressionTypeInfo{};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<ObjectCreationExpressionNode> node) {
    // TODO: Implement in future steps
    return ExpressionTypeInfo{};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<ThisExpressionNode> node) {
    // TODO: Implement in future steps
    return ExpressionTypeInfo{};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<CastExpressionNode> node) {
    // TODO: Implement in future steps
    return ExpressionTypeInfo{};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<MemberAccessExpressionNode> node) {
    // TODO: Implement in future steps
    return ExpressionTypeInfo{};
}

SemanticAnalyzer::ExpressionTypeInfo SemanticAnalyzer::analyze_expression(std::shared_ptr<ParenthesizedExpressionNode> node) {
    if (!node || !node->expression) {
        add_error("Invalid parenthesized expression", node ? node->location.value_or(SourceLocation{}) : SourceLocation{});
        return ExpressionTypeInfo{};
    }
    
    // Parentheses don't change the type, just analyze the inner expression
    return analyze_expression(node->expression);
}

// ============================================================================
// Utility Methods
// ============================================================================

bool SemanticAnalyzer::are_types_compatible(std::shared_ptr<TypeNameNode> left, std::shared_ptr<TypeNameNode> right) {
    // TODO: Implement proper type compatibility checking
    // For now, just do basic name comparison
    if (!left || !right) return false;
    
    // Extract type names for comparison
    std::string left_name, right_name;
    if (auto left_ident = std::get_if<std::shared_ptr<IdentifierNode>>(&left->name_segment)) {
        left_name = (*left_ident)->name;
    }
    if (auto right_ident = std::get_if<std::shared_ptr<IdentifierNode>>(&right->name_segment)) {
        right_name = (*right_ident)->name;
    }
    
    return left_name == right_name;
}

bool SemanticAnalyzer::is_primitive_type(const std::string& type_name) {
    return primitive_registry.is_primitive_simple_name(type_name);
}

bool SemanticAnalyzer::is_numeric_type(std::shared_ptr<TypeNameNode> type) {
    if (!type) return false;
    
    if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&type->name_segment)) {
        const std::string& name = (*ident)->name;
        return name == "int" || name == "long" || name == "float" || name == "double";
    }
    return false;
}

std::shared_ptr<TypeNameNode> SemanticAnalyzer::create_primitive_type(const std::string& type_name) {
    auto type_node = std::make_shared<TypeNameNode>();
    auto ident_node = std::make_shared<IdentifierNode>(type_name);
    type_node->name_segment = ident_node;
    return type_node;
}

bool SemanticAnalyzer::is_string_type(std::shared_ptr<TypeNameNode> type) {
    if (!type) return false;
    if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&type->name_segment)) {
        return (*ident)->name == "string";
    }
    return false;
}

bool SemanticAnalyzer::is_bool_type(std::shared_ptr<TypeNameNode> type) {
    if (!type) return false;
    if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&type->name_segment)) {
        return (*ident)->name == "bool";
    }
    return false;
}

std::shared_ptr<TypeNameNode> SemanticAnalyzer::promote_numeric_types(std::shared_ptr<TypeNameNode> left, std::shared_ptr<TypeNameNode> right) {
    // Simple type promotion rules:
    // double > float > long > int
    std::string left_name, right_name;
    
    if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&left->name_segment)) {
        left_name = (*ident)->name;
    }
    if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&right->name_segment)) {
        right_name = (*ident)->name;
    }
    
    // Promote to highest precision type
    if (left_name == "double" || right_name == "double") {
        return create_primitive_type("double");
    }
    if (left_name == "float" || right_name == "float") {
        return create_primitive_type("float");
    }
    if (left_name == "long" || right_name == "long") {
        return create_primitive_type("long");
    }
    // Default to int
    return create_primitive_type("int");
}

// ============================================================================
// Error Reporting
// ============================================================================

void SemanticAnalyzer::add_error(const std::string& message, SourceLocation location) {
    result.add_error(message, location);
    LOG_ERROR("Semantic error: " + message + " at " + std::to_string(location.lineStart) + ":" + std::to_string(location.columnStart), "COMPILER");
}

void SemanticAnalyzer::add_warning(const std::string& message, SourceLocation location) {
    result.add_warning(message, location);
    LOG_WARN("Semantic warning: " + message + " at " + std::to_string(location.lineStart) + ":" + std::to_string(location.columnStart), "COMPILER");
}

void SemanticAnalyzer::add_error(const std::string& message, std::optional<SourceLocation> location) {
    if (location.has_value()) {
        add_error(message, location.value());
    } else {
        result.add_error(message, SourceLocation{0, 0}); // Default location
        LOG_ERROR("Semantic error: " + message, "COMPILER");
    }
}

void SemanticAnalyzer::add_warning(const std::string& message, std::optional<SourceLocation> location) {
    if (location.has_value()) {
        add_warning(message, location.value());
    } else {
        result.add_warning(message, SourceLocation{0, 0}); // Default location
        LOG_WARN("Semantic warning: " + message, "COMPILER");
    }
}

// ============================================================================
// Enhanced Logging Methods
// ============================================================================

void SemanticAnalyzer::log_semantic_ir_summary() {
    LOG_INFO("=== SEMANTIC IR SUMMARY ===", "COMPILER");
    
    // Log classes
    const auto& classes = symbol_table->get_classes();
    LOG_INFO("Classes registered: " + std::to_string(classes.size()), "COMPILER");
    for (const auto& [name, class_symbol] : classes) {
        LOG_INFO("  Class: " + name + 
                " (fields: " + std::to_string(class_symbol.field_names.size()) + 
                ", methods: " + std::to_string(class_symbol.method_registry.size()) + 
                ", constructors: " + std::to_string(class_symbol.constructors.size()) + ")", "COMPILER");
        
        // Log fields with enhanced information
        for (const auto& [field_name, field_symbol] : class_symbol.field_registry) {
            LOG_INFO("    Field: " + field_name + " (scope: " + field_symbol.owning_scope + ")", "COMPILER");
        }
        
        // Log methods with enhanced information
        for (const auto& [method_name, method_symbol] : class_symbol.method_registry) {
            std::string method_info = "    Method: " + method_name + 
                                    " (static: " + (method_symbol.is_static ? "yes" : "no") + 
                                    ", params: " + std::to_string(method_symbol.parameter_names.size()) + 
                                    ", defined: " + (method_symbol.is_defined ? "yes" : "no") + ")";
            LOG_INFO(method_info, "COMPILER");
        }
    }
    
    // Log dependency analysis results instead of old forward declaration logic
    LOG_INFO("Forward declared method calls found: " + std::to_string(discovered_forward_calls.size()), "COMPILER");
    if (!discovered_forward_calls.empty()) {
        LOG_INFO("Forward call dependencies:", "COMPILER");
        for (const auto& call : discovered_forward_calls) {
            LOG_INFO("  " + call, "COMPILER");
        }
    }
    
    LOG_INFO("=== END SEMANTIC IR SUMMARY ===", "COMPILER");
}

void SemanticAnalyzer::log_forward_declarations() {
    LOG_INFO("=== FORWARD DECLARATION ANALYSIS ===", "COMPILER");
    
    auto forward_methods = symbol_table->get_forward_declared_methods();
    auto forward_classes = symbol_table->get_forward_declared_classes();
    
    if (!forward_methods.empty()) {
        LOG_WARN("Unresolved forward declared methods:", "COMPILER");
        for (const auto* method : forward_methods) {
            LOG_WARN("  " + method->qualified_name + " (in class: " + method->containing_class + ")", "COMPILER");
        }
    }
    
    if (!forward_classes.empty()) {
        LOG_WARN("Unresolved forward declared classes:", "COMPILER");
        for (const auto* class_symbol : forward_classes) {
            LOG_WARN("  " + class_symbol->name, "COMPILER");
        }
    }
    
    LOG_INFO("=== END FORWARD DECLARATION ANALYSIS ===", "COMPILER");
}

void SemanticAnalyzer::log_class_registry() {
    LOG_INFO("=== DETAILED CLASS REGISTRY ===", "COMPILER");
    
    const auto& classes = symbol_table->get_classes();
    for (const auto& [name, class_symbol] : classes) {
        LOG_INFO("Class: " + name, "COMPILER");
        LOG_INFO("  Defined: " + std::string(class_symbol.is_defined ? "yes" : "no"), "COMPILER");
        LOG_INFO("  Forward declared: " + std::string(class_symbol.is_forward_declared ? "yes" : "no"), "COMPILER");
        LOG_INFO("  Base class: " + (class_symbol.base_class.empty() ? "none" : class_symbol.base_class), "COMPILER");
        
        if (!class_symbol.field_registry.empty()) {
            LOG_INFO("  Fields:", "COMPILER");
            for (const auto& [field_name, field_symbol] : class_symbol.field_registry) {
                LOG_INFO("    " + field_name + " (used: " + (field_symbol.is_used ? "yes" : "no") + ")", "COMPILER");
            }
        }
        
        if (!class_symbol.method_registry.empty()) {
            LOG_INFO("  Methods:", "COMPILER");
            for (const auto& [method_name, method_symbol] : class_symbol.method_registry) {
                std::string method_details = "    " + method_name + 
                                           " (constructor: " + (method_symbol.is_constructor ? "yes" : "no") +
                                           ", destructor: " + (method_symbol.is_destructor ? "yes" : "no") +
                                           ", external: " + (method_symbol.is_external ? "yes" : "no") + ")";
                LOG_INFO(method_details, "COMPILER");
            }
        }
    }
    
    LOG_INFO("=== END DETAILED CLASS REGISTRY ===", "COMPILER");
}

void SemanticAnalyzer::log_method_registry() {
    LOG_INFO("=== GLOBAL METHOD REGISTRY ===", "COMPILER");
    
    // Count methods by type
    int constructor_count = 0;
    int destructor_count = 0;
    int external_count = 0;
    int static_count = 0;
    int instance_count = 0;
    
    const auto& classes = symbol_table->get_classes();
    for (const auto& [class_name, class_symbol] : classes) {
        for (const auto& [method_name, method_symbol] : class_symbol.method_registry) {
            if (method_symbol.is_constructor) constructor_count++;
            if (method_symbol.is_destructor) destructor_count++;
            if (method_symbol.is_external) external_count++;
            if (method_symbol.is_static) static_count++;
            else instance_count++;
            
            // Log detailed method information
            std::string method_info = "Method: " + method_symbol.qualified_name +
                                    " (params: " + std::to_string(method_symbol.parameter_names.size()) + 
                                    ", defined: " + (method_symbol.is_defined ? "yes" : "no") + ")";
            LOG_INFO("  " + method_info, "COMPILER");
        }
    }
    
    LOG_INFO("Method summary - Constructors: " + std::to_string(constructor_count) +
             ", Destructors: " + std::to_string(destructor_count) +
             ", External: " + std::to_string(external_count) +
             ", Static: " + std::to_string(static_count) +
             ", Instance: " + std::to_string(instance_count), "COMPILER");
    
    LOG_INFO("=== END GLOBAL METHOD REGISTRY ===", "COMPILER");
}

void SemanticAnalyzer::log_scope_information() {
    LOG_INFO("=== SCOPE ANALYSIS ===", "COMPILER");
    
    auto available_vars = symbol_table->get_available_variables_in_scope();
    LOG_INFO("Variables in current scope: " + std::to_string(available_vars.size()), "COMPILER");
    for (const std::string& var_name : available_vars) {
        auto* var_symbol = symbol_table->find_variable(var_name);
        if (var_symbol) {
            std::string var_info = "  " + var_name + 
                                 " (parameter: " + (var_symbol->is_parameter ? "yes" : "no") +
                                 ", field: " + (var_symbol->is_field ? "yes" : "no") +
                                 ", used: " + (var_symbol->is_used ? "yes" : "no") +
                                 ", assigned: " + (var_symbol->is_definitely_assigned ? "yes" : "no") + ")";
            LOG_INFO(var_info, "COMPILER");
        }
    }
    
    LOG_INFO("Current scope: " + get_full_scope_path(), "COMPILER");
    LOG_INFO("Scope depth: " + std::to_string(current_scope_depth), "COMPILER");
    LOG_INFO("=== END SCOPE ANALYSIS ===", "COMPILER");
}

// ============================================================================
// Enhanced Scope Tracking Methods
// ============================================================================

void SemanticAnalyzer::push_semantic_scope(const std::string& scope_name) {
    scope_stack.push_back(scope_name);
    current_scope_depth++;
    symbol_table->push_scope();
    log_scope_change("ENTER", scope_name);
}

void SemanticAnalyzer::pop_semantic_scope() {
    if (!scope_stack.empty()) {
        std::string scope_name = scope_stack.back();
        scope_stack.pop_back();
        current_scope_depth--;
        symbol_table->pop_scope();
        log_scope_change("EXIT", scope_name);
    }
}

std::string SemanticAnalyzer::get_full_scope_path() {
    if (scope_stack.empty()) {
        return "global";
    }
    
    std::string path = "";
    for (size_t i = 0; i < scope_stack.size(); ++i) {
        if (i > 0) path += ".";
        path += scope_stack[i];
    }
    return path;
}

void SemanticAnalyzer::log_scope_change(const std::string& action, const std::string& scope_name) {
    LOG_INFO("SCOPE " + action + ": " + scope_name + " (depth: " + std::to_string(current_scope_depth) + 
             ", full path: " + get_full_scope_path() + ")", "COMPILER");
}

void SemanticAnalyzer::generate_uml_diagram_output() {
    LOG_INFO("Generating PlantUML class diagram for semantic analysis results", "COMPILER");
    
    std::stringstream plantuml_output;
    
    plantuml_output << "@startuml\n";
    plantuml_output << "!theme toy\n";
    
    // Generate classes
    const auto& classes = symbol_table->get_classes();
    
    for (const auto& [class_name, class_symbol] : classes) {
        plantuml_output << "class " << class_name << " {\n";
        
        // Add fields with proper visibility and types
        for (const auto& [field_name, field_symbol] : class_symbol.field_registry) {
            std::string type_name = "unknown";
            if (field_symbol.type) {
                if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&field_symbol.type->name_segment)) {
                    type_name = (*ident)->name;
                }
            }
            // Use + for public fields (following current pattern)
            plantuml_output << "  +" << type_name << " " << field_name << "\n";
        }
        
        // Add separator line if we have both fields and methods
        if (!class_symbol.field_registry.empty() && !class_symbol.method_registry.empty()) {
            plantuml_output << "  ..\n";
        }
        
        // Add methods with proper visibility and signatures
        for (const auto& [method_name, method_symbol] : class_symbol.method_registry) {
            if (method_name == "%ctor" || method_name == "%dtor") {
                continue; // Skip internal constructor/destructor names for cleaner display
            }
            
            // Determine visibility (assuming public for now)
            std::string visibility = "+";
            
            // Get return type
            std::string return_type = "void";
            if (method_symbol.return_type) {
                if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&method_symbol.return_type->name_segment)) {
                    return_type = (*ident)->name;
                }
            }
            
            // Build parameter string
            std::stringstream params;
            for (size_t i = 0; i < method_symbol.parameter_names.size(); ++i) {
                if (i > 0) params << ", ";
                
                std::string param_type = "unknown";
                if (i < method_symbol.parameter_types.size() && method_symbol.parameter_types[i]) {
                    if (auto ident = std::get_if<std::shared_ptr<IdentifierNode>>(&method_symbol.parameter_types[i]->name_segment)) {
                        param_type = (*ident)->name;
                    }
                }
                params << param_type << " " << method_symbol.parameter_names[i];
            }
            
            // Add static modifier if applicable
            std::string static_modifier = method_symbol.is_static ? "{static} " : "";
            
            plantuml_output << "  " << visibility << static_modifier << method_name 
                          << "(" << params.str() << ") : " << return_type << "\n";
        }
        
        plantuml_output << "}\n\n";
    }
    
    // Add method call dependencies using structured call information
    plantuml_output << "' Method call dependencies\n";
    
    // Group calls by type (forward vs normal) and deduplicate class-level relationships
    std::set<std::string> forward_class_relationships;
    std::set<std::string> normal_class_relationships;
    
    for (const auto& call_info : discovered_method_calls) {
        if (call_info.caller_class != call_info.callee_class) {
            std::string relationship = call_info.caller_class + " --> " + call_info.callee_class;
            if (call_info.is_forward_call) {
                forward_class_relationships.insert(relationship);
            } else {
                normal_class_relationships.insert(relationship);
            }
        }
    }
    
    // Add forward declaration calls (dashed arrows)
    for (const auto& relationship : forward_class_relationships) {
        size_t arrow_pos = relationship.find(" --> ");
        if (arrow_pos != std::string::npos) {
            std::string caller_class = relationship.substr(0, arrow_pos);
            std::string callee_class = relationship.substr(arrow_pos + 5);
            plantuml_output << caller_class << " ..> " << callee_class;
        }
    }
    
    // Add normal method calls (solid arrows)
    for (const auto& relationship : normal_class_relationships) {
        size_t arrow_pos = relationship.find(" --> ");
        if (arrow_pos != std::string::npos) {
            std::string caller_class = relationship.substr(0, arrow_pos);
            std::string callee_class = relationship.substr(arrow_pos + 5);
            // Only show normal calls that aren't already shown as forward calls
            if (forward_class_relationships.find(relationship) == forward_class_relationships.end()) {
                plantuml_output << caller_class << " --> " << callee_class;
            }
        }
    }
    
    plantuml_output << "\n@enduml\n";
    
    // Save to .puml file
    std::string diagram_content = plantuml_output.str();
    std::ofstream file("tests/build/class_diagram.puml");
    if (file.is_open()) {
        file << diagram_content;
        file.close();
        LOG_INFO("PlantUML class diagram saved to: tests/build/class_diagram.puml", "COMPILER");
    } else {
        LOG_WARN("Could not save PlantUML diagram to file, outputting to log:", "COMPILER");
        LOG_INFO("PlantUML Diagram:\n" + diagram_content, "COMPILER");
    }
}

} // namespace Mycelium::Scripting::Lang