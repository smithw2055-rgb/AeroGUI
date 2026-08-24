#pragma once

#include "gui/core/facets/LayoutFacet.hpp"
#include <Aero/UIElement.hpp>

namespace Aero::Core {

// Every UIElement owns one of these by default. It keeps the existing
// MeasureOverride/ArrangeOverride contract intact while routing layout through
// the per-element facet bag, so the host's layout state can later be relocated
// into the facet without touching the dispatch path.
class DefaultLayoutFacet : public LayoutFacet {
public:
    explicit DefaultLayoutFacet(::Aero::UIElement& owner) noexcept
        : LayoutFacet(owner) {}

    Size Measure(const Size& availableSize) noexcept override {
        auto* owner = GetOwner();
        return owner != nullptr ? MeasureOverride(*owner, availableSize) : Size{};
    }
    Size Arrange(const Size& finalSize) noexcept override {
        auto* owner = GetOwner();
        return owner != nullptr ? ArrangeOverride(*owner, finalSize) : Size{};
    }
};

// Subclassable layout facets (whitepaper 4.2). Each overrides Measure/Arrange
// to specialize layout math per panel. The default implementations below
// delegate to the owned element's existing MeasureOverride/ArrangeOverride so
// current behavior is preserved; the *LayoutCalculator kernels already present
// in LayoutFacet.hpp are the intended compute path for the final swap, when
// panels adopt their concrete layout facet instead of overriding the virtuals.

class StackLayoutFacet : public LayoutFacet {
public:
    using Orientation = StackLayoutCalculator::Orientation;

    explicit StackLayoutFacet(
        ::Aero::UIElement& owner,
        Orientation orientation = Orientation::Vertical) noexcept
        : LayoutFacet(owner), orientation_(orientation) {}

    Orientation GetOrientation() const noexcept { return orientation_; }
    void SetOrientation(Orientation value) noexcept { orientation_ = value; }

    Size Measure(const Size& availableSize) noexcept override {
        auto* owner = GetOwner();
        return owner != nullptr ? MeasureOverride(*owner, availableSize) : Size{};
    }
    Size Arrange(const Size& finalSize) noexcept override {
        auto* owner = GetOwner();
        return owner != nullptr ? ArrangeOverride(*owner, finalSize) : Size{};
    }

private:
    Orientation orientation_ = Orientation::Vertical;
};

class GridLayoutFacet : public LayoutFacet {
public:
    explicit GridLayoutFacet(::Aero::UIElement& owner) noexcept : LayoutFacet(owner) {}

    Size Measure(const Size& availableSize) noexcept override {
        auto* owner = GetOwner();
        return owner != nullptr ? MeasureOverride(*owner, availableSize) : Size{};
    }
    Size Arrange(const Size& finalSize) noexcept override {
        auto* owner = GetOwner();
        return owner != nullptr ? ArrangeOverride(*owner, finalSize) : Size{};
    }
};

class DockLayoutFacet : public LayoutFacet {
public:
    explicit DockLayoutFacet(::Aero::UIElement& owner) noexcept : LayoutFacet(owner) {}

    Size Measure(const Size& availableSize) noexcept override {
        auto* owner = GetOwner();
        return owner != nullptr ? MeasureOverride(*owner, availableSize) : Size{};
    }
    Size Arrange(const Size& finalSize) noexcept override {
        auto* owner = GetOwner();
        return owner != nullptr ? ArrangeOverride(*owner, finalSize) : Size{};
    }
};

class CanvasLayoutFacet : public LayoutFacet {
public:
    explicit CanvasLayoutFacet(::Aero::UIElement& owner) noexcept : LayoutFacet(owner) {}

    Size Measure(const Size& availableSize) noexcept override {
        auto* owner = GetOwner();
        return owner != nullptr ? MeasureOverride(*owner, availableSize) : Size{};
    }
    Size Arrange(const Size& finalSize) noexcept override {
        auto* owner = GetOwner();
        return owner != nullptr ? ArrangeOverride(*owner, finalSize) : Size{};
    }
};

class WrapLayoutFacet : public LayoutFacet {
public:
    explicit WrapLayoutFacet(::Aero::UIElement& owner) noexcept : LayoutFacet(owner) {}

    Size Measure(const Size& availableSize) noexcept override {
        auto* owner = GetOwner();
        return owner != nullptr ? MeasureOverride(*owner, availableSize) : Size{};
    }
    Size Arrange(const Size& finalSize) noexcept override {
        auto* owner = GetOwner();
        return owner != nullptr ? ArrangeOverride(*owner, finalSize) : Size{};
    }
};

} // namespace Aero::Core
