#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>

#include <cstdint>

namespace Aero::Platform {

struct ImeCandidateWindow final {
    Base::Rect caret;
    double dpiScale = 1.0;
};

class AERO_API ITextCompositionClient {
public:
    virtual ~ITextCompositionClient() = default;

    virtual Base::Result<void>
    BeginComposition() noexcept = 0;
    virtual Base::Result<void>
    UpdateComposition(
        Base::StringView text) noexcept = 0;
    virtual Base::Result<void>
    CommitComposition(
        Base::StringView text) noexcept = 0;
    virtual Base::Result<void>
    CancelComposition() noexcept = 0;
};

class AERO_API ITextInputMethodHost {
public:
    virtual ~ITextInputMethodHost() = default;

    virtual Base::Result<void> SetClient(
        ITextCompositionClient* client) noexcept = 0;
    virtual Base::Result<void>
    SetCandidateWindow(
        const ImeCandidateWindow& value) noexcept = 0;
    virtual Base::Result<void>
    CancelNativeComposition() noexcept = 0;
};

// Win32 Imm32 adapter. Public signatures keep HWND, WPARAM and LPARAM opaque.
// The host forwards native window messages to HandleMessage after Attach().
class AERO_API Win32ImeAdapter final
    : public ITextInputMethodHost {
public:
    Win32ImeAdapter() noexcept = default;
    ~Win32ImeAdapter() override;

    Base::Result<void> Attach(
        void* window) noexcept;
    Base::Result<bool> Detach() noexcept;
    bool IsAttached() const noexcept {
        return window_ != nullptr;
    }
    bool IsComposing() const noexcept {
        return composing_;
    }

    Base::Result<bool> HandleMessage(
        std::uint32_t message,
        std::uintptr_t wParam,
        std::intptr_t lParam) noexcept;

    Base::Result<void> SetClient(
        ITextCompositionClient* client) noexcept override;
    Base::Result<void> SetCandidateWindow(
        const ImeCandidateWindow& value) noexcept override;
    Base::Result<void>
    CancelNativeComposition() noexcept override;

private:
    void* window_ = nullptr;
    ITextCompositionClient* client_ = nullptr;
    bool composing_ = false;
};

} // namespace Aero::Platform
