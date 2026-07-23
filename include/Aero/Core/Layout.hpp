#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/ObjectTree.hpp>

#include <cstdint>

namespace Aero::Core {

struct MetaRegistrationContext;

using Point = Base::Point;
using Size = Base::Size;
using Rect = Base::Rect;
using Thickness = Base::Thickness;

enum class HorizontalAlignment : std::uint8_t { Stretch = 0U, Left, Center, Right };
enum class VerticalAlignment : std::uint8_t { Stretch = 0U, Top, Center, Bottom };

struct Length final {
    double value = 0.0;
    bool isAuto = true;

    static constexpr Length Auto() noexcept { return {}; }
    static constexpr Length Pixels(double value) noexcept {
        return {value, false};
    }
};

AERO_API bool IsFinite(Point value) noexcept;
AERO_API bool IsFinite(Size value) noexcept;
AERO_API bool IsFinite(Rect value) noexcept;
AERO_API bool IsFinite(Thickness value) noexcept;
AERO_API bool IsValidLayoutSize(Size value) noexcept;
AERO_API bool IsValidLayoutRect(Rect value) noexcept;
AERO_API Size Deflate(Size value, Thickness padding) noexcept;
AERO_API Size Inflate(Size value, Thickness padding) noexcept;
AERO_API Rect Intersect(Rect left, Rect right) noexcept;
AERO_API double RoundLayoutValue(double value, double dpiScale) noexcept;

class LayoutManager;

class AERO_API LayoutElement : public TreeNode {
    AERO_DECLARE_METADATA(LayoutElement, TreeNode)
public:
    LayoutElement(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId runtimeType, Base::IAllocator* allocator = nullptr) noexcept;
    ~LayoutElement() override;

    Base::Result<void> InvalidateMeasure() noexcept;
    Base::Result<void> InvalidateArrange() noexcept;
    Size DesiredSize() const noexcept { return desiredSize_; }
    Size RenderSize() const noexcept { return renderSize_; }
    Rect LayoutSlot() const noexcept { return layoutSlot_; }
    Rect LayoutClip() const noexcept { return layoutClip_; }
    bool IsMeasureValid() const noexcept { return measureValid_; }
    bool IsArrangeValid() const noexcept { return arrangeValid_; }
    bool ClipToBounds() const noexcept;
    bool IsHitTestVisible() const noexcept;
    bool UseLayoutRounding() const noexcept;
    double DpiScale() const noexcept { return dpiScale_; }
    std::uint64_t LayoutRevision() const noexcept { return layoutRevision_; }
    bool HasWidth() const noexcept;
    bool HasHeight() const noexcept;
    double Width() const noexcept;
    double Height() const noexcept;
    Size MinSize() const noexcept;
    Size MaxSize() const noexcept;
    Thickness Margin() const noexcept;
    HorizontalAlignment GetHorizontalAlignment() const noexcept;
    VerticalAlignment GetVerticalAlignment() const noexcept;

    inline static constexpr DependencyPropertyHandle WidthProperty =
        MakeDependencyPropertyHandle(StaticTypeIdValue_, "Width");
    inline static constexpr DependencyPropertyHandle HeightProperty =
        MakeDependencyPropertyHandle(StaticTypeIdValue_, "Height");
    inline static constexpr DependencyPropertyHandle MinWidthProperty =
        MakeDependencyPropertyHandle(StaticTypeIdValue_, "MinWidth");
    inline static constexpr DependencyPropertyHandle MaxWidthProperty =
        MakeDependencyPropertyHandle(StaticTypeIdValue_, "MaxWidth");
    inline static constexpr DependencyPropertyHandle MinHeightProperty =
        MakeDependencyPropertyHandle(StaticTypeIdValue_, "MinHeight");
    inline static constexpr DependencyPropertyHandle MaxHeightProperty =
        MakeDependencyPropertyHandle(StaticTypeIdValue_, "MaxHeight");
    inline static constexpr DependencyPropertyHandle MarginProperty =
        MakeDependencyPropertyHandle(StaticTypeIdValue_, "Margin");
    inline static constexpr DependencyPropertyHandle HorizontalAlignmentProperty =
        MakeDependencyPropertyHandle(StaticTypeIdValue_, "HorizontalAlignment");
    inline static constexpr DependencyPropertyHandle VerticalAlignmentProperty =
        MakeDependencyPropertyHandle(StaticTypeIdValue_, "VerticalAlignment");
    inline static constexpr DependencyPropertyHandle ClipToBoundsProperty =
        MakeDependencyPropertyHandle(StaticTypeIdValue_, "ClipToBounds");
    inline static constexpr DependencyPropertyHandle IsHitTestVisibleProperty =
        MakeDependencyPropertyHandle(StaticTypeIdValue_, "IsHitTestVisible");
    inline static constexpr DependencyPropertyHandle UseLayoutRoundingProperty =
        MakeDependencyPropertyHandle(StaticTypeIdValue_, "UseLayoutRounding");

    Base::Result<void> SetClipToBounds(bool value) noexcept;
    Base::Result<void> SetHitTestVisible(bool value) noexcept;
    Base::Result<void> SetLayoutRounding(
        bool enabled, double dpiScale = 1.0) noexcept;
    Base::Result<void> SetWidth(double value) noexcept;
    Base::Result<void> ClearWidth() noexcept;
    Base::Result<void> SetHeight(double value) noexcept;
    Base::Result<void> ClearHeight() noexcept;
    Base::Result<void> SetMinSize(Size value) noexcept;
    Base::Result<void> SetMaxSize(Size value) noexcept;
    Base::Result<void> SetMargin(Thickness value) noexcept;
    Base::Result<void> SetHorizontalAlignment(
        HorizontalAlignment value) noexcept;
    Base::Result<void> SetVerticalAlignment(
        VerticalAlignment value) noexcept;

protected:
    Base::Result<void> OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;
    virtual Base::Result<Size> MeasureOverride(Size availableSize) noexcept;
    virtual Base::Result<Size> ArrangeOverride(Size finalSize) noexcept;
    Base::Result<void> MeasureChild(
        LayoutElement& child, Size availableSize) noexcept;
    Base::Result<void> ArrangeChild(
        LayoutElement& child, Rect finalRect) noexcept;
    Base::Span<LayoutElement* const> LayoutChildren() const noexcept {
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

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> Attach(LayoutElement& parent, LayoutElement& child) noexcept;
    Base::Result<void> Detach(LayoutElement& parent, LayoutElement& child) noexcept;
    Base::Result<void> SetRoot(LayoutElement* root, Size availableSize) noexcept;
    Base::Result<void> InvalidateMeasure(LayoutElement& element) noexcept;
    Base::Result<void> InvalidateArrange(LayoutElement& element) noexcept;
    Base::Result<std::uint32_t> Flush() noexcept;
    LayoutDiagnostics Diagnostics() const noexcept;
    std::uint64_t PassVersion() const noexcept { return passVersion_; }

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

    Base::Result<void> VerifyElement(const LayoutElement& element) const noexcept;
    Base::Result<void> QueueMeasure(LayoutElement& element) noexcept;
    Base::Result<void> QueueArrange(LayoutElement& element) noexcept;
    Base::Result<void> MeasureElement(LayoutElement& element, Size constraint) noexcept;
    Base::Result<void> ArrangeElement(LayoutElement& element, Rect slot) noexcept;
    void RemoveChild(Base::Vector<LayoutElement*>& children, LayoutElement& child) noexcept;
    static void LayoutHook(void* context) noexcept;
};

} // namespace Aero::Core
