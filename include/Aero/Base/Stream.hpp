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
class AERO_BASE_API Stream : public Object {
public:
    ~Stream() override;

    virtual bool CanRead() const noexcept = 0;
    virtual Result<std::uint32_t> Read(
        Span<std::uint8_t> destination) noexcept = 0;

    virtual bool CanSeek() const noexcept;
    virtual Result<std::uint64_t> Position() const noexcept;
    virtual Result<std::uint64_t> Length() const noexcept;
    virtual Result<std::uint64_t> Seek(
        std::int64_t offset,
        SeekOrigin origin) noexcept;

protected:
    Stream() noexcept;
};

} // namespace Aero::Base
