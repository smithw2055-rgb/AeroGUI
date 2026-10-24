#pragma once

#include <Aero/Value.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Controls/Scroll.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Events.hpp>
#include <cstdint>

namespace Aero::Controls {

enum class VirtualizationCacheLengthUnit : std::uint8_t {
    Pixel = 0U,
    Item,
    Page
};

struct VirtualizationCacheLength {
    double cacheBeforeViewport = 0.0;
    double cacheAfterViewport = 0.0;
};

using ::Aero::Meta::TypeId;

enum class ScrollUnit : std::uint8_t { Item = 0U, Pixel };

enum class VirtualizationMode : std::uint8_t { Standard = 0U, Recycling };

// WPF attached-property owner shared by all virtualizing panels. The current
// panel implementation is pixel-based; exposing this owner preserves the
// authored contract while item-unit realization is added.
class AERO_GUI_API VirtualizingPanel : public Panel {
    AERO_DECLARE_TYPE(VirtualizingPanel, Panel)

public:
    AERO_ATTACHED_PROPERTY(ScrollUnit, ScrollUnit);
    AERO_ATTACHED_PROPERTY(VirtualizationMode, VirtualizationMode);

protected:
    friend class ItemContainerGenerator;

    explicit VirtualizingPanel(TypeId runtimeType) noexcept : Panel(runtimeType) {}
    ~VirtualizingPanel() override = default;

    // Shared by stack and wrap virtualization. Stack keeps its own extent tree.
    virtual Result<void> AttachGenerator(ItemContainerGenerator& generator, std::uint32_t itemCount) noexcept;
    virtual void DetachGenerator(ItemContainerGenerator& generator) noexcept;
    virtual void HandleItemsChanged(const Collections::ItemsChangedEvent& event, std::uint32_t itemCount) noexcept;

    ItemContainerGenerator* generator_ = nullptr;
    Base::Vector<double> itemExtents_;
    ScrollData data_{};
    std::uint32_t visibleFirstIndex_ = 0U;
    std::uint32_t visibleCount_ = 0U;
    std::uint32_t desiredFirstIndex_ = 0U;
    std::uint32_t desiredCount_ = 0U;
};

class AERO_GUI_API VirtualizingStackPanel : public VirtualizingPanel,
      public IScrollInfo {
    AERO_DECLARE_TYPE(VirtualizingStackPanel, VirtualizingPanel)

public:
    VirtualizingStackPanel() noexcept;
    ~VirtualizingStackPanel() override;

    VirtualizationCacheLength GetCacheLength() const noexcept;
    void SetCacheLength(VirtualizationCacheLength value) noexcept;
    VirtualizationCacheLengthUnit GetCacheLengthUnit() const noexcept;
    void SetCacheLengthUnit(VirtualizationCacheLengthUnit value) noexcept;

    Orientation GetOrientation() const noexcept;
    void SetOrientation(Orientation value) noexcept;
    std::uint32_t GetOverscanCount() const noexcept;
    void SetOverscanCount(std::uint32_t value) noexcept;
    double GetEstimatedItemExtent() const noexcept;
    void SetEstimatedItemExtent(double value) noexcept;

    std::uint32_t GetVisibleFirstIndex() const noexcept { return visibleFirstIndex_; }
    std::uint32_t GetVisibleCount() const noexcept { return visibleCount_; }
    std::uint32_t GetRealizedFirstIndex() const noexcept { return desiredFirstIndex_; }
    std::uint32_t GetRealizedCount() const noexcept { return desiredCount_; }
    double GetItemExtent(std::uint32_t index) const noexcept;
    double GetItemOffset(std::uint32_t index) const noexcept;

    ScrollData GetData() const noexcept override { return data_; }
    void SetViewport(Size viewport) noexcept override;
    void SetHorizontalOffset(double value) noexcept override;
    void SetVerticalOffset(double value) noexcept override;
    bool LineHorizontal(double direction) noexcept override;
    bool LineVertical(double direction) noexcept override;
    bool PageHorizontal(double direction) noexcept override;
    bool PageVertical(double direction) noexcept override;

    AERO_DEPENDENCY_PROPERTY(Orientation, Orientation);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, OverscanCount);
    AERO_DEPENDENCY_PROPERTY(double, EstimatedItemExtent);
    AERO_DEPENDENCY_PROPERTY(VirtualizationCacheLength, CacheLength);
    AERO_DEPENDENCY_PROPERTY(VirtualizationCacheLengthUnit, CacheLengthUnit);

protected:
    explicit VirtualizingStackPanel(TypeId runtimeType) noexcept;
    void OnPropertyInvalidated(PropertyInvalidationFlags flags) noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    virtual void CalculateRealizationRange() noexcept;

