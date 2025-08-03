#pragma once

#include <string>
#include <vector>

namespace Myre
{
    namespace Scripting
    {
        namespace Lang
        {

            // Simple error reporting without complex Result<T> pattern
            struct SemanticError
            {
                enum Kind
                {
                    SymbolAlreadyDefined,
                    SymbolNotFound,
                    TypeMismatch,
                    InvalidOperation,
                    ReturnTypeMismatch,
                    BreakNotInLoop,
                    ContinueNotInLoop,
                    InvalidAssignment,
                    FunctionNotFound,
                    WrongArgumentCount,
                    FieldNotFound,
                    NotCallable
                };

                Kind kind;
                std::string message;
                int line;
                int column;

                SemanticError(Kind k, const std::string &msg, int l = 0, int c = 0)
                    : kind(k), message(msg), line(l), column(c) {}

                std::string to_string() const
                {
                    std::string result = "[Error] " + message;
                    if (line > 0)
                    {
                        result += " at line " + std::to_string(line) + ":" + std::to_string(column);
                    }
                    return result;
                }
            };

            // Simple error collector
            class ErrorCollector
            {
            private:
                std::vector<SemanticError> errors_;
                bool has_fatal_error_;

            public:
                ErrorCollector() : has_fatal_error_(false) {}

                void add_error(const SemanticError &error)
                {
                    errors_.push_back(error);
                }

                void add_error(SemanticError::Kind kind, const std::string &message, int line = 0, int col = 0)
                {
                    errors_.emplace_back(kind, message, line, col);
                }

                bool has_errors() const { return !errors_.empty(); }
                bool has_fatal_error() const { return has_fatal_error_; }
                void set_fatal() { has_fatal_error_ = true; }

                const std::vector<SemanticError> &errors() const { return errors_; }

                void clear()
                {
                    errors_.clear();
                    has_fatal_error_ = false;
                }

                void print_errors() const
                {
                    for (const auto &error : errors_)
                    {
                        std::cerr << error.to_string() << std::endl;
                    }
                }
            };

        }
    }
} // namespace Myre