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

using MountEdgeState = Presentation::MountEdgeState;

// Compatibility facade for early Slice C callers. It contains no independent
// attachment logic; every operation delegates to Presentation::MountService.
class MountTransactionService final {
public:
    MountTransactionService(
        Presentation::ObjectTree& tree,
        Presentation::LayoutManager& layout,
        Presentation::RenderManager* renderer = nullptr) noexcept
        : service_(tree, &layout, renderer) {}

    Base::Result<MountEdgeState> Attach(
        Presentation::Visual& parent,
        Presentation::Visual& child) noexcept {
        return service_.Attach(parent, child);
    }

    Base::Result<void> Detach(
        Presentation::Visual& parent,
        Presentation::Visual& child,
        MountEdgeState* state = nullptr) noexcept {
        if (state != nullptr) return service_.Detach(*state);

        MountEdgeState current;
        current.logicalParent = &parent;
        current.visualParent = &parent;
        current.child = &child;
        current.childHandle = child.Handle();
        current.logicalAttached = child.LogicalParent() == &parent;
        current.visualAttached = child.VisualParent() == &parent;
        current.layoutAttached = current.visualAttached &&
            parent.AsUIElement() != nullptr &&
            child.AsUIElement() != nullptr;
        current.renderAttached = current.visualAttached &&
            service_.Renderer() != nullptr &&
            parent.AsFrameworkElement() != nullptr &&
            child.AsFrameworkElement() != nullptr;
        return service_.Detach(current);
    }

private:
    Presentation::MountService service_;
};

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
    Base::Result<RuntimeObjectState*> Ensure(
        const Presentation::Visual* visual) noexcept {
        return Ensure(visual != nullptr
            ? visual->Handle()
            : Presentation::VisualHandle{});
    }
    RuntimeObjectState* Find(
        Presentation::VisualHandle handle) noexcept;
    RuntimeObjectState* Find(
        const Presentation::Visual* visual) noexcept {
        return Find(visual != nullptr
            ? visual->Handle()
            : Presentation::VisualHandle{});
    }
    const RuntimeObjectState* Find(
        Presentation::VisualHandle handle) const noexcept;
    const RuntimeObjectState* Find(
        const Presentation::Visual* visual) const noexcept {
        return Find(visual != nullptr
            ? visual->Handle()
            : Presentation::VisualHandle{});
    }
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
