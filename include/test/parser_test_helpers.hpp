#pragma once

#include "test/test_framework.hpp"
#include "ast/ast.hpp"
#include "ast/ast_printer.hpp"
#include "common/logger.hpp"
#include <sstream>
#include <string>

namespace Mycelium::Testing {

using namespace Mycelium::Scripting::Lang;
using namespace Mycelium::Scripting::Common;

// Helper function to generate AST debug information using logger string capture
inline std::string get_ast_debug_info(AstNode* node, const std::string& label = "AST") {
    if (!node) {
        return label + ": <null>";
    }
    
    std::stringstream ss;
    ss << "\n" << label << " Debug Info:\n";
    ss << "  Node Type: " << get_node_type_name(node) << "\n";
    ss << "  Type ID: " << static_cast<int>(node->typeId) << "\n";
    ss << "  AST Structure:\n";
    
    // Use logger string capture to get AST printer output
    Logger& logger = Logger::get_instance();
    logger.begin_string_capture();
    
    AstPrinterVisitor printer;
    node->accept(&printer);
    
    std::string ast_output = logger.end_string_capture();
    
    // Indent each line of the AST output
    std::istringstream iss(ast_output);
    std::string line;
    while (std::getline(iss, line)) {
        ss << "    " << line << "\n";
    }
    
    return ss.str();
}

// Enhanced assertion macros for parser tests that include AST debugging

#define ASSERT_AST_TRUE(condition, node, message) \
    do { \
        if (!(condition)) { \
            std::string debug_info = get_ast_debug_info(node, "Failed Node"); \
            return TestResult(false, std::string(message) + debug_info); \
        } \
    } while(0)

#define ASSERT_AST_FALSE(condition, node, message) \
    do { \
        if (condition) { \
            std::string debug_info = get_ast_debug_info(node, "Failed Node"); \
            return TestResult(false, std::string(message) + debug_info); \
        } \
    } while(0)

#define ASSERT_AST_EQ(expected, actual, node, message) \
    do { \
        if ((expected) != (actual)) { \
            std::string debug_info = get_ast_debug_info(node, "Failed Node"); \
            std::stringstream ss; \
            ss << message << " (Expected: " << static_cast<int>(expected) << ", Actual: " << static_cast<int>(actual) << ")" << debug_info; \
            return TestResult(false, ss.str()); \
        } \
    } while(0)

#define ASSERT_AST_STR_EQ(expected, actual, node, message) \
    do { \
        if (std::string(expected) != std::string(actual)) { \
            std::string debug_info = get_ast_debug_info(node, "Failed Node"); \
            std::stringstream ss; \
            ss << message << " (Expected: '" << (expected) << "', Actual: '" << (actual) << "')" << debug_info; \
            return TestResult(false, ss.str()); \
        } \
    } while(0)

#define ASSERT_AST_NOT_NULL(ptr, node, message) \
    do { \
        if ((ptr) == nullptr) { \
            std::string debug_info = get_ast_debug_info(node, "Failed Node"); \
            return TestResult(false, std::string(message) + debug_info); \
        } \
    } while(0)

#define ASSERT_AST_NULL(ptr, node, message) \
    do { \
        if ((ptr) != nullptr) { \
            std::string debug_info = get_ast_debug_info(node, "Failed Node"); \
            return TestResult(false, std::string(message) + debug_info); \
        } \
    } while(0)

// Specialized macros for common parser test patterns

#define ASSERT_NODE_TYPE(node, expected_type, context_node, message) \
    = (node_is<expected_type>(node) ? node_cast<expected_type>(node) : \
        ({ \
            std::string debug_info = get_ast_debug_info(context_node, "Context"); \
            std::string node_info = get_ast_debug_info(node, "Actual Node"); \
            std::stringstream ss; \
            ss << message << " (Expected: " << #expected_type << ", Actual: " << get_node_type_name(node) << ")"; \
            ss << debug_info << node_info; \
            return TestResult(false, ss.str()); \
            (expected_type*)nullptr; \
        }))

#define ASSERT_BINARY_OP(binary_node, expected_op, context_node, message) \
    do { \
        if (!node_is<BinaryExpressionNode>(binary_node)) { \
            std::string debug_info = get_ast_debug_info(context_node, "Context"); \
            return TestResult(false, std::string("Node is not a binary expression: ") + message + debug_info); \
        } \
        auto _bin = static_cast<BinaryExpressionNode*>(binary_node); \
        if (_bin->opKind != expected_op) { \
            std::string debug_info = get_ast_debug_info(context_node, "Context"); \
            std::stringstream ss; \
            ss << message << " (Expected op: " << to_string(expected_op) << ", Actual op: " << to_string(_bin->opKind) << ")"; \
            ss << debug_info; \
            return TestResult(false, ss.str()); \
        } \
    } while(0)

#define ASSERT_UNARY_OP(unary_node, expected_op, context_node, message) \
    do { \
        if (!node_is<UnaryExpressionNode>(unary_node)) { \
            std::string debug_info = get_ast_debug_info(context_node, "Context"); \
            return TestResult(false, std::string("Node is not a unary expression: ") + message + debug_info); \
        } \
        auto _un = static_cast<UnaryExpressionNode*>(unary_node); \
        if (_un->opKind != expected_op) { \
            std::string debug_info = get_ast_debug_info(context_node, "Context"); \
            std::stringstream ss; \
            ss << message << " (Expected op: " << to_string(expected_op) << ", Actual op: " << to_string(_un->opKind) << ")"; \
            ss << debug_info; \
            return TestResult(false, ss.str()); \
        } \
    } while(0)

#define ASSERT_ASSIGNMENT_OP(assign_node, expected_op, context_node, message) \
    do { \
        if (!node_is<AssignmentExpressionNode>(assign_node)) { \
            std::string debug_info = get_ast_debug_info(context_node, "Context"); \
            return TestResult(false, std::string("Node is not an assignment expression: ") + message + debug_info); \
        } \
        auto _assign = static_cast<AssignmentExpressionNode*>(assign_node); \
        if (_assign->opKind != expected_op) { \
            std::string debug_info = get_ast_debug_info(context_node, "Context"); \
            std::stringstream ss; \
            ss << message << " (Expected op: " << to_string(expected_op) << ", Actual op: " << to_string(_assign->opKind) << ")"; \
            ss << debug_info; \
            return TestResult(false, ss.str()); \
        } \
    } while(0)

#define ASSERT_IDENTIFIER_NAME(identifier_node, expected_name, context_node, message) \
    do { \
        if (!node_is<IdentifierNode>(identifier_node)) { \
            std::string debug_info = get_ast_debug_info(context_node, "Context"); \
            return TestResult(false, std::string("Node is not an identifier: ") + message + debug_info); \
        } \
        auto _id = static_cast<IdentifierNode*>(identifier_node); \
        if (std::string(_id->name) != std::string(expected_name)) { \
            std::string debug_info = get_ast_debug_info(context_node, "Context"); \
            std::stringstream ss; \
            ss << message << " (Expected: '" << expected_name << "', Actual: '" << _id->name << "')"; \
            ss << debug_info; \
            return TestResult(false, ss.str()); \
        } \
    } while(0)

// Token assertion macros for lexer tests

#define ASSERT_TOKEN_KIND(actual_kind, expected_kind, index, message) \
    do { \
        if ((actual_kind) != (expected_kind)) { \
            std::stringstream ss; \
            ss << message << " at token " << (index) << " (Expected: " << std::string(to_string(expected_kind)) \
               << ", Actual: " << std::string(to_string(actual_kind)) << ")"; \
            return TestResult(false, ss.str()); \
        } \
    } while(0)

#define ASSERT_TOKEN_TEXT(actual_text, expected_text, index, message) \
    do { \
        if (std::string(actual_text) != std::string(expected_text)) { \
            std::stringstream ss; \
            ss << message << " at token " << (index) << " (Expected: '" << (expected_text) \
               << "', Actual: '" << std::string(actual_text) << "')"; \
            return TestResult(false, ss.str()); \
        } \
    } while(0)

#define ASSERT_TOKEN_SEQUENCE(stream, expected_kinds, message) \
    do { \
        if ((stream).size() < (expected_kinds).size()) { \
            std::stringstream ss; \
            ss << message << " (Stream has " << (stream).size() << " tokens but expected " << (expected_kinds).size() << ")"; \
            return TestResult(false, ss.str()); \
        } \
        for (size_t i = 0; i < (expected_kinds).size(); ++i) { \
            ASSERT_TOKEN_KIND((stream)[i].kind, (expected_kinds)[i], i, message); \
        } \
    } while(0)

// Macro to print AST for debugging (doesn't fail the test)
#define DEBUG_PRINT_AST(node, label) \
    do { \
        if (node) { \
            std::cout << "\n=== " << label << " ===\n"; \
            std::cout << get_ast_debug_info(node, label) << "\n"; \
            std::cout << "=== End " << label << " ===\n\n"; \
        } else { \
            std::cout << "\n=== " << label << " ===\n<null>\n=== End " << label << " ===\n\n"; \
        } \
    } while(0)

} // namespace Mycelium::Testing