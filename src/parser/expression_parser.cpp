#include "parser/expression_parser.hpp"
#include "parser/parser.hpp"

namespace Myre
{

    ExpressionParser::ExpressionParser(Parser *parser) : ParserBase(parser)
    {
    }

    // Main expression parsing entry point
    ParseResult<ExpressionNode> ExpressionParser::parse_expression(int min_precedence)
    {
        // Handle assignment expressions first (right-associative, lowest precedence)
        auto left_result = parse_binary_expression(min_precedence);
        if (!left_result.is_success())
        {
            return left_result;
        }

        auto *left = left_result.get_node();

        // Check for assignment operators
        if (context().check(TokenKind::Assign))
        {
            const Token &assign_token = context().current();
            context().advance(); // consume '='

            // Parse right-hand side (right-associative)
            auto right_result = parse_expression(0); // Start from lowest precedence for right-associativity

            if (right_result.is_fatal())
            {
                auto *error = create_error("Expected expression after '='");
                return ParseResult<ExpressionNode>::error(error);
            }

            auto *assign_expr = parser_->get_allocator().alloc<AssignmentExpressionNode>();
            assign_expr->target = left;
            assign_expr->source = right_result.get_node();
            assign_expr->opKind = assign_token.to_assignment_operator_kind();
            assign_expr->contains_errors = ast_has_errors(left) || ast_has_errors(right_result.get_node());

            return ParseResult<ExpressionNode>::success(assign_expr);
        }

        return left_result;
    }

    // Pratt parser implementation for binary expressions
    ParseResult<ExpressionNode> ExpressionParser::parse_binary_expression(int min_precedence)
    {
        auto left_result = parse_primary();
        if (!left_result.is_success())
        {
            return left_result;
        }

        auto *left = left_result.get_node();

        while (!context().at_end())
        {
            const Token &op_token = context().current();
            TokenKind op_kind = op_token.kind;
            int precedence = get_precedence(op_kind);

            if (precedence == 0 || precedence < min_precedence)
                break;

            context().advance(); // consume operator

            auto right_result = parse_binary_expression(precedence + 1);
            if (!right_result.is_success())
            {
                // Error recovery: create binary expr with error on right side
                auto *error = create_error("Expected right operand");
                auto *binary = parser_->get_allocator().alloc<BinaryExpressionNode>();
                binary->left = left;
                binary->right = error; // Use AstNode* directly for error integration
                binary->opKind = op_token.to_binary_operator_kind();
                binary->contains_errors = true;
                return ParseResult<ExpressionNode>::success(binary);
            }

            auto *binary = parser_->get_allocator().alloc<BinaryExpressionNode>();
            binary->left = left;
            binary->right = right_result.get_node();
            binary->opKind = op_token.to_binary_operator_kind();
            binary->contains_errors = ast_has_errors(left) || ast_has_errors(right_result.get_node());

            left = binary;
        }

        return ParseResult<ExpressionNode>::success(left);
    }

    // Primary expression parsing - handles literals, identifiers, parentheses, unary expressions
    ParseResult<ExpressionNode> ExpressionParser::parse_primary()
    {
        auto &ctx = context();

        // Handle unary expressions first
        if (ctx.check(TokenKind::Not) || ctx.check(TokenKind::Minus) ||
            ctx.check(TokenKind::Increment) || ctx.check(TokenKind::Decrement))
        {
            return parse_unary_expression();
        }

        if (ctx.check(TokenKind::IntegerLiteral))
        {
            return parse_integer_literal();
        }

        if (ctx.check(TokenKind::FloatLiteral))
        {
            return parse_float_literal();
        }

        if (ctx.check(TokenKind::DoubleLiteral))
        {
            return parse_double_literal();
        }

        if (ctx.check(TokenKind::StringLiteral))
        {
            return parse_string_literal();
        }

        if (ctx.check(TokenKind::BooleanLiteral))
        {
            return parse_boolean_literal();
        }

        if (ctx.check(TokenKind::Identifier))
        {
            return parse_identifier_or_call();
        }

        if (ctx.check(TokenKind::LeftParen))
        {
            return parse_parenthesized_expression();
        }

        if (ctx.check(TokenKind::New))
        {
            return parse_new_expression();
        }

        return ParseResult<ExpressionNode>::error(
            create_error("Expected expression"));
    }

