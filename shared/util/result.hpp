#pragma once

#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
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
    Result(T value) : data_(std::move(value)) {}
    Result(E error) : data_(std::move(error)) {}

    bool has_value() const { return std::holds_alternative<T>(data_); }
    explicit operator bool() const { return has_value(); }

    const T& value() const { return std::get<T>(data_); }
    T& value() { return std::get<T>(data_); }

    const E& error() const { return std::get<E>(data_); }
    E& error() { return std::get<E>(data_); }

private:
    std::variant<T, E> data_;
};

template <typename E>
class Result<void, E> {
public:
    Result() : data_(std::monostate{}) {}
    Result(E error) : data_(std::move(error)) {}

    bool has_value() const { return std::holds_alternative<std::monostate>(data_); }
    explicit operator bool() const { return has_value(); }

    const E& error() const { return std::get<E>(data_); }
    E& error() { return std::get<E>(data_); }

private:
    std::variant<std::monostate, E> data_;
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
