#pragma once

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Config.hpp>

#include <new>
#include <type_traits>
#include <utility>

namespace Aero::Base {

enum class ErrorCode : std::uint32_t {
    Ok = 0,
    OutOfMemory,
    InvalidArgument,
    OutOfRange,
    InvalidUtf8,
    NotInitialized,
    Unsupported,
    InternalError,
    AlreadyExists,
    InvalidState,
    NotFound,
    IdCollision,
    CycleDetected,
    WrongThread,
    ReadOnly,
    ValidationFailed
};

struct Status  {
    ErrorCode code = ErrorCode::Ok;
    const char* message = "";

    constexpr Status() noexcept = default;
    constexpr Status(ErrorCode value, const char* text) noexcept : code(value), message(text != nullptr ? text : "") {}

    constexpr bool IsOk() const noexcept { return code == ErrorCode::Ok; }

    static constexpr Status Ok() noexcept { return {}; }

    static constexpr Status Failure(ErrorCode code, const char* message) noexcept { return {code, message}; }
    static constexpr Status InvalidArgument(const char* message) noexcept { return {ErrorCode::InvalidArgument, message}; }
    static constexpr Status InvalidState(const char* message) noexcept { return {ErrorCode::InvalidState, message}; }
    static constexpr Status NotInitialized(const char* message) noexcept { return {ErrorCode::NotInitialized, message}; }
    static constexpr Status NotFound(const char* message) noexcept { return {ErrorCode::NotFound, message}; }
    static constexpr Status OutOfRange(const char* message) noexcept { return {ErrorCode::OutOfRange, message}; }
    static constexpr Status OutOfMemory(const char* message = "Out of memory") noexcept { return {ErrorCode::OutOfMemory, message}; }
};

namespace Detail {

template<typename, typename = void>
struct HasArrowOperator : std::false_type {};

template<typename U>
struct HasArrowOperator<U, std::void_t<decltype(std::declval<U&>().operator->())>> : std::true_type {};

template<typename U>
constexpr decltype(auto) ArrowHelper(U& val) noexcept {
    if constexpr (std::is_pointer_v<U>) {
        return val;
    } else if constexpr (HasArrowOperator<U>::value) {
        return val.operator->();
    } else {
        return &val;
    }
}

template<typename U>
constexpr decltype(auto) ArrowHelper(const U& val) noexcept {
    if constexpr (std::is_pointer_v<U>) {
        return val;
    } else if constexpr (HasArrowOperator<const U>::value) {
        return val.operator->();
    } else {
        return &val;
    }
}

} // namespace Detail

template<class T> class Result  {
public:
    Result(const T& value) : hasValue_(true) { new (&storage_.value) T(value); }

    Result(T&& value) noexcept(std::is_nothrow_move_constructible<T>::value) : hasValue_(true) {
        new (&storage_.value) T(std::move(value));
    }

    Result(Status status) noexcept : hasValue_(false) {
        if (status.IsOk()) {
            status = Status::Failure(ErrorCode::InternalError, "Result<T> cannot represent success without a value");
        }
        new (&storage_.status) Status(status);
    }

    Result(const Result& other) : hasValue_(other.hasValue_) {
        if (hasValue_) {
            new (&storage_.value) T(other.storage_.value);
        } else { new (&storage_.status) Status(other.storage_.status); }
    }

    Result(Result&& other) noexcept(std::is_nothrow_move_constructible<T>::value) : hasValue_(other.hasValue_) {
        if (hasValue_) {
            new (&storage_.value) T(std::move(other.storage_.value));
        } else { new (&storage_.status) Status(other.storage_.status); }
    }

    Result& operator=(const Result& other) {
        if (this != &other) {
            Destroy();
            hasValue_ = other.hasValue_;
            if (hasValue_) {
                new (&storage_.value) T(other.storage_.value);
            } else { new (&storage_.status) Status(other.storage_.status); }
        }
        return *this;
    }

    Result& operator=(Result&& other) noexcept(std::is_nothrow_move_constructible<T>::value &&
                 std::is_nothrow_move_assignable<T>::value) {
        if (this != &other) {
            Destroy();
            hasValue_ = other.hasValue_;
            if (hasValue_) {
                new (&storage_.value) T(std::move(other.storage_.value));
            } else { new (&storage_.status) Status(other.storage_.status); }
        }
        return *this;
    }

    ~Result() { Destroy(); }

    bool HasValue() const noexcept { return hasValue_; }

    explicit operator bool() const noexcept { return HasValue(); }

    T& Value() & noexcept {
        AERO_ASSERT(hasValue_);
        return storage_.value;
    }

    const T& Value() const& noexcept {
        AERO_ASSERT(hasValue_);
        return storage_.value;
    }

    T&& Value() && noexcept {
        AERO_ASSERT(hasValue_);
        return std::move(storage_.value);
    }

    decltype(auto) operator*() & noexcept {
        return Value();
    }

    decltype(auto) operator*() const& noexcept {
        return Value();
    }

    decltype(auto) operator*() && noexcept {
        return std::move(*this).Value();
    }

    decltype(auto) operator->() noexcept {
        AERO_ASSERT(hasValue_);
        return Detail::ArrowHelper(storage_.value);
    }

    decltype(auto) operator->() const noexcept {
        AERO_ASSERT(hasValue_);
        return Detail::ArrowHelper(storage_.value);
    }

    Status GetStatus() const noexcept { return hasValue_ ? Status::Ok() : storage_.status; }

private:
    union Storage {
        Storage() noexcept {}
        ~Storage() {}

        T value;
        Status status;
    } storage_;

    bool hasValue_ = false;

    void Destroy() noexcept {
        if (hasValue_) {
            storage_.value.~T();
        } else { storage_.status.~Status(); }
    }
};

template<> class Result<void>  {
public:
    Result() noexcept = default;

    Result(Status status) noexcept : status_(status) {}

    bool HasValue() const noexcept { return status_.IsOk(); }

    explicit operator bool() const noexcept { return HasValue(); }

    Status GetStatus() const noexcept { return status_; }

private:
    Status status_ = Status::Ok();
};

} // namespace Aero::Base

namespace Aero {

template<class T> using Result = Base::Result<T>;

} // namespace Aero