    // Literal parsing methods
    ParseResult<ExpressionNode> ExpressionParser::parse_integer_literal()
    {
        const Token &token = context().current();
        context().advance();

        auto *literal = parser_->get_allocator().alloc<LiteralExpressionNode>();
        literal->kind = LiteralKind::Integer;
        literal->contains_errors = false;

        auto *token_node = parser_->get_allocator().alloc<TokenNode>();
        token_node->text = token.text;
        token_node->contains_errors = false;
        literal->token = token_node;

        return ParseResult<ExpressionNode>::success(literal);
    }

    ParseResult<ExpressionNode> ExpressionParser::parse_string_literal()
    {
        const Token &token = context().current();
        context().advance();

        auto *literal = parser_->get_allocator().alloc<LiteralExpressionNode>();
        literal->kind = LiteralKind::String;
        literal->contains_errors = false;

        auto *token_node = parser_->get_allocator().alloc<TokenNode>();
        token_node->text = token.text;
        token_node->contains_errors = false;
        literal->token = token_node;

        return ParseResult<ExpressionNode>::success(literal);
    }

    ParseResult<ExpressionNode> ExpressionParser::parse_boolean_literal()
    {
        const Token &token = context().current();
        context().advance();

        auto *literal = parser_->get_allocator().alloc<LiteralExpressionNode>();
        literal->kind = LiteralKind::Boolean;
        literal->contains_errors = false;

        auto *token_node = parser_->get_allocator().alloc<TokenNode>();
        token_node->text = token.text;
        token_node->contains_errors = false;
        literal->token = token_node;

        return ParseResult<ExpressionNode>::success(literal);
    }

    ParseResult<ExpressionNode> ExpressionParser::parse_float_literal()
    {
        const Token &token = context().current();
        context().advance();

        auto *literal = parser_->get_allocator().alloc<LiteralExpressionNode>();
        literal->kind = LiteralKind::Float;
        literal->contains_errors = false;

        auto *token_node = parser_->get_allocator().alloc<TokenNode>();
        token_node->text = token.text;
        token_node->contains_errors = false;
        literal->token = token_node;

        return ParseResult<ExpressionNode>::success(literal);
    }

    ParseResult<ExpressionNode> ExpressionParser::parse_double_literal()
    {
        const Token &token = context().current();
        context().advance();

        auto *literal = parser_->get_allocator().alloc<LiteralExpressionNode>();
        literal->kind = LiteralKind::Double;
        literal->contains_errors = false;

        auto *token_node = parser_->get_allocator().alloc<TokenNode>();
        token_node->text = token.text;
        token_node->contains_errors = false;
        literal->token = token_node;

        return ParseResult<ExpressionNode>::success(literal);
    }

    ParseResult<ExpressionNode> ExpressionParser::parse_unary_expression()
    {
        const Token &op_token = context().current();
        context().advance(); // consume unary operator

        auto operand_result = parse_primary();
        if (operand_result.is_fatal())
        {
            auto *error = create_error("Expected operand after unary operator");
            return ParseResult<ExpressionNode>::error(error);
        }

        auto *unary_expr = parser_->get_allocator().alloc<UnaryExpressionNode>();
        unary_expr->operand = operand_result.get_node();
        unary_expr->opKind = op_token.to_unary_operator_kind();
        unary_expr->isPostfix = false;
        unary_expr->contains_errors = ast_has_errors(operand_result.get_node());

        return ParseResult<ExpressionNode>::success(unary_expr);
    }

