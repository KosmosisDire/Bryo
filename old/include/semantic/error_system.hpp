#pragma once

#include "semantic/symbol_registry.hpp"
#include <string>
#include <vector>
#include <variant>
#include <stdexcept>

namespace Myre
{

    // === UNIFIED ERROR SYSTEM ===

    class CompilerError
    {
    public:
        enum class Phase
        {
            Parse,
            TypeCheck,
            CodeGen,
            Link
        };
        enum class Severity
        {
            Warning,
            Error,
            Fatal
        };

    private:
        Phase phase_;
        Severity severity_;
        std::string message_;
        SourceLocation location_;
        std::vector<std::string> context_stack_;

    public:
        CompilerError(Phase phase, Severity severity,
                      std::string message, SourceLocation location = {})
            : phase_(phase), severity_(severity), message_(std::move(message)), location_(location) {}

        CompilerError with_context(const std::string &context) const
        {
            CompilerError error = *this;
            error.context_stack_.push_back(context);
            return error;
        }

        // Getters
        Phase phase() const { return phase_; }
        Severity severity() const { return severity_; }
        const std::string &message() const { return message_; }
        const SourceLocation &location() const { return location_; }
        const std::vector<std::string> &context_stack() const { return context_stack_; }

        std::string phase_string() const
        {
            switch (phase_)
            {
            case Phase::Parse:
                return "Parse";
            case Phase::TypeCheck:
                return "TypeCheck";
            case Phase::CodeGen:
                return "CodeGen";
            case Phase::Link:
                return "Link";
            }
            return "Unknown";
        }

        std::string severity_string() const
        {
            switch (severity_)
            {
            case Severity::Warning:
                return "Warning";
            case Severity::Error:
                return "Error";
            case Severity::Fatal:
                return "Fatal";
            }
            return "Unknown";
        }

        std::string to_string() const
        {
            std::string result = "[" + phase_string() + " " + severity_string() + "] ";
            result += message_;

            if (location_.line > 0)
            {
                result += " at " + location_.to_string();
            }

            if (!context_stack_.empty())
            {
                result += "\nContext:";
                for (const auto &context : context_stack_)
                {
                    result += "\n  " + context;
                }
            }

            return result;
        }
    };

    // === RESULT TYPE ===

    template <typename T>
    class Result
    {
    private:
        std::variant<T, CompilerError> value_;

    public:
        Result(T value) : value_(std::move(value)) {}
        Result(CompilerError error) : value_(std::move(error)) {}

        bool is_success() const { return std::holds_alternative<T>(value_); }
        bool is_error() const { return std::holds_alternative<CompilerError>(value_); }

        const T &value() const
        {
            if (!is_success())
            {
                throw std::runtime_error("Attempted to get value from error result");
            }
            return std::get<T>(value_);
        }

        const CompilerError &error() const
        {
            if (!is_error())
            {
                throw std::runtime_error("Attempted to get error from success result");
            }
            return std::get<CompilerError>(value_);
        }

        // Monadic operations
        template <typename F>
        auto and_then(F &&func) const -> Result<decltype(func(value()))>
        {
            if (is_success())
            {
                return func(value());
            }
            else
            {
                return error();
            }
        }

        Result<T> or_else(const T &default_value) const
        {
            return is_success() ? *this : Result<T>(default_value);
        }

        template <typename F>
        Result<T> map_error(F &&func) const
        {
            if (is_error())
            {
                return func(error());
            }
            else
            {
                return *this;
            }
        }
    };

    // === CONVENIENCE FUNCTIONS ===

    template <typename T>
    Result<T> success(T value)
    {
        return Result<T>(std::move(value));
    }

    inline CompilerError type_error(const std::string &message, SourceLocation location = {})
    {
        return CompilerError(CompilerError::Phase::TypeCheck,
                             CompilerError::Severity::Error,
                             message, location);
    }

    inline CompilerError codegen_error(const std::string &message, SourceLocation location = {})
    {
        return CompilerError(CompilerError::Phase::CodeGen,
                             CompilerError::Severity::Error,
                             message, location);
    }

    inline CompilerError fatal_error(const std::string &message, SourceLocation location = {})
    {
        return CompilerError(CompilerError::Phase::TypeCheck,
                             CompilerError::Severity::Fatal,
                             message, location);
    }

} // namespace Myre