#pragma once

#include "gui/core/Facet.hpp"
#include <Aero/Layout.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Span.hpp>
#include <algorithm>
#include <cmath>

namespace Aero::Core {

// 2-Pass Layout Facet contract (Measure & Arrange)
class LayoutFacet : public Facet {
public:
    static constexpr FacetType StaticType = FacetType::Layout;

    explicit LayoutFacet(::Aero::UIElement& owner) noexcept : Facet(&owner) {}

    virtual Size Measure(const Size& availableSize) = 0;
    virtual Size Arrange(const Size& finalSize) = 0;

    void InvalidateMeasure() noexcept { measureValid_ = false; }
    void InvalidateArrange() noexcept { arrangeValid_ = false; }

    // Read accessors (layout state now lives in the facet; UIElement getters
    // forward here).
    Size GetDesiredSize() const noexcept { return desiredSize_; }
    Size GetRenderSize() const noexcept { return renderSize_; }
    Size GetUntransformedDesiredSize() const noexcept { return untransformedDesiredSize_; }
    Size GetPreviousMeasureConstraint() const noexcept { return previousMeasureConstraint_; }
    Rect GetLayoutSlot() const noexcept { return layoutSlot_; }
    Rect GetLayoutClip() const noexcept { return layoutClip_; }
    Rect GetVisualRect() const noexcept { return visualRect_; }
    std::uint64_t GetLayoutRevision() const noexcept { return layoutRevision_; }
    bool IsMeasureValid() const noexcept { return measureValid_; }
    bool IsArrangeValid() const noexcept { return arrangeValid_; }
    bool IsMeasureQueued() const noexcept { return measureQueued_; }
    bool IsArrangeQueued() const noexcept { return arrangeQueued_; }
    bool IsMeasuring() const noexcept { return measuring_; }
    bool IsArranging() const noexcept { return arranging_; }
    bool IsLayoutAttached() const noexcept { return layoutAttached_; }

    // Mutable accessors forward to the per-element facet; the public static
    // forwarders below resolve the facet from the element and return these
    // references so callers (LayoutEngine) keep a single, uniform path.
    void SetDesiredSize(Size size) noexcept { desiredSize_ = size; }
    void SetRenderSize(Size size) noexcept { renderSize_ = size; }
    void SetUntransformedDesiredSize(Size size) noexcept { untransformedDesiredSize_ = size; }
    void SetPreviousMeasureConstraint(Size size) noexcept { previousMeasureConstraint_ = size; }
    void SetLayoutSlot(Rect rect) noexcept { layoutSlot_ = rect; }
    void SetLayoutClip(Rect rect) noexcept { layoutClip_ = rect; }
    void SetVisualRect(Rect rect) noexcept { visualRect_ = rect; }
    void SetLayoutRevision(std::uint64_t value) noexcept { layoutRevision_ = value; }
    void BumpLayoutRevision() noexcept { ++layoutRevision_; }
    void SetMeasureValid(bool value) noexcept { measureValid_ = value; }
    void SetArrangeValid(bool value) noexcept { arrangeValid_ = value; }
    void SetMeasureQueued(bool value) noexcept { measureQueued_ = value; }
    void SetArrangeQueued(bool value) noexcept { arrangeQueued_ = value; }
    void SetMeasuring(bool value) noexcept { measuring_ = value; }
    void SetArranging(bool value) noexcept { arranging_ = value; }
    void SetLayoutAttached(bool value) noexcept { layoutAttached_ = value; }

    static Size MeasureOverride(
        Aero::UIElement& element,
        Size availableSize) noexcept {
        return element.MeasureOverride(availableSize);
    }
    static Size ArrangeOverride(
        Aero::UIElement& element,
        Size finalSize) noexcept {
        return element.ArrangeOverride(finalSize);
    }
    static void SetActualSize(
        Aero::FrameworkElement& element,
        double width,
        double height) noexcept {
        element.SetReadOnlyCurrentValue(
            Aero::FrameworkElement::ActualWidthProperty, width);
        element.SetReadOnlyCurrentValue(
            Aero::FrameworkElement::ActualHeightProperty, height);
    }

protected:
    Size desiredSize_{0.0, 0.0};
    Size untransformedDesiredSize_{0.0, 0.0};
    Size renderSize_{0.0, 0.0};
    Size previousMeasureConstraint_{0.0, 0.0};
    Rect layoutSlot_{0.0, 0.0, 0.0, 0.0};
    Rect layoutClip_{0.0, 0.0, 0.0, 0.0};
    Rect visualRect_{0.0, 0.0, 0.0, 0.0};
    std::uint64_t layoutRevision_ = 0U;
    bool layoutAttached_ = false;
    bool measureValid_ = false;
    bool arrangeValid_ = false;
    bool measureQueued_ = false;
    bool arrangeQueued_ = false;
    bool measuring_ = false;
    bool arranging_ = false;
};

// Pure Data-Oriented Layout Calculators
struct StackLayoutCalculator {
    enum class Orientation : std::uint8_t { Horizontal, Vertical };

