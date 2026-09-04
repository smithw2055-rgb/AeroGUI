#pragma once

#include <Aero/Layout.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/Threading.hpp>
#include "gui/core/VisualHandle.hpp"

namespace Aero {

using namespace Aero::Threading;

class LayoutEngine {
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
    // P3.2 explicit Layout phase entry (formerly the frame-hook body).
    // ViewFrame calls it directly; no hook registration remains.
    static void LayoutHook(void* context) noexcept;
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
    Base::Vector<VisualHandle> measureQueue_;
    Base::Vector<VisualHandle> arrangeQueue_;
    bool initialized_ = false;
    std::uint64_t passVersion_ = 0U;
    std::uint32_t measuredCount_ = 0U;
    std::uint32_t arrangedCount_ = 0U;
    Base::Status lastFlushStatus_;
    bool flushing_ = false;

    Base::Result<void> VerifyElement(const UIElement& element) const noexcept;
    Base::Result<void> QueueMeasure(UIElement& element) noexcept;
    Base::Result<void> QueueArrange(UIElement& element) noexcept;
    void RemoveQueued(UIElement& element) noexcept;
    UIElement* ResolveQueued(VisualHandle handle) const noexcept;
    Base::Result<VisualHandle> EnqueueHandle(UIElement& element) noexcept;
    Base::Result<void> MeasureElement(UIElement& element, Size constraint) noexcept;
    Base::Result<void> ArrangeElement(UIElement& element, Rect slot) noexcept;
};

} // namespace Aero
