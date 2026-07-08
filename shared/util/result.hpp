#pragma once

#include <system_error>
#include <type_traits>
#include <utility>

namespace ud {

enum class ErrorCode {
    Success = 0,
    UnknownError,
    OutOfMemory,
    NetworkError,
    Timeout,
    InvalidParameter,
    NotImplemented,
    SystemError
};

class Error {
public:
    Error(ErrorCode code = ErrorCode::Success, const char* message = nullptr)
        : code_(code), message_(message) {}

    ErrorCode code() const { return code_; }
    const char* message() const { return message_; }
    explicit operator bool() const { return code_ != ErrorCode::Success; }

private:
    ErrorCode code_;
    const char* message_;
};

template <typename T, typename E = Error>
class Result {
public:
    Result(T value) : has_value_(true), value_(std::move(value)) {}
    Result(E error) : has_value_(false), error_(std::move(error)) {}

    bool has_value() const { return has_value_; }
    explicit operator bool() const { return has_value_; }

    const T& value() const { return value_; }
    T& value() { return value_; }

    const E& error() const { return error_; }
    E& error() { return error_; }

private:
    bool has_value_;
    union {
        T value_;
        E error_;
    };
};

template <typename E>
class Result<void, E> {
public:
    Result() : has_value_(true) {}
    Result(E error) : has_value_(false), error_(std::move(error)) {}

    bool has_value() const { return has_value_; }
    explicit operator bool() const { return has_value_; }

    const E& error() const { return error_; }
    E& error() { return error_; }

private:
    bool has_value_;
    union {
        bool dummy_;
        E error_;
    };
};

#define UD_TRY(expr) \
    do { \
        auto&& _res = (expr); \
        if (!_res) { \
            return _res.error(); \
        } \
    } while (0)

#define UD_ASSIGN_OR_RETURN(var, expr) \
    auto&& _res_##var = (expr); \
    if (!_res_##var) { \
        return _res_##var.error(); \
    } \
    var = std::move(_res_##var.value())

} // namespace ud
