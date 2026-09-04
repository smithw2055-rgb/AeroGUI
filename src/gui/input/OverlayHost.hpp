#pragma once

// Source-only popup / tooltip / context-menu chrome next to InputRouter.
// Not installed under include/Aero. Included from ViewState.hpp after ViewState.

#include <cstdint>

namespace Aero {

class OverlayHost {
public:
    explicit OverlayHost(ViewState& owner) noexcept;
    void Bind() noexcept;

    ViewState* view = nullptr;
    Base::IAllocator* allocator = nullptr;
    ::Aero::Meta::Registry* metadata = nullptr;
    Aero::InputRouter* input = nullptr;
    ::Aero::Render::RenderTree* renderer = nullptr;

    Base::Vector<Aero::FrameworkElement*> renderOverlays;
    Base::Vector<Aero::UIElement*> inputOverlays;
    Base::Vector<Aero::Base::Transform2D> overlayTransforms;
    Base::Ref<Controls::ToolTip> pendingToolTip;
    Base::Ref<Controls::ToolTip> activeToolTip;
    Base::Ref<Aero::UIElement> toolTipTarget;
    Base::Ref<Aero::UIElement> overlayFocusReturn;
    std::uint32_t toolTipElapsed = 0U;
    std::uint32_t toolTipVisibleElapsed = 0U;

    Base::Result<void> SynchronizeOverlays() noexcept;
    void ClearOverlays() noexcept;
    void CloseAllOverlays() noexcept;
    Base::Result<void> RestoreOverlayFocus() noexcept;
    Base::Result<void> DismissOverlaysForPointer(
        const Input::PointerInput& pointer,
        Aero::UIElement* target) noexcept;
    Base::Result<bool> DismissTopOverlayForEscape() noexcept;
    Base::Result<void> OpenContextMenuForPointer(
        const Input::PointerInput& pointer,
        Aero::UIElement* hitTarget) noexcept;
    Base::Result<void> UpdateToolTipForPointer(
        const Input::PointerInput& pointer,
        Aero::UIElement* hitTarget) noexcept;
    Base::Result<std::uint32_t> AdvanceToolTipTime(
        std::uint32_t elapsedMilliseconds) noexcept;

private:
    static bool IsVisualDescendantOrSelf(
        const Aero::Media::Visual& root,
        const Aero::Media::Visual& target) noexcept;
};

} // namespace Aero
