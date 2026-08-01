#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>

#include <cstdint>

namespace Aero::Integration {

class AERO_API IClipboard {
public:
    virtual ~IClipboard() = default;
    virtual Base::Result<void> ReadText(Base::String& output) noexcept = 0;
    virtual Base::Result<void> WriteText(Base::StringView text) noexcept = 0;
};

class AERO_API MemoryClipboard final : public IClipboard {
public:
    explicit MemoryClipboard(
        Base::IAllocator* allocator = nullptr) noexcept
        : text_(allocator) {}

    Base::Result<void> ReadText(
        Base::String& output) noexcept override;
    Base::Result<void> WriteText(
        Base::StringView text) noexcept override;
    std::uint64_t Generation() const noexcept {
        return generation_;
    }

private:
    Base::String text_;
    std::uint64_t generation_ = 0U;
};

struct ImeCandidateWindow final {
    Base::Rect caret;
    double dpiScale = 1.0;
};

class AERO_API ITextCompositionClient {
public:
    virtual ~ITextCompositionClient() = default;
    virtual Base::Result<void> BeginComposition() noexcept = 0;
    virtual Base::Result<void> UpdateComposition(
        Base::StringView text) noexcept = 0;
    virtual Base::Result<void> CommitComposition(
        Base::StringView text) noexcept = 0;
    virtual Base::Result<void> CancelComposition() noexcept = 0;
};

class AERO_API ITextInputMethodHost {
public:
    virtual ~ITextInputMethodHost() = default;
    virtual Base::Result<void> SetClient(
        ITextCompositionClient* client) noexcept = 0;
    virtual Base::Result<void> SetCandidateWindow(
        const ImeCandidateWindow& value) noexcept = 0;
    virtual Base::Result<void> CancelNativeComposition() noexcept = 0;
};


} // namespace Aero::Integration
