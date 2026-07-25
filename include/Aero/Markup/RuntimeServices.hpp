#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Platform/Clipboard.hpp>
#include <Aero/Platform/Ime.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Presentation/Rendering.hpp>

#include <cstdint>

namespace Aero::Markup {

struct MountEdgeState final {
    Presentation::VisualHandle child;
    bool logicalAttached = false;
    bool visualAttached = false;
    bool layoutAttached = false;
    bool renderAttached = false;
};

// Single transaction path for logical, visual, layout and render attachment.
// XAML, templates and item generation can share this service instead of
// duplicating sequencing and rollback rules.
class AERO_API MountTransactionService final {
public:
    MountTransactionService(
        Presentation::ObjectTree& tree,
        Presentation::LayoutManager& layout,
        Presentation::RenderManager* renderer = nullptr) noexcept;

    Base::Result<MountEdgeState> Attach(
        Presentation::Visual& parent,
        Presentation::Visual& child) noexcept;
    Base::Result<void> Detach(
        Presentation::Visual& parent,
        Presentation::Visual& child,
        MountEdgeState* state = nullptr) noexcept;

private:
    Presentation::ObjectTree* tree_ = nullptr;
    Presentation::LayoutManager* layout_ = nullptr;
    Presentation::RenderManager* renderer_ = nullptr;
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

// Private-style sidecar storage keyed by generation handles. It permits new
// runtime state without growing Visual/UIElement/FrameworkElement public ABI.
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

// Platform-neutral service bundle. Native clipboard and IME adapters remain in
// platform targets; controls consume only these contracts.
struct HostTextServices final {
    Platform::IClipboard* clipboard = nullptr;
    Platform::ITextInputMethodHost* inputMethod = nullptr;

    bool HasClipboard() const noexcept {
        return clipboard != nullptr;
    }
    bool HasInputMethod() const noexcept {
        return inputMethod != nullptr;
    }
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
