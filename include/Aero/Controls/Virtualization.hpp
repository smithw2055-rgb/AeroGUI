#pragma once

#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Scroll.hpp>

namespace Aero::Controls {

enum class ScrollUnit : std::uint8_t { Item = 0U, Pixel };
enum class VirtualizationMode : std::uint8_t { Standard = 0U, Recycling };

// WPF attached-property owner shared by all virtualizing panels. The current
// panel implementation is pixel-based; exposing this owner preserves the
// authored contract while item-unit realization is added.
class AERO_API VirtualizingPanel : public Base::Object {
    AERO_DECLARE_TYPE(VirtualizingPanel, Base::Object)
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    inline static constexpr Members::AttachedProperty<ScrollUnit>
        ScrollUnitProperty{"ScrollUnit"};
    inline static constexpr Members::AttachedProperty<VirtualizationMode>
        VirtualizationModeProperty{"VirtualizationMode"};
};

class AERO_API VirtualizingStackPanel final
    : public Panel,
      public IScrollInfo {
    AERO_DECLARE_TYPE(VirtualizingStackPanel, Panel)
public:
    VirtualizingStackPanel() noexcept;
    ~VirtualizingStackPanel() override;

    Orientation GetOrientation() const noexcept;
    Base::Result<void> SetOrientation(
        Orientation value) noexcept;
    std::uint32_t OverscanCount() const noexcept;
    Base::Result<void> SetOverscanCount(
        std::uint32_t value) noexcept;
    double EstimatedItemExtent() const noexcept;
    Base::Result<void> SetEstimatedItemExtent(
        double value) noexcept;

    std::uint32_t VisibleFirstIndex() const noexcept {
        return visibleFirstIndex_;
    }
    std::uint32_t VisibleCount() const noexcept {
        return visibleCount_;
    }
    std::uint32_t RealizedFirstIndex() const noexcept {
        return desiredFirstIndex_;
    }
    std::uint32_t RealizedCount() const noexcept {
        return desiredCount_;
    }
    double ItemExtent(
        std::uint32_t index) const noexcept;
    double ItemOffset(
        std::uint32_t index) const noexcept;

    ScrollData Data() const noexcept override {
        return data_;
    }
    Base::Result<bool> SetViewport(
        Size viewport) noexcept override;
    Base::Result<bool> SetHorizontalOffset(
        double value) noexcept override;
    Base::Result<bool> SetVerticalOffset(
        double value) noexcept override;
    Base::Result<bool> LineHorizontal(
        double direction) noexcept override;
    Base::Result<bool> LineVertical(
        double direction) noexcept override;
    Base::Result<bool> PageHorizontal(
        double direction) noexcept override;
    Base::Result<bool> PageVertical(
        double direction) noexcept override;

    inline static constexpr Members::Property<Orientation>
        OrientationProperty{"Orientation"};
    inline static constexpr Members::Property<std::uint32_t>
        OverscanCountProperty{"OverscanCount"};
    inline static constexpr Members::Property<double>
        EstimatedItemExtentProperty{"EstimatedItemExtent"};

protected:
    Base::Result<void> OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;

private:
    friend class ItemContainerGenerator;

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

    Base::Result<void> AttachGenerator(
        ItemContainerGenerator& generator,
        std::uint32_t itemCount) noexcept;
    void DetachGenerator(
        ItemContainerGenerator& generator) noexcept;
    Base::Result<void> OnItemsChanged(
        const ItemsChangedEvent& event,
        std::uint32_t itemCount) noexcept;
    Base::Result<void> ResizeExtentCache(
        std::uint32_t itemCount) noexcept;
    Base::Result<void> ApplyExtentDelta(
        const ItemsChangedEvent& event,
        std::uint32_t itemCount) noexcept;
    Base::Result<void> UpdateRealization(
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
    Base::Result<void> RebuildExtentTree() noexcept;
    void AddExtentDeviation(
        std::uint32_t index,
        double delta) noexcept;
    double PrefixDeviation(
        std::uint32_t count) const noexcept;
    void SetMeasuredExtent(
        std::uint32_t index,
        double value) noexcept;
    Base::Result<bool> SetMainScrollOffset(
        double value) noexcept;
    Base::Result<bool> SetCrossScrollOffset(
        double value) noexcept;
};

} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Controls::ScrollUnit> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("ScrollUnit");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "ScrollUnit";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Controls::VirtualizationMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("VirtualizationMode");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "VirtualizationMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core