    ParseResult<ExpressionNode> ExpressionParser::parse_identifier_or_call()
    {
        const Token &token = context().current();
        context().advance();

        auto *identifier_expr = parser_->get_allocator().alloc<IdentifierExpressionNode>();
        identifier_expr->contains_errors = false;
        auto *identifier = parser_->get_allocator().alloc<IdentifierNode>();
        identifier->name = token.text;
        identifier->contains_errors = false;
        identifier_expr->identifier = identifier;

        // Start with the identifier, then handle postfix operations
        ExpressionNode *current = identifier_expr;

        // Handle postfix operations: calls, member access, array indexing, increment/decrement
        while (true)
        {
            if (context().check(TokenKind::LeftParen))
            {
                // Function call: expr()
                auto call_result = parse_call_suffix(current);
                if (!call_result.is_success())
                    break;
                current = call_result.get_node();
            }
            else if (context().check(TokenKind::Dot))
            {
                // Member access: expr.member
                auto member_result = parse_member_access_suffix(current);
                if (!member_result.is_success())
                    break;
                current = member_result.get_node();
            }
            else if (context().check(TokenKind::LeftBracket))
            {
                // Array indexing: expr[index]
                auto index_result = parse_indexer_suffix(current);
                if (!index_result.is_success())
                    break;
                current = index_result.get_node();
            }
            else if (context().check(TokenKind::Increment) || context().check(TokenKind::Decrement))
            {
                // Postfix increment/decrement: expr++ or expr--
                const Token &op_token = context().current();
                context().advance(); // consume ++ or --

                auto *unary_expr = parser_->get_allocator().alloc<UnaryExpressionNode>();
                unary_expr->operand = current;
                unary_expr->opKind = op_token.to_unary_operator_kind();
                unary_expr->isPostfix = true;
                unary_expr->contains_errors = ast_has_errors(current);

                current = unary_expr;
            }
            else
            {
                break; // No more postfix operations
            }
        }

        return ParseResult<ExpressionNode>::success(current);
    }

    ParseResult<ExpressionNode> ExpressionParser::parse_parenthesized_expression()
    {
        context().advance(); // consume '('
        auto expr_result = parse_expression();
        parser_->expect(TokenKind::RightParen, "Expected ')' after expression");
        return expr_result;
    }

    // Operator precedence table for Myre language (following design document)
    int ExpressionParser::get_precedence(TokenKind op)
    {
        switch (op)
        {
        case TokenKind::Or:
            return 1; // ||
        case TokenKind::And:
            return 2; // &&
        case TokenKind::Equal:
        case TokenKind::NotEqual:
            return 3; // ==, !=
        case TokenKind::Less:
        case TokenKind::LessEqual:
        case TokenKind::Greater:
        case TokenKind::GreaterEqual:
            return 4; // <, <=, >, >=
        case TokenKind::DotDot:
        case TokenKind::DotDotEquals:
            return 5; // .. (range)
        case TokenKind::Plus:
        case TokenKind::Minus:
            return 6; // +, -
        case TokenKind::Asterisk:
        case TokenKind::Slash:
        case TokenKind::Percent:
            return 7; // *, /, %
        case TokenKind::Dot:
            return 9; // . (member access)
        case TokenKind::LeftBracket:
            return 10; // [] (indexing)
        case TokenKind::LeftParen:
            return 10; // () (function call)
        default:
            return 0; // Not a binary operator
        }
    }

    // Postfix expression parsing helpers
    ParseResult<ExpressionNode> ExpressionParser::parse_call_suffix(ExpressionNode *target)
    {
        context().advance(); // consume '('

        auto *call_expr = parser_->get_allocator().alloc<CallExpressionNode>();
        call_expr->target = target;
        call_expr->contains_errors = ast_has_errors(target);

        std::vector<AstNode *> arguments;

        // Parse argument list
        while (!context().check(TokenKind::RightParen) && !context().at_end())
        {
            auto arg_result = parse_expression();

            if (arg_result.is_fatal())
            {
                // Error recovery: add error node and try to continue
                auto *error = create_error("Invalid argument in call");
                arguments.push_back(error);
                call_expr->contains_errors = true;

                // Try to recover to next argument or end of call
                parser_->recover_to_safe_point(context());
                break;
            }

            arguments.push_back(arg_result.get_ast_node());
            if (ast_has_errors(arg_result.get_ast_node()))
            {
                call_expr->contains_errors = true;
            }

            // Check for comma separator
            if (context().check(TokenKind::Comma))
            {
                context().advance();
            }
            else if (!context().check(TokenKind::RightParen))
            {
                create_error("Expected ',' or ')' in argument list");
                call_expr->contains_errors = true;
                break;
            }
        }

        parser_->expect(TokenKind::RightParen, "Expected ')' to close function call");

        // Allocate argument array
        if (!arguments.empty())
        {
            auto *arg_array = parser_->get_allocator().alloc_array<AstNode *>(arguments.size());
            for (size_t i = 0; i < arguments.size(); ++i)
            {
                arg_array[i] = arguments[i];
            }
            call_expr->arguments.values = arg_array;
            call_expr->arguments.size = static_cast<int>(arguments.size());
        }

        return ParseResult<ExpressionNode>::success(call_expr);
    }