    Result<void> AttachGenerator(ItemContainerGenerator& generator, std::uint32_t itemCount) noexcept override;
    void DetachGenerator(ItemContainerGenerator& generator) noexcept override;
    void HandleItemsChanged(const ItemsChangedEvent& event, std::uint32_t itemCount) noexcept override;

    void UpdateRealization(bool notifyGenerator) noexcept;
    double MainOffset() const noexcept;
    double MainViewport() const noexcept;
    double MainExtent() const noexcept;
    void SetMainExtent(double value) noexcept;
    std::uint32_t ItemIndexAtOffset(double offset) const noexcept;

    double estimatedItemExtent_ = 24.0;
    Orientation orientation_ = Orientation::Vertical;

private:
    Base::Vector<double> extentTree_;
    double crossExtent_ = 0.0;
    std::uint32_t overscanCount_ = 2U;

    void ResizeExtentCache(std::uint32_t itemCount) noexcept;
    void ApplyExtentDelta(const ItemsChangedEvent& event, std::uint32_t itemCount) noexcept;
    void SetMainOffset(double value) noexcept;
    void ClampOffsets() noexcept;
    double ExtentForIndex(std::uint32_t index) const noexcept;
    void RebuildExtentTree() noexcept;
    void AddExtentDeviation(std::uint32_t index, double delta) noexcept;
    double PrefixDeviation(std::uint32_t count) const noexcept;
    void SetMeasuredExtent(std::uint32_t index, double value) noexcept;
    void SetMainScrollOffset(double value) noexcept;
    void SetCrossScrollOffset(double value) noexcept;
};

// Wrap virtualization shares the item generator on VirtualizingPanel.
// The stack extent tree stays on the stack panel.
class AERO_GUI_API VirtualizingWrapPanel : public VirtualizingPanel,
      public IScrollInfo {
    AERO_DECLARE_TYPE(VirtualizingWrapPanel, VirtualizingPanel)

public:
    VirtualizingWrapPanel() noexcept;

    double GetItemWidth() const noexcept;
    double GetItemHeight() const noexcept;
    void SetItemWidth(double value) noexcept;
    void SetItemHeight(double value) noexcept;

    Orientation GetOrientation() const noexcept;
    void SetOrientation(Orientation value) noexcept;
    std::uint32_t GetOverscanCount() const noexcept;
    void SetOverscanCount(std::uint32_t value) noexcept;
    VirtualizationCacheLength GetCacheLength() const noexcept;
    void SetCacheLength(VirtualizationCacheLength value) noexcept;
    VirtualizationCacheLengthUnit GetCacheLengthUnit() const noexcept;
    void SetCacheLengthUnit(VirtualizationCacheLengthUnit value) noexcept;

    ScrollData GetData() const noexcept override { return data_; }
    void SetViewport(Size viewport) noexcept override;
    void SetHorizontalOffset(double value) noexcept override;
    void SetVerticalOffset(double value) noexcept override;
    bool LineHorizontal(double direction) noexcept override;
    bool LineVertical(double direction) noexcept override;
    bool PageHorizontal(double direction) noexcept override;
    bool PageVertical(double direction) noexcept override;

    AERO_DEPENDENCY_PROPERTY(double, ItemWidth);
    AERO_DEPENDENCY_PROPERTY(double, ItemHeight);
    AERO_DEPENDENCY_PROPERTY(Orientation, Orientation);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, OverscanCount);
    AERO_DEPENDENCY_PROPERTY(VirtualizationCacheLength, CacheLength);
    AERO_DEPENDENCY_PROPERTY(VirtualizationCacheLengthUnit, CacheLengthUnit);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    void CalculateRealizationRange() noexcept;

private:
    double MainExtent() const noexcept;
    void SetMainExtent(double value) noexcept;
    void SetMainOffset(double value) noexcept;
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::VirtualizationCacheLengthUnit)
AERO_DECLARE_TYPE_VALUE(::Aero::Controls::VirtualizationCacheLength, "VirtualizationCacheLength")
AERO_DECLARE_TYPE_ENUM(Aero::Controls::ScrollUnit)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::VirtualizationMode)
