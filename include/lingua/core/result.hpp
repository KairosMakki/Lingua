#pragma once

#include <lingua/core/error.hpp>

#include <cassert>
#include <memory>
#include <utility>
#include <variant>

namespace lingua {

template <typename T>
class Result {
public:
    using value_type = T;
    using error_type = Error;

    Result() : storage_(Error{ErrorCode::InternalError, "uninitialized"}) {}
    Result(const T& v) : storage_(v) {}
    Result(T&& v) : storage_(std::move(v)) {}
    Result(const Error& e) : storage_(e) {}
    Result(Error&& e) : storage_(std::move(e)) {}

    [[nodiscard]] bool has_value() const noexcept { return std::holds_alternative<T>(storage_); }
    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

    T& value() & {
        assert(has_value());
        return std::get<T>(storage_);
    }
    const T& value() const & {
        assert(has_value());
        return std::get<T>(storage_);
    }
    T&& value() && {
        assert(has_value());
        return std::get<T>(std::move(storage_));
    }

    Error& error() & {
        assert(!has_value());
        return std::get<Error>(storage_);
    }
    const Error& error() const & {
        assert(!has_value());
        return std::get<Error>(storage_);
    }

    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }
    T& operator*() & { return value(); }
    const T& operator*() const & { return value(); }
    T&& operator*() && { return std::move(value()); }

    template <typename U>
    T value_or(U&& default_value) const & {
        if (has_value()) return value();
        return static_cast<T>(std::forward<U>(default_value));
    }

private:
    std::variant<T, Error> storage_;
};

template <>
class Result<void> {
public:
    Result() : ok_(true) {}
    Result(const Error& e) : ok_(false), err_(e) {}
    Result(Error&& e) : ok_(false), err_(std::move(e)) {}

    [[nodiscard]] bool has_value() const noexcept { return ok_; }
    [[nodiscard]] explicit operator bool() const noexcept { return ok_; }

    Error& error() & { return err_; }
    const Error& error() const & { return err_; }

private:
    bool ok_;
    Error err_{};
};

} // namespace lingua