#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>

#include <cstdint>

namespace Aero::Input {

class AERO_GUI_API IClipboard {
public:
    virtual ~IClipboard() = default;
    virtual Result<void> ReadText(String& output) noexcept = 0;
    virtual Result<void> WriteText(StringView text) noexcept = 0;
};

class AERO_GUI_API MemoryClipboard : public IClipboard {
public:
    explicit MemoryClipboard(
        Base::IAllocator* allocator = nullptr) noexcept
        : text_(allocator) {}

    Result<void> ReadText(
        String& output) noexcept override;
    Result<void> WriteText(
        StringView text) noexcept override;
    std::uint64_t Generation() const noexcept {
        return generation_;
    }

private:
    String text_;
    std::uint64_t generation_ = 0U;
};

struct ImeCandidateWindow {
    Base::Rect caret;
    double dpiScale = 1.0;
};

class AERO_GUI_API ITextCompositionClient {
public:
    virtual ~ITextCompositionClient() = default;
    virtual Result<void> BeginComposition() noexcept = 0;
    virtual Result<void> UpdateComposition(
        StringView text) noexcept = 0;
    virtual Result<void> CommitComposition(
        StringView text) noexcept = 0;
    virtual Result<void> CancelComposition() noexcept = 0;
};

class AERO_GUI_API ITextInputMethodHost {
public:
    virtual ~ITextInputMethodHost() = default;
    virtual void SetClient(
        ITextCompositionClient* client) noexcept = 0;
    virtual void SetCandidateWindow(
        const ImeCandidateWindow& value) noexcept = 0;
    virtual Result<void> CancelNativeComposition() noexcept = 0;
};


} // namespace Aero::Input
