#include "parser/parser.h"
#include "parser/expression_parser.h"
#include "parser/statement_parser.h"
#include "ast/ast_allocator.hpp"

namespace Mycelium::Scripting::Lang {

class DeclarationParser {
public:
    DeclarationParser(Parser* parser) : parser_(parser) {}
    
    ParseResult<DeclarationNode> parse_declaration() {
        // Placeholder for now
        return ParseResult<DeclarationNode>::error(
            parser_->create_error(ErrorKind::UnexpectedToken, "Declaration parsing not implemented yet"));
    }
    
private:
    Parser* parser_;
};

Parser::Parser(TokenStream& tokens) 
    : context_(tokens), 
      expr_parser_(std::make_unique<ExpressionParser>(this)),
      stmt_parser_(std::make_unique<StatementParser>(this)),
      decl_parser_(std::make_unique<DeclarationParser>(this)) {
}

Parser::~Parser() = default;

ParseResult<CompilationUnitNode> Parser::parse() {
    auto* unit = allocator_.alloc<CompilationUnitNode>();
    unit->contains_errors = false;  // Initialize, will update based on children
    std::vector<AstNode*> statements;  // Changed to AstNode* for error integration
    
    while (!context_.at_end()) {
        auto stmt_result = parse_top_level_construct();
        
        if (stmt_result.is_success()) {
            statements.push_back(stmt_result.get_node());
        } else if (stmt_result.is_error()) {
            // Add error node and continue
            statements.push_back(stmt_result.get_error());
            // Simple recovery: skip to next statement boundary
            recovery_.recover_to_safe_point(context_);
        } else {
            // Fatal error - attempt recovery
            size_t pos_before_recovery = context_.position;
            recovery_.recover_to_safe_point(context_);
            
            // Ensure we made progress
            if (context_.position <= pos_before_recovery) {
                context_.advance(); // Force progress
            }
            
            if (!context_.at_end()) {
                auto* error = create_error(ErrorKind::UnexpectedToken, "Invalid top-level construct");
                statements.push_back(error);
                continue;
            }
            break;
        }
    }
    
    // Allocate SizedArray for statements (now using AstNode* for error integration)
    if (!statements.empty()) {
        auto* stmt_array = allocator_.alloc_array<AstNode*>(statements.size());
        bool has_errors = false;
        for (size_t i = 0; i < statements.size(); ++i) {
            stmt_array[i] = statements[i];
            if (ast_has_errors(statements[i])) {
                has_errors = true;
            }
        }
        unit->statements.values = stmt_array;
        unit->statements.size = static_cast<int>(statements.size());
        unit->contains_errors = has_errors;  // Propagate error flag
    } else {
        unit->contains_errors = false;
    }
    
    return ParseResult<CompilationUnitNode>::success(unit);
}

ParseResult<StatementNode> Parser::parse_top_level_construct() {
    // For now, just parse as statements
    // Later we'll add declaration parsing
    return get_statement_parser().parse_statement();
}


} // namespace Mycelium::Scripting::Lang