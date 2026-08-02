#pragma once

#include <Aero/Integration/Platform.hpp>

#include <cstdint>

namespace Aero::Platform {

class AERO_API Win32Clipboard final : public Integration::IClipboard {
public:
    explicit Win32Clipboard(
        void* ownerWindow = nullptr) noexcept
        : ownerWindow_(ownerWindow) {}

    void SetOwnerWindow(void* value) noexcept {
        ownerWindow_ = value;
    }
    void* OwnerWindow() const noexcept {
        return ownerWindow_;
    }
    Base::Result<void> ReadText(
        Base::String& output) noexcept override;
    Base::Result<void> WriteText(
        Base::StringView text) noexcept override;

private:
    void* ownerWindow_ = nullptr;
};

std::intptr_t DispatchWin32ImeWindowMessage(
    void* window,
    std::uint32_t message,
    std::uintptr_t wParam,
    std::intptr_t lParam) noexcept;

class AERO_API Win32ImeAdapter final
    : public Integration::ITextInputMethodHost {
public:
    Win32ImeAdapter() noexcept = default;
    ~Win32ImeAdapter() override;

    Base::Result<void> Attach(void* window) noexcept;
    Base::Result<bool> AttachActiveWindow() noexcept;
    Base::Result<bool> Detach() noexcept;
    bool IsAttached() const noexcept {
        return window_ != nullptr;
    }
    void* AttachedWindow() const noexcept {
        return window_;
    }
    bool IsComposing() const noexcept {
        return composing_;
    }
    Base::Result<bool> HandleMessage(
        std::uint32_t message,
        std::uintptr_t wParam,
        std::intptr_t lParam) noexcept;
    void SetClient(
        Integration::ITextCompositionClient* client) noexcept override;
    void SetCandidateWindow(
        const Integration::ImeCandidateWindow& value) noexcept override;
    Base::Result<void> CancelNativeComposition() noexcept override;

private:
    friend std::intptr_t DispatchWin32ImeWindowMessage(
        void*, std::uint32_t,
        std::uintptr_t, std::intptr_t) noexcept;

    void* window_ = nullptr;
    void* previousProcedure_ = nullptr;
    Integration::ITextCompositionClient* client_ = nullptr;
    bool composing_ = false;
    bool caretCreated_ = false;
    bool caretVisible_ = false;
    int caretWidth_ = 0;
    int caretHeight_ = 0;

    void DestroyNativeCaret() noexcept;
};

} // namespace Aero::Platform
