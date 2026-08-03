#pragma once

#include "gui/GuiPrivate.hpp"

#include <Aero/Layout.hpp>

namespace Aero {

struct UIElement::Impl {
    static void*& LayoutManager(UIElement& element) noexcept {
        return element.layoutManager_;
    }
    static const void* LayoutManager(
        const UIElement& element) noexcept {
        return element.layoutManager_;
    }
    static bool& LayoutAttached(UIElement& element) noexcept {
        return element.layoutAttached_;
    }
    static bool& MeasureValid(UIElement& element) noexcept {
        return element.measureValid_;
    }
    static bool& ArrangeValid(UIElement& element) noexcept {
        return element.arrangeValid_;
    }
    static bool& MeasureQueued(UIElement& element) noexcept {
        return element.measureQueued_;
    }
    static bool& ArrangeQueued(UIElement& element) noexcept {
        return element.arrangeQueued_;
    }
    static bool& Measuring(UIElement& element) noexcept {
        return element.measuring_;
    }
    static bool& Arranging(UIElement& element) noexcept {
        return element.arranging_;
    }
    static Size& DesiredSize(UIElement& element) noexcept {
        return element.desiredSize_;
    }
    static Size& RenderSize(UIElement& element) noexcept {
        return element.renderSize_;
    }
    static Size& UntransformedDesiredSize(UIElement& element) noexcept {
        return element.untransformedDesiredSize_;
    }
    static Size& PreviousMeasureConstraint(UIElement& element) noexcept {
        return element.previousMeasureConstraint_;
    }
    static Rect& LayoutSlot(UIElement& element) noexcept {
        return element.layoutSlot_;
    }
    static Rect& LayoutClip(UIElement& element) noexcept {
        return element.layoutClip_;
    }
    static std::uint64_t& LayoutRevision(UIElement& element) noexcept {
        return element.layoutRevision_;
    }
    static Size MeasureOverride(
        UIElement& element,
        Size availableSize) noexcept {
        return element.MeasureOverride(availableSize);
    }
    static Size ArrangeOverride(
        UIElement& element,
        Size finalSize) noexcept {
        return element.ArrangeOverride(finalSize);
    }
    static void SetActualSize(
        FrameworkElement& element,
        double width,
        double height) noexcept {
        element.SetReadOnlyCurrentValue(
            FrameworkElement::ActualWidthProperty, width);
        element.SetReadOnlyCurrentValue(
            FrameworkElement::ActualHeightProperty, height);
    }
};

} // namespace Aero

namespace Aero::GuiPrivate::Detail {

using namespace Aero::Meta;
using namespace Aero::Threading;

class AERO_API LayoutEngine {
public:
    explicit LayoutEngine(Dispatcher& dispatcher) noexcept;
    ~LayoutEngine() noexcept;
    LayoutEngine(const LayoutEngine&) = delete;
    LayoutEngine& operator=(const LayoutEngine&) = delete;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> Attach(UIElement& parent, UIElement& child) noexcept;
    Base::Result<void> Detach(UIElement& parent, UIElement& child) noexcept;
    Base::Result<void> SetRoot(UIElement* root, Size availableSize) noexcept;
    Base::Result<void> InvalidateMeasure(UIElement& element) noexcept;
    Base::Result<void> InvalidateArrange(UIElement& element) noexcept;
    Base::Result<std::uint32_t> Flush() noexcept;
    LayoutDiagnostics Diagnostics() const noexcept;
    std::uint64_t PassVersion() const noexcept { return passVersion_; }
    Base::Status LastFlushStatus() const noexcept {
        return lastFlushStatus_;
    }

private:
    friend class Aero::UIElement;
    Dispatcher* dispatcher_ = nullptr;
    UIElement* root_ = nullptr;
    Size rootAvailableSize_;
    Base::Vector<Aero::GuiPrivate::Detail::VisualLease> measureQueue_;
    Base::Vector<Aero::GuiPrivate::Detail::VisualLease> arrangeQueue_;
    DispatcherFrameHookHandle phaseHook_;
    std::uint64_t passVersion_ = 0U;
    std::uint32_t measuredCount_ = 0U;
    std::uint32_t arrangedCount_ = 0U;
    Base::Status lastFlushStatus_;
    bool flushing_ = false;

    Base::Result<void> VerifyElement(const UIElement& element) const noexcept;
    Base::Result<void> QueueMeasure(UIElement& element) noexcept;
    Base::Result<void> QueueArrange(UIElement& element) noexcept;
    void RemoveQueued(UIElement& element) noexcept;
    Base::Result<void> MeasureElement(UIElement& element, Size constraint) noexcept;
    Base::Result<void> ArrangeElement(UIElement& element, Rect slot) noexcept;
    static void LayoutHook(void* context) noexcept;
};

} // namespace Aero::GuiPrivate::Detail