    static Size ComputeMeasure(
        Base::Span<const Size> childSizes,
        Size availableSize,
        Orientation orientation) noexcept {
        static_cast<void>(availableSize);
        Size desired{0.0, 0.0};
        for (const Size& childDesired : childSizes) {
            if (orientation == Orientation::Vertical) {
                desired.width = std::max(desired.width, childDesired.width);
                desired.height += childDesired.height;
            } else {
                desired.width += childDesired.width;
                desired.height = std::max(desired.height, childDesired.height);
            }
        }
        return desired;
    }

    static void ComputeArrange(
        Base::Span<const Size> childDesiredSizes,
        Rect finalRect,
        Orientation orientation,
        Base::Span<Rect> outChildRects) noexcept {
        double offset = 0.0;
        const std::uint32_t count = childDesiredSizes.Size() < outChildRects.Size() ? childDesiredSizes.Size() : outChildRects.Size();
        for (std::uint32_t i = 0; i < count; ++i) {
            const Size& desired = childDesiredSizes[i];
            if (orientation == Orientation::Vertical) {
                outChildRects[i] = Rect{finalRect.x, finalRect.y + offset, finalRect.width, desired.height};
                offset += desired.height;
            } else {
                outChildRects[i] = Rect{finalRect.x + offset, finalRect.y, desired.width, finalRect.height};
                offset += desired.width;
            }
        }
    }
};

struct CanvasLayoutCalculator {
    struct ChildPlacement {
        Size desiredSize;
        double left = 0.0;
        double top = 0.0;
        double right = 0.0;
        double bottom = 0.0;
        bool hasLeft = false;
        bool hasTop = false;
        bool hasRight = false;
        bool hasBottom = false;
    };

    static Size ComputeMeasure(Base::Span<const ChildPlacement> children) noexcept {
        Size desired{0.0, 0.0};
        for (const auto& child : children) {
            double x = child.hasLeft ? child.left : 0.0;
            double y = child.hasTop ? child.top : 0.0;
            desired.width = std::max(desired.width, x + child.desiredSize.width);
            desired.height = std::max(desired.height, y + child.desiredSize.height);
        }
        return desired;
    }

    static void ComputeArrange(
        Base::Span<const ChildPlacement> children,
        Rect finalRect,
        Base::Span<Rect> outChildRects) noexcept {
        const std::uint32_t count = children.Size() < outChildRects.Size() ? children.Size() : outChildRects.Size();
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto& child = children[i];
            double x = 0.0;
            double y = 0.0;
            if (child.hasLeft) {
                x = child.left;
            } else if (child.hasRight) {
                x = finalRect.width - child.right - child.desiredSize.width;
            }
            if (child.hasTop) {
                y = child.top;
            } else if (child.hasBottom) {
                y = finalRect.height - child.bottom - child.desiredSize.height;
            }
            outChildRects[i] = Rect{finalRect.x + x, finalRect.y + y, child.desiredSize.width, child.desiredSize.height};
        }
    }
};

struct DockLayoutCalculator {
    enum class Dock : std::uint8_t { Left = 0, Top = 1, Right = 2, Bottom = 3 };

    struct ChildPlacement {
        Size desiredSize;
        Dock dock = Dock::Left;
    };

    static Size ComputeMeasure(
        Base::Span<const ChildPlacement> children,
        bool lastChildFill) noexcept {
        Size parentSize{0.0, 0.0};
        double accumulatedWidth = 0.0;
        double accumulatedHeight = 0.0;

        const std::uint32_t count = children.Size();
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto& child = children[i];
            const bool isLast = (i + 1U == count);

            if (isLast && lastChildFill) {
                parentSize.width = std::max(parentSize.width, accumulatedWidth + child.desiredSize.width);
                parentSize.height = std::max(parentSize.height, accumulatedHeight + child.desiredSize.height);
            } else {
                switch (child.dock) {
                case Dock::Left:
                case Dock::Right:
                    accumulatedWidth += child.desiredSize.width;
                    parentSize.height = std::max(parentSize.height, accumulatedHeight + child.desiredSize.height);
                    break;
                case Dock::Top:
                case Dock::Bottom:
                    accumulatedHeight += child.desiredSize.height;
                    parentSize.width = std::max(parentSize.width, accumulatedWidth + child.desiredSize.width);
                    break;
                }
            }
        }
        parentSize.width = std::max(parentSize.width, accumulatedWidth);
        parentSize.height = std::max(parentSize.height, accumulatedHeight);
        return parentSize;
    }

    static void ComputeArrange(
        Base::Span<const ChildPlacement> children,
        Rect finalRect,
        bool lastChildFill,
        Base::Span<Rect> outChildRects) noexcept {
        double left = 0.0;
        double top = 0.0;
        double right = 0.0;
        double bottom = 0.0;

        const std::uint32_t count = children.Size() < outChildRects.Size() ? children.Size() : outChildRects.Size();
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto& child = children[i];
            const bool isLast = (i + 1U == count);

            if (isLast && lastChildFill) {
                outChildRects[i] = Rect{
                    finalRect.x + left,
                    finalRect.y + top,
                    std::max(0.0, finalRect.width - left - right),
                    std::max(0.0, finalRect.height - top - bottom)
                };
            } else {
                switch (child.dock) {
                case Dock::Left:
                    outChildRects[i] = Rect{
                        finalRect.x + left,
                        finalRect.y + top,
                        child.desiredSize.width,
                        std::max(0.0, finalRect.height - top - bottom)
                    };
                    left += child.desiredSize.width;
                    break;
                case Dock::Right:
                    outChildRects[i] = Rect{
                        finalRect.x + std::max(0.0, finalRect.width - right - child.desiredSize.width),
                        finalRect.y + top,
                        child.desiredSize.width,
                        std::max(0.0, finalRect.height - top - bottom)
                    };
                    right += child.desiredSize.width;
                    break;
                case Dock::Top:
                    outChildRects[i] = Rect{
                        finalRect.x + left,
                        finalRect.y + top,
                        std::max(0.0, finalRect.width - left - right),
                        child.desiredSize.height
                    };
                    top += child.desiredSize.height;
                    break;
                case Dock::Bottom:
                    outChildRects[i] = Rect{
                        finalRect.x + left,
                        finalRect.y + std::max(0.0, finalRect.height - bottom - child.desiredSize.height),
                        std::max(0.0, finalRect.width - left - right),
                        child.desiredSize.height
                    };
                    bottom += child.desiredSize.height;
                    break;
                }
            }
        }
    }
};

