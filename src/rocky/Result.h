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
    /**
    * General purpose failure object that can be used to report errors
    */
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
            if (type == ResourceUnavailable)
                return "Resource unavailable" + (message.empty() ? "" : "... " + message);
            if (type == ServiceUnavailable)
                return "Service unavailable" + (message.empty() ? "" : "... " + message);
            if (type == ConfigurationError)
                return "Configuration error" + (message.empty() ? "" : "... " + message);
            if (type == AssertionFailure)
                return "Assertion failure" + (message.empty() ? "" : "... " + message);
            if (type == OperationCanceled)
                return "Operation canceled" + (message.empty() ? "" : "... " + message);
            return "General error" + (message.empty() ? "" : "... " + message);
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

    /**
    * Result union that can hold either a success value object or a failure object.
    */
    template<typename T = std::nullopt_t, typename E = Failure>
    class [[nodiscard]] Result
    {
    public:
        //! Default constructor, default good result.
        Result();

        //! Copy construct
        Result(const Result& rhs) = default;

        //! New successful result
        Result(const T& good) {
            _value.template emplace<T>(good);
        }
        //! New successful result (moved)
        Result(T&& good) noexcept {
            _value.template emplace<T>(std::move(good));
        }
        //! New failure result
        Result(const E& bad) {
            _value.template emplace<E>(bad);
        }
        //! New failure result (moved)
        Result(E&& bad) noexcept {
            _value.template emplace<E>(std::move(bad));
        }
        //! Assignment operator (copy)
        Result& operator = (const Result& rhs) {
            if (this != &rhs) {
                if (std::holds_alternative<T>(rhs._value))
                    _value.template emplace<T>(std::get<T>(rhs._value));
                else if (std::holds_alternative<E>(rhs._value))
                    _value.template emplace<E>(std::get<E>(rhs._value));
            }
            return *this;
        }

        //! Assignment operator (failure)
        Result& operator = (const E& bad) {
            _value.template emplace<E>(bad);
            return *this;
        }
        //! Assignment operator (success)
        Result& operator = (const T& good) {
            _value.template emplace<T>(good);
            return *this;
        }

        //! Did the result succeed?
        [[nodiscard]] bool ok() const {
            return std::holds_alternative<T>(_value);
        }
        //! Did the result fail?
        [[nodiscard]] bool failed() const {
            return std::holds_alternative<E>(_value);
        }
        //! Did the result succeed?
        [[nodiscard]] operator bool() const {
            return ok();
        }
        //! Access a good result (be sure to check ok()/failed() first)
        [[nodiscard]] T* operator -> () {
            return &value();
        }
        //! Access a good result (be sure to check ok()/failed() first)
        [[nodiscard]] const T* operator -> () const {
            return &value();
        }
        //! Access a good result (be sure to check ok()/failed() first)
        [[nodiscard]] T& value() {
            return std::get<T>(_value);
        }
        //! Access a good result (be sure to check ok()/failed() first)
        [[nodiscard]] const T& value() const {
            return std::get<T>(_value);
        }
        //! Access the failure result (be sure to check ok()/failed() first)
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

    // specialize for std::nullopt_t + Failure
    template<>
    inline Result<std::nullopt_t, Failure>::Result() {
        _value.emplace<std::nullopt_t>(std::nullopt);
    }
}
