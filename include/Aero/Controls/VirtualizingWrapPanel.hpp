#pragma once

#include <Aero/Controls/IScrollInfo.hpp>
#include <Aero/Controls/VirtualizationCacheLength.hpp>
#include <Aero/Controls/VirtualizingPanel.hpp>

namespace Aero::Controls {

// Wrap virtualization shares the item generator on VirtualizingPanel.
// The stack extent tree stays on the stack panel.
class AERO_GUI_API VirtualizingWrapPanel
    : public VirtualizingPanel,
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

    ScrollData GetData() const noexcept override {
        return data_;
    }
    void SetViewport(Size viewport) noexcept override;
    void SetHorizontalOffset(double value) noexcept override;
    void SetVerticalOffset(double value) noexcept override;
    Result<bool> LineHorizontal(double direction) noexcept override;
    Result<bool> LineVertical(double direction) noexcept override;
    Result<bool> PageHorizontal(double direction) noexcept override;
    Result<bool> PageVertical(double direction) noexcept override;

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
