/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Common.h>
#include <string>
#include <variant>
#include <optional>

namespace ROCKY_NAMESPACE
{
    const std::string FailureText[] = {
        "Resource unavailable",
        "Service unavailable",
        "Configuration error",
        "Assertion failure",
        "Operation canceled",
        "General error"
    };

    struct Failure
    {
        enum Type {
            ResourceUnavailable,  // e.g. failure to access a file, URL, database, or other resource
            ServiceUnavailable,   // e.g. failure to connect to a site, load a plugin, extension, or other module
            ConfigurationError,   // required data or properties missing
            AssertionFailure,     // an illegal software state was detected
            OperationCanceled,    // operation was canceled intentionally
            GeneralError          // something else went wrong
        };

        Type type = GeneralError;
        std::string message = {};

        Failure() = default;
        Failure(const Failure& rhs) = default;
        Failure(Failure&& rhs) noexcept = default;

        explicit Failure(const Type t) : type(t) {}
        explicit Failure(std::string_view m) : message(m) {}
        explicit Failure(const Type t, std::string_view m) : type(t), message(m) {}

        std::string string() const {
            return message.empty() ? FailureText[(int)type < 6 ? (int)type : 5] :
                FailureText[(int)type < 6 ? (int)type : 5] + ": " + message;
        }

        Failure& operator()(std::string_view m) {
            message = m;
            return *this;
        }
    };

    //! Global convenience objects
    const Failure Failure_ServiceUnavailable(Failure::ServiceUnavailable);
    const Failure Failure_ResourceUnavailable(Failure::ResourceUnavailable);
    const Failure Failure_ConfigurationError(Failure::ConfigurationError);
    const Failure Failure_AssertionFailure(Failure::AssertionFailure);
    const Failure Failure_OperationCanceled(Failure::OperationCanceled);
    const Failure Failure_GeneralError(Failure::GeneralError);

    template<typename T = std::nullopt_t, typename E = Failure>
    class [[nodiscard]] Result
    {
    public:
        //! Default constructor, default good result.
        Result();

        Result(const Result& rhs) = default;

        Result(const T& good) {
            _value.template emplace<T>(good);
        }
        Result(T&& good) noexcept {
            _value.template emplace<T>(std::move(good));
        }
        Result(const E& bad) {
            _value.template emplace<E>(bad);
        }
        Result(E&& bad) noexcept {
            _value.template emplace<E>(std::move(bad));
        }

        Result& operator = (const Result& rhs) {
            if (this != &rhs) {
                if (std::holds_alternative<T>(rhs._value))
                    _value.template emplace<T>(std::get<T>(rhs._value));
                else if (std::holds_alternative<E>(rhs._value))
                    _value.template emplace<E>(std::get<E>(rhs._value));
            }
            return *this;
        }

        Result& operator = (const E& bad) {
            _value.template emplace<E>(bad);
            return *this;
        }
        Result& operator = (const T& good) {
            _value.template emplace<T>(good);
            return *this;
        }

        [[nodiscard]] bool ok() const {
            return std::holds_alternative<T>(_value);
        }
        [[nodiscard]] bool failed() const {
            return std::holds_alternative<E>(_value);
        }
        [[nodiscard]] operator bool() const {
            return ok();
        }
        [[nodiscard]] T* operator -> () {
            return &value();
        }
        [[nodiscard]] T& value() {
            return std::get<T>(_value);
        }
        [[nodiscard]] const E& error() const {
            return std::get<E>(_value);
        }

    private:
        std::variant<std::monostate, T, E> _value;
    };

    template<class T, class E>
    Result<T,E>::Result() {
        _value.template emplace<T>(T{});
    }

    template<>
    inline Result<std::nullopt_t, Failure>::Result() {
        _value.emplace<std::nullopt_t>(std::nullopt);
    }
}
