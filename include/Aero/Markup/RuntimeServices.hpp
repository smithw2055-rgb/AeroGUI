#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Platform/Clipboard.hpp>
#include <Aero/Platform/Ime.hpp>
#include <Aero/Presentation/MountService.hpp>

#include <cstdint>

namespace Aero::Markup {

// Source-compatible names retained for applications that adopted Slice C
// before MountService moved to Presentation. All mounting now uses the single
// Presentation implementation shared by XAML, templates and item generation.
using MountEdgeState = Presentation::MountEdgeState;
using MountTransactionService = Presentation::MountService;

struct RuntimeObjectState final {
    Presentation::VisualHandle handle;
    std::uint64_t layoutRevision = 0U;
    std::uint64_t renderRevision = 0U;
    std::uint32_t inputFlags = 0U;
    bool measureQueued = false;
    bool arrangeQueued = false;
    bool renderQueued = false;
};

class AERO_API RuntimeObjectStateStore final {
public:
    explicit RuntimeObjectStateStore(
        Base::IAllocator* allocator = nullptr) noexcept;

    Base::Result<RuntimeObjectState*> Ensure(
        Presentation::VisualHandle handle) noexcept;
    RuntimeObjectState* Find(
        Presentation::VisualHandle handle) noexcept;
    const RuntimeObjectState* Find(
        Presentation::VisualHandle handle) const noexcept;
    bool Remove(Presentation::VisualHandle handle) noexcept;
    std::uint32_t Prune(
        const Presentation::ObjectTree& tree) noexcept;
    void Clear() noexcept { states_.Clear(); }
    std::uint32_t Size() const noexcept { return states_.Size(); }

private:
    Base::Vector<RuntimeObjectState> states_;
};

struct HostTextServices final {
    Platform::IClipboard* clipboard = nullptr;
    Platform::ITextInputMethodHost* inputMethod = nullptr;

    bool HasClipboard() const noexcept { return clipboard != nullptr; }
    bool HasInputMethod() const noexcept { return inputMethod != nullptr; }
};

struct TextEditorControllerState final {
    std::uint32_t selectionAnchor = 0U;
    std::uint32_t caret = 0U;
    bool readOnly = false;
    bool composing = false;
};

struct TextEditorLayoutState final {
    Presentation::Size measured;
    Presentation::Rect caretRectangle;
    std::uint64_t revision = 0U;
};

struct TextEditorRenderState final {
    std::uint32_t glyphRunCount = 0U;
    std::uint64_t revision = 0U;
    bool caretVisible = false;
    bool selectionVisible = false;
};

} // namespace Aero::Markup
