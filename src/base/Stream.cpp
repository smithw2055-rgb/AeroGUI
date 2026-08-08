#include <Aero/Base/Stream.hpp>

namespace Aero::Base {

Stream::Stream() noexcept = default;
Stream::~Stream() = default;

bool Stream::CanSeek() const noexcept {
    return false;
}

Result<std::uint64_t> Stream::Position() const noexcept {
    return Status::Failure(
        ErrorCode::Unsupported,
        "Stream does not expose a position");
}

Result<std::uint64_t> Stream::Length() const noexcept {
    return Status::Failure(
        ErrorCode::Unsupported,
        "Stream does not expose a length");
}

Result<std::uint64_t> Stream::Seek(
    std::int64_t offset,
    SeekOrigin origin) noexcept {
    (void)offset;
    (void)origin;
    return Status::Failure(
        ErrorCode::Unsupported,
        "Stream does not support seeking");
}

} // namespace Aero::Base
