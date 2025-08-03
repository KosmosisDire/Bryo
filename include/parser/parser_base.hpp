#pragma once

#include "ast/ast.hpp"
#include "parse_result.hpp"
#include "token_stream.hpp"

namespace Myre
{

    // Forward declarations
    class Parser;
    class ParseContext;

    /**
     * ParserBase - Base class for all specialized parsers
     *
     * Provides common functionality for all parser subclasses:
     * - Access to main parser instance
     * - Context management helpers
     * - Error creation utilities
     */
    class ParserBase
    {
    protected:
        Parser *parser_; // Reference to main parser for shared services

        // Helper accessors for parser state
        ParseContext &context();
        ErrorNode *create_error(const char *msg);

    public:
        explicit ParserBase(Parser *parser) : parser_(parser) {}
        virtual ~ParserBase() = default;

        // Prevent copying and moving
        ParserBase(const ParserBase &) = delete;
        ParserBase &operator=(const ParserBase &) = delete;
        ParserBase(ParserBase &&) = delete;
        ParserBase &operator=(ParserBase &&) = delete;
    };

} // namespace Myre