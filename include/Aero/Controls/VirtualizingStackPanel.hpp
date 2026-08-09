#pragma once

#include <Aero/Controls/ItemsControl.hpp>
#include <Aero/Controls/ItemContainerGenerator.hpp>
#include <Aero/Controls/ScrollViewer.hpp>
#include <Aero/Controls/Panel.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
enum class ScrollUnit : std::uint8_t { Item = 0U, Pixel };

enum class VirtualizationMode : std::uint8_t { Standard = 0U, Recycling };

// WPF attached-property owner shared by all virtualizing panels. The current
// panel implementation is pixel-based; exposing this owner preserves the
// authored contract while item-unit realization is added.
class AERO_GUI_API VirtualizingPanel : public Base::Object {
    AERO_DECLARE_TYPE(VirtualizingPanel, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    inline static constexpr AttachedProperty<ScrollUnit> ScrollUnitProperty{"ScrollUnit"};
    inline static constexpr AttachedProperty<VirtualizationMode> VirtualizationModeProperty{"VirtualizationMode"};
};

class AERO_GUI_API VirtualizingStackPanel
    : public Panel,
      public IScrollInfo {
    AERO_DECLARE_TYPE(VirtualizingStackPanel, Panel)
public:
    VirtualizingStackPanel() noexcept;
    ~VirtualizingStackPanel() override;

    Orientation GetOrientation() const noexcept;
    void SetOrientation(
        Orientation value) noexcept;
    std::uint32_t GetOverscanCount() const noexcept;
    void SetOverscanCount(
        std::uint32_t value) noexcept;
    double GetEstimatedItemExtent() const noexcept;
    void SetEstimatedItemExtent(
        double value) noexcept;

    std::uint32_t GetVisibleFirstIndex() const noexcept {
        return visibleFirstIndex_;
    }
    std::uint32_t GetVisibleCount() const noexcept {
        return visibleCount_;
    }
    std::uint32_t GetRealizedFirstIndex() const noexcept {
        return desiredFirstIndex_;
    }
    std::uint32_t GetRealizedCount() const noexcept {
        return desiredCount_;
    }
    double GetItemExtent(
        std::uint32_t index) const noexcept;
    double GetItemOffset(
        std::uint32_t index) const noexcept;

    ScrollData GetData() const noexcept override {
        return data_;
    }
    void SetViewport(
        Size viewport) noexcept override;
    void SetHorizontalOffset(
        double value) noexcept override;
    void SetVerticalOffset(
        double value) noexcept override;
    Result<bool> LineHorizontal(
        double direction) noexcept override;
    Result<bool> LineVertical(
        double direction) noexcept override;
    Result<bool> PageHorizontal(
        double direction) noexcept override;
    Result<bool> PageVertical(
        double direction) noexcept override;

    inline static constexpr DependencyProperty<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr DependencyProperty<std::uint32_t> OverscanCountProperty{"OverscanCount"};
    inline static constexpr DependencyProperty<double> EstimatedItemExtentProperty{"EstimatedItemExtent"};

protected:
    void OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    friend class ItemContainerGenerator;
#if defined(AERO_GUI_IMPLEMENTATION)
    friend struct ::Aero::Media::Visual::Access;
#endif

    ItemContainerGenerator* generator_ = nullptr;
    Base::Vector<double> itemExtents_;
    Base::Vector<double> extentTree_;
    ScrollData data_;
    double crossExtent_ = 0.0;
    double estimatedItemExtent_ = 24.0;
    std::uint32_t overscanCount_ = 2U;
    Orientation orientation_ = Orientation::Vertical;
    std::uint32_t visibleFirstIndex_ = 0U;
    std::uint32_t visibleCount_ = 0U;
    std::uint32_t desiredFirstIndex_ = 0U;
    std::uint32_t desiredCount_ = 0U;

    Result<void> AttachGenerator(
        ItemContainerGenerator& generator,
        std::uint32_t itemCount) noexcept;
    void DetachGenerator(
        ItemContainerGenerator& generator) noexcept;
    Result<void> HandleItemsChanged(
        const ItemsChangedEvent& event,
        std::uint32_t itemCount) noexcept;
    Result<void> ResizeExtentCache(
        std::uint32_t itemCount) noexcept;
    Result<void> ApplyExtentDelta(
        const ItemsChangedEvent& event,
        std::uint32_t itemCount) noexcept;
    Result<void> UpdateRealization(
        bool notifyGenerator) noexcept;
    void CalculateRealizationRange() noexcept;
    std::uint32_t ItemIndexAtOffset(
        double offset) const noexcept;
    double MainOffset() const noexcept;
    double MainViewport() const noexcept;
    double MainExtent() const noexcept;
    void SetMainOffset(double value) noexcept;
    void SetMainExtent(double value) noexcept;
    void ClampOffsets() noexcept;
    double ExtentForIndex(
        std::uint32_t index) const noexcept;
    Result<void> RebuildExtentTree() noexcept;
    void AddExtentDeviation(
        std::uint32_t index,
        double delta) noexcept;
    double PrefixDeviation(
        std::uint32_t count) const noexcept;
    void SetMeasuredExtent(
        std::uint32_t index,
        double value) noexcept;
    void SetMainScrollOffset(
        double value) noexcept;
    void SetCrossScrollOffset(
        double value) noexcept;
};
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::ScrollUnit)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::VirtualizationMode)
