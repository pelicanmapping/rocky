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
        Failure& operator = (const Failure& rhs) = default;

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
    * Result has NO default constructor. If you want to hold onto a Failure state,
    * Use the Status object instead.
    */
    template<typename T = std::nullopt_t, typename E = Failure>
    class [[nodiscard]] Result
    {
    public:
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
        //! Release ownership of the good result, leaving this object in an undefined state.
        [[nodiscard]] T release() {
            return std::move(std::get<T>(_value));
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

    //! Constant to use as a "good" value for a Result<>.
    constexpr std::nullopt_t ResultVoidOK = std::nullopt;

    //! Constant to use as a "fail" value for a Result<T, std::nullopt_t>.
    constexpr std::nullopt_t ResultFail = std::nullopt;

    /**
    * Status object that holds a potential Failure state.
    * The default constructed Status represents a good state.
    */
    class Status
    {
    public:
        //! Construct a status indicating all is well
        Status() = default;

        //! Construct a status indicating failure
        Status(const Failure& f) : _error(f) {}

        //! Assign
        Status& operator = (const Failure& f) {
            _error = f;
            return *this;
        }

        //! Assign
        Status& operator = (const Status& rhs) {
            if (this != &rhs) {
                _error = rhs._error;
            }
            return *this;
        }

        inline const Failure& error() const {
            return _error.value();
        }
        inline bool ok() const {
            return !_error.has_value();
        }
        inline bool failed() const {
            return _error.has_value();
        }
        inline void clear() {
            _error.reset();
        }

    private:
        std::optional<Failure> _error;
    };
}
