#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/ObjectTree.hpp>

#include <cstdint>

namespace Aero::Core {

struct Point final { double x = 0.0; double y = 0.0; };
struct Size final { double width = 0.0; double height = 0.0; };
struct Rect final { double x = 0.0; double y = 0.0; double width = 0.0; double height = 0.0; };
struct Thickness final { double left = 0.0; double top = 0.0; double right = 0.0; double bottom = 0.0; };

AERO_NODISCARD AERO_API bool IsFinite(Point value) noexcept;
AERO_NODISCARD AERO_API bool IsFinite(Size value) noexcept;
AERO_NODISCARD AERO_API bool IsFinite(Rect value) noexcept;
AERO_NODISCARD AERO_API bool IsFinite(Thickness value) noexcept;
AERO_NODISCARD AERO_API bool IsValidLayoutSize(Size value) noexcept;
AERO_NODISCARD AERO_API bool IsValidLayoutRect(Rect value) noexcept;
AERO_NODISCARD AERO_API Size Deflate(Size value, Thickness padding) noexcept;
AERO_NODISCARD AERO_API Size Inflate(Size value, Thickness padding) noexcept;
AERO_NODISCARD AERO_API Rect Intersect(Rect left, Rect right) noexcept;
AERO_NODISCARD AERO_API double RoundLayoutValue(double value, double dpiScale) noexcept;

class LayoutManager;

class AERO_API LayoutElement : public TreeNode {
public:
    LayoutElement(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId runtimeType, Base::IAllocator* allocator = nullptr) noexcept;
    ~LayoutElement() override;

    AERO_NODISCARD Base::Result<void> InvalidateMeasure() noexcept;
    AERO_NODISCARD Base::Result<void> InvalidateArrange() noexcept;
    AERO_NODISCARD Size DesiredSize() const noexcept { return desiredSize_; }
    AERO_NODISCARD Size RenderSize() const noexcept { return renderSize_; }
    AERO_NODISCARD Rect LayoutSlot() const noexcept { return layoutSlot_; }
    AERO_NODISCARD Rect LayoutClip() const noexcept { return layoutClip_; }
    AERO_NODISCARD bool IsMeasureValid() const noexcept { return measureValid_; }
    AERO_NODISCARD bool IsArrangeValid() const noexcept { return arrangeValid_; }
    AERO_NODISCARD bool ClipToBounds() const noexcept { return clipToBounds_; }
    AERO_NODISCARD bool UseLayoutRounding() const noexcept { return useLayoutRounding_; }
    AERO_NODISCARD double DpiScale() const noexcept { return dpiScale_; }
    AERO_NODISCARD std::uint64_t LayoutRevision() const noexcept { return layoutRevision_; }

    void SetClipToBounds(bool value) noexcept { clipToBounds_ = value; }
    AERO_NODISCARD Base::Result<void> SetLayoutRounding(
        bool enabled, double dpiScale = 1.0) noexcept;

protected:
    AERO_NODISCARD virtual Base::Result<Size> MeasureOverride(Size availableSize) noexcept;
    AERO_NODISCARD virtual Base::Result<Size> ArrangeOverride(Size finalSize) noexcept;
    AERO_NODISCARD Base::Result<void> MeasureChild(
        LayoutElement& child, Size availableSize) noexcept;
    AERO_NODISCARD Base::Result<void> ArrangeChild(
        LayoutElement& child, Rect finalRect) noexcept;
    AERO_NODISCARD Base::Span<LayoutElement* const> LayoutChildren() const noexcept {
        return {layoutChildren_.Data(), layoutChildren_.Size()};
    }

private:
    friend class LayoutManager;
    LayoutManager* manager_ = nullptr;
    LayoutElement* layoutParent_ = nullptr;
    Base::Vector<LayoutElement*> layoutChildren_;
    Size desiredSize_;
    Size renderSize_;
    Size previousMeasureConstraint_;
    Rect layoutSlot_;
    Rect layoutClip_;
    std::uint64_t layoutRevision_ = 0U;
    bool measureValid_ = false;
    bool arrangeValid_ = false;
    bool measureQueued_ = false;
    bool arrangeQueued_ = false;
    bool measuring_ = false;
    bool arranging_ = false;
    bool clipToBounds_ = false;
    bool useLayoutRounding_ = false;
    double dpiScale_ = 1.0;
};

struct LayoutDiagnostics final {
    std::uint64_t passVersion = 0U;
    std::uint32_t measuredCount = 0U;
    std::uint32_t arrangedCount = 0U;
    std::uint32_t pendingMeasureCount = 0U;
    std::uint32_t pendingArrangeCount = 0U;
};

class AERO_API LayoutManager final {
public:
    explicit LayoutManager(Dispatcher& dispatcher,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~LayoutManager() noexcept;
    LayoutManager(const LayoutManager&) = delete;
    LayoutManager& operator=(const LayoutManager&) = delete;

    AERO_NODISCARD Base::Result<void> Initialize() noexcept;
    AERO_NODISCARD Base::Result<void> Attach(LayoutElement& parent, LayoutElement& child) noexcept;
    AERO_NODISCARD Base::Result<void> Detach(LayoutElement& parent, LayoutElement& child) noexcept;
    AERO_NODISCARD Base::Result<void> SetRoot(LayoutElement* root, Size availableSize) noexcept;
    AERO_NODISCARD Base::Result<void> InvalidateMeasure(LayoutElement& element) noexcept;
    AERO_NODISCARD Base::Result<void> InvalidateArrange(LayoutElement& element) noexcept;
    AERO_NODISCARD Base::Result<std::uint32_t> Flush() noexcept;
    AERO_NODISCARD LayoutDiagnostics Diagnostics() const noexcept;
    AERO_NODISCARD std::uint64_t PassVersion() const noexcept { return passVersion_; }

private:
    friend class LayoutElement;
    Dispatcher* dispatcher_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    LayoutElement* root_ = nullptr;
    Size rootAvailableSize_;
    Base::Vector<LayoutElement*> measureQueue_;
    Base::Vector<LayoutElement*> arrangeQueue_;
    DispatcherFrameHookHandle phaseHook_;
    std::uint64_t passVersion_ = 0U;
    std::uint32_t measuredCount_ = 0U;
    std::uint32_t arrangedCount_ = 0U;
    bool flushing_ = false;

    AERO_NODISCARD Base::Result<void> VerifyElement(const LayoutElement& element) const noexcept;
    AERO_NODISCARD Base::Result<void> QueueMeasure(LayoutElement& element) noexcept;
    AERO_NODISCARD Base::Result<void> QueueArrange(LayoutElement& element) noexcept;
    AERO_NODISCARD Base::Result<void> MeasureElement(LayoutElement& element, Size constraint) noexcept;
    AERO_NODISCARD Base::Result<void> ArrangeElement(LayoutElement& element, Rect slot) noexcept;
    void RemoveChild(Base::Vector<LayoutElement*>& children, LayoutElement& child) noexcept;
    static void LayoutHook(void* context) noexcept;
};

} // namespace Aero::Core
