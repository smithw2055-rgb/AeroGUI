#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>

#include <cstdint>

namespace Aero::Base {

enum class SeekOrigin : std::uint8_t {
    Begin = 0U,
    Current,
    End,
};

// Synchronous, read-only byte stream used by XAML, image and resource
// providers. Short reads are valid; a successful zero-byte read means EOF.
class AERO_API Stream : public Object {
public:
    ~Stream() override = default;

    virtual bool CanRead() const noexcept = 0;
    virtual Result<std::uint32_t> Read(
        Span<std::uint8_t> destination) noexcept = 0;

    virtual bool CanSeek() const noexcept { return false; }
    virtual Result<std::uint64_t> Position() const noexcept {
        return Status::Failure(
            ErrorCode::Unsupported,
            "Stream does not expose a position");
    }
    virtual Result<std::uint64_t> Length() const noexcept {
        return Status::Failure(
            ErrorCode::Unsupported,
            "Stream does not expose a length");
    }
    virtual Result<std::uint64_t> Seek(
        std::int64_t offset,
        SeekOrigin origin) noexcept {
        (void)offset;
        (void)origin;
        return Status::Failure(
            ErrorCode::Unsupported,
            "Stream does not support seeking");
    }

protected:
    Stream() noexcept = default;
};

} // namespace Aero::Base
