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
    explicit MemoryClipboard(Base::IAllocator* allocator = nullptr) noexcept : text_(allocator) {}
    Base::Result<void> ReadText(Base::String& output) noexcept override;
    Base::Result<void> WriteText(Base::StringView text) noexcept override;
    std::uint64_t Generation() const noexcept { return generation_; }
private:
    Base::String text_;
    std::uint64_t generation_ = 0U;
};

class AERO_API Win32Clipboard final : public IClipboard {
public:
    explicit Win32Clipboard(void* ownerWindow = nullptr) noexcept : ownerWindow_(ownerWindow) {}
    void SetOwnerWindow(void* value) noexcept { ownerWindow_ = value; }
    void* OwnerWindow() const noexcept { return ownerWindow_; }
    Base::Result<void> ReadText(Base::String& output) noexcept override;
    Base::Result<void> WriteText(Base::StringView text) noexcept override;
private:
    void* ownerWindow_ = nullptr;
};

struct ImeCandidateWindow final {
    Base::Rect caret;
    double dpiScale = 1.0;
};

class AERO_API ITextCompositionClient {
public:
    virtual ~ITextCompositionClient() = default;
    virtual Base::Result<void> BeginComposition() noexcept = 0;
    virtual Base::Result<void> UpdateComposition(Base::StringView text) noexcept = 0;
    virtual Base::Result<void> CommitComposition(Base::StringView text) noexcept = 0;
    virtual Base::Result<void> CancelComposition() noexcept = 0;
};

class AERO_API ITextInputMethodHost {
public:
    virtual ~ITextInputMethodHost() = default;
    virtual Base::Result<void> SetClient(ITextCompositionClient* client) noexcept = 0;
    virtual Base::Result<void> SetCandidateWindow(const ImeCandidateWindow& value) noexcept = 0;
    virtual Base::Result<void> CancelNativeComposition() noexcept = 0;
};

std::intptr_t DispatchWin32ImeWindowMessage(void* window, std::uint32_t message,
    std::uintptr_t wParam, std::intptr_t lParam) noexcept;

class AERO_API Win32ImeAdapter final : public ITextInputMethodHost {
public:
    Win32ImeAdapter() noexcept = default;
    ~Win32ImeAdapter() override;
    Base::Result<void> Attach(void* window) noexcept;
    Base::Result<bool> AttachActiveWindow() noexcept;
    Base::Result<bool> Detach() noexcept;
    bool IsAttached() const noexcept { return window_ != nullptr; }
    void* AttachedWindow() const noexcept { return window_; }
    bool IsComposing() const noexcept { return composing_; }
    Base::Result<bool> HandleMessage(std::uint32_t message, std::uintptr_t wParam,
        std::intptr_t lParam) noexcept;
    Base::Result<void> SetClient(ITextCompositionClient* client) noexcept override;
    Base::Result<void> SetCandidateWindow(const ImeCandidateWindow& value) noexcept override;
    Base::Result<void> CancelNativeComposition() noexcept override;
private:
    friend std::intptr_t DispatchWin32ImeWindowMessage(void*, std::uint32_t,
        std::uintptr_t, std::intptr_t) noexcept;
    void* window_ = nullptr;
    void* previousProcedure_ = nullptr;
    ITextCompositionClient* client_ = nullptr;
    bool composing_ = false;
    bool caretCreated_ = false;
    bool caretVisible_ = false;
    int caretWidth_ = 0;
    int caretHeight_ = 0;
    void DestroyNativeCaret() noexcept;
};

struct HostServices final {
    IClipboard* clipboard = nullptr;
    ITextInputMethodHost* textInputMethod = nullptr;
};

} // namespace Aero::Integration