    ParseResult<ExpressionNode> ExpressionParser::parse_member_access_suffix(ExpressionNode *target)
    {
        context().advance(); // consume '.'

        if (!context().check(TokenKind::Identifier))
        {
            auto *error = create_error("Expected identifier after '.'");
            auto *member_expr = parser_->get_allocator().alloc<MemberAccessExpressionNode>();
            member_expr->target = target;
            member_expr->member = nullptr;
            member_expr->contains_errors = true;
            return ParseResult<ExpressionNode>::success(member_expr);
        }

        const Token &member_token = context().current();
        context().advance();

        auto *member_expr = parser_->get_allocator().alloc<MemberAccessExpressionNode>();
        member_expr->target = target;

        auto *member_identifier = parser_->get_allocator().alloc<IdentifierNode>();
        member_identifier->name = member_token.text;
        member_identifier->contains_errors = false;
        member_expr->member = member_identifier;

        member_expr->contains_errors = ast_has_errors(target);

        return ParseResult<ExpressionNode>::success(member_expr);
    }

    ParseResult<ExpressionNode> ExpressionParser::parse_indexer_suffix(ExpressionNode *target)
    {
        context().advance(); // consume '['

        auto index_result = parse_expression();
        if (index_result.is_fatal())
        {
            auto *error = create_error("Expected index expression");
            return ParseResult<ExpressionNode>::error(error);
        }

        parser_->expect(TokenKind::RightBracket, "Expected ']' after index expression");

        auto *indexer_expr = parser_->get_allocator().alloc<IndexerExpressionNode>();
        indexer_expr->target = target;
        indexer_expr->index = index_result.get_node();
        indexer_expr->contains_errors = ast_has_errors(target) || ast_has_errors(index_result.get_node());

        return ParseResult<ExpressionNode>::success(indexer_expr);
    }

    // Parse new expressions: new TypeName() or new TypeName
    ParseResult<ExpressionNode> ExpressionParser::parse_new_expression()
    {
        const Token &new_token = context().current();
        context().advance(); // consume 'new'

        // Parse the type name
        auto type_result = parser_->parse_type_expression();
        if (!type_result.is_success())
        {
            auto *error = create_error("Expected type name after 'new'");
            return ParseResult<ExpressionNode>::error(error);
        }

        auto *new_expr = parser_->get_allocator().alloc<NewExpressionNode>();
        new_expr->contains_errors = ast_has_errors(type_result.get_node());

        // Create token node for the 'new' keyword
        auto *new_keyword = parser_->get_allocator().alloc<TokenNode>();
        new_keyword->text = new_token.text;
        new_keyword->tokenKind = new_token.kind;
        new_keyword->contains_errors = false;
        new_expr->newKeyword = new_keyword;

        new_expr->type = type_result.get_node();

        // Check for optional constructor call: ()
        if (context().check(TokenKind::LeftParen))
        {
            // Parse constructor call as a call expression with the type as target
            // For now, we'll create a simple call expression
            auto call_result = parse_call_suffix(nullptr); // We'll fix the target afterwards
            if (call_result.is_success())
            {
                auto *call_expr = static_cast<CallExpressionNode *>(call_result.get_node());
                call_expr->target = nullptr; // Constructor calls don't have a target expression
                new_expr->constructorCall = call_expr;
                if (ast_has_errors(call_expr))
                {
                    new_expr->contains_errors = true;
                }
            }
            else
            {
                // Failed to parse constructor call - treat as error
                new_expr->contains_errors = true;
                new_expr->constructorCall = nullptr;
            }
        }
        else
        {
            // No constructor call
            new_expr->constructorCall = nullptr;
        }

        return ParseResult<ExpressionNode>::success(new_expr);
    }

    ParseResult<ExpressionNode> ExpressionParser::parse_match_expression()
    {
        return ParseResult<ExpressionNode>::error(
            create_error("Match expressions not implemented yet"));
    }

    ParseResult<ExpressionNode> ExpressionParser::parse_enum_variant()
    {
        return ParseResult<ExpressionNode>::error(
            create_error("Enum variants not implemented yet"));
    }

} // namespace Myre