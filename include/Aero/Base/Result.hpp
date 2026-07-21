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

struct Status final {
    ErrorCode code = ErrorCode::Ok;
    const char* message = "";

    constexpr Status() noexcept = default;
    constexpr Status(ErrorCode value, const char* text) noexcept
        : code(value), message(text != nullptr ? text : "") {}

    AERO_NODISCARD constexpr bool IsOk() const noexcept {
        return code == ErrorCode::Ok;
    }

    AERO_NODISCARD static constexpr Status Ok() noexcept {
        return {};
    }

    AERO_NODISCARD static constexpr Status Failure(
        ErrorCode code, const char* message) noexcept {
        return {code, message};
    }
};

template<class T>
class AERO_NODISCARD Result final {
public:
    Result(const T& value)
        : hasValue_(true) {
        new (&storage_.value) T(value);
    }

    Result(T&& value) noexcept(std::is_nothrow_move_constructible<T>::value)
        : hasValue_(true) {
        new (&storage_.value) T(std::move(value));
    }

    Result(Status status) noexcept
        : hasValue_(false) {
        AERO_ASSERT(!status.IsOk());
        new (&storage_.status) Status(status);
    }

    Result(const Result& other)
        : hasValue_(other.hasValue_) {
        if (hasValue_) {
            new (&storage_.value) T(other.storage_.value);
        } else {
            new (&storage_.status) Status(other.storage_.status);
        }
    }

    Result(Result&& other) noexcept(std::is_nothrow_move_constructible<T>::value)
        : hasValue_(other.hasValue_) {
        if (hasValue_) {
            new (&storage_.value) T(std::move(other.storage_.value));
        } else {
            new (&storage_.status) Status(other.storage_.status);
        }
    }

    Result& operator=(const Result& other) {
        if (this != &other) {
            Destroy();
            hasValue_ = other.hasValue_;
            if (hasValue_) {
                new (&storage_.value) T(other.storage_.value);
            } else {
                new (&storage_.status) Status(other.storage_.status);
            }
        }
        return *this;
    }

    Result& operator=(Result&& other)
        noexcept(std::is_nothrow_move_constructible<T>::value &&
                 std::is_nothrow_move_assignable<T>::value) {
        if (this != &other) {
            Destroy();
            hasValue_ = other.hasValue_;
            if (hasValue_) {
                new (&storage_.value) T(std::move(other.storage_.value));
            } else {
                new (&storage_.status) Status(other.storage_.status);
            }
        }
        return *this;
    }

    ~Result() {
        Destroy();
    }

    AERO_NODISCARD bool HasValue() const noexcept {
        return hasValue_;
    }

    AERO_NODISCARD explicit operator bool() const noexcept {
        return HasValue();
    }

    AERO_NODISCARD T& Value() & noexcept {
        AERO_ASSERT(hasValue_);
        return storage_.value;
    }

    AERO_NODISCARD const T& Value() const& noexcept {
        AERO_ASSERT(hasValue_);
        return storage_.value;
    }

    AERO_NODISCARD T&& Value() && noexcept {
        AERO_ASSERT(hasValue_);
        return std::move(storage_.value);
    }

    AERO_NODISCARD Status GetStatus() const noexcept {
        return hasValue_ ? Status::Ok() : storage_.status;
    }

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
        } else {
            storage_.status.~Status();
        }
    }
};

template<>
class AERO_NODISCARD Result<void> final {
public:
    Result() noexcept = default;

    Result(Status status) noexcept
        : status_(status) {
        AERO_ASSERT(!status.IsOk());
    }

    AERO_NODISCARD bool HasValue() const noexcept {
        return status_.IsOk();
    }

    AERO_NODISCARD explicit operator bool() const noexcept {
        return HasValue();
    }

    AERO_NODISCARD Status GetStatus() const noexcept {
        return status_;
    }

private:
    Status status_ = Status::Ok();
};

} // namespace Aero::Base