struct WrapLayoutCalculator {
    enum class Orientation : std::uint8_t { Horizontal = 0, Vertical = 1 };

    static Size ComputeMeasure(
        Base::Span<const Size> childSizes,
        Size availableSize,
        Orientation orientation,
        double itemWidth = 0.0,
        double itemHeight = 0.0) noexcept {
        Size curLineSize{0.0, 0.0};
        Size panelSize{0.0, 0.0};
        const bool isHorizontal = (orientation == Orientation::Horizontal);
        const double maxExtent = isHorizontal ? availableSize.width : availableSize.height;

        for (const Size& sz : childSizes) {
            Size childSize = sz;
            if (itemWidth > 0.0) childSize.width = itemWidth;
            if (itemHeight > 0.0) childSize.height = itemHeight;

            const double childExtent = isHorizontal ? childSize.width : childSize.height;
            const double childThickness = isHorizontal ? childSize.height : childSize.width;

            if (curLineSize.width + childExtent > maxExtent && curLineSize.width > 0.0) {
                panelSize.width = std::max(panelSize.width, curLineSize.width);
                panelSize.height += curLineSize.height;
                curLineSize.width = childExtent;
                curLineSize.height = childThickness;
            } else {
                curLineSize.width += childExtent;
                curLineSize.height = std::max(curLineSize.height, childThickness);
            }
        }

        panelSize.width = std::max(panelSize.width, curLineSize.width);
        panelSize.height += curLineSize.height;

        return isHorizontal ? panelSize : Size{panelSize.height, panelSize.width};
    }
};

struct GridLayoutCalculator {
    enum class GridUnitType : std::uint8_t { Auto = 0, Pixel = 1, Star = 2 };

    struct TrackDef {
        double userValue = 1.0;
        GridUnitType unitType = GridUnitType::Star;
        double minSize = 0.0;
        double maxSize = 1.0e12;
    };

    struct TrackResult {
        double offset = 0.0;
        double length = 0.0;
    };

    static void ResolveTracks(
        Base::Span<const TrackDef> tracks,
        double availableSize,
        Base::Span<TrackResult> outTracks) noexcept {
        const std::uint32_t count = tracks.Size() < outTracks.Size() ? tracks.Size() : outTracks.Size();
        if (count == 0U) return;

        double remaining = availableSize;
        double totalStar = 0.0;

        for (std::uint32_t i = 0; i < count; ++i) {
            const auto& def = tracks[i];
            if (def.unitType == GridUnitType::Pixel) {
                double len = std::max(def.minSize, std::min(def.maxSize, def.userValue));
                outTracks[i].length = len;
                remaining = std::max(0.0, remaining - len);
            } else if (def.unitType == GridUnitType::Auto) {
                double len = std::max(def.minSize, def.userValue);
                outTracks[i].length = len;
                remaining = std::max(0.0, remaining - len);
            } else {
                totalStar += std::max(0.0, def.userValue);
                outTracks[i].length = 0.0;
            }
        }

        if (totalStar > 0.0 && remaining > 0.0) {
            for (std::uint32_t i = 0; i < count; ++i) {
                const auto& def = tracks[i];
                if (def.unitType == GridUnitType::Star) {
                    double share = (def.userValue / totalStar) * remaining;
                    double len = std::max(def.minSize, std::min(def.maxSize, share));
                    outTracks[i].length = len;
                }
            }
        }

        double curOffset = 0.0;
        for (std::uint32_t i = 0; i < count; ++i) {
            outTracks[i].offset = curOffset;
            curOffset += outTracks[i].length;
        }
    }
};

template<>
struct FacetTrait<LayoutFacet> {
    static constexpr std::uint32_t Id = static_cast<std::uint32_t>(FacetId::Layout);
    static constexpr FacetType Type = FacetType::Layout;
};

} // namespace Aero::Core
