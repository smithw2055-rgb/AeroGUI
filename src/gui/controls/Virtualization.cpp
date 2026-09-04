#include <Aero/Controls.hpp>

#include <algorithm>
#include <cmath>

namespace Aero::Controls {
namespace {

constexpr double LayoutInfinity = 1.0e12;
constexpr double ComparisonEpsilon = 0.000001;
constexpr double CrossLineExtent = 16.0;

bool Same(double left, double right) noexcept {
    return std::fabs(left - right) <=
        ComparisonEpsilon;
}

bool ValidNonnegative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

bool ValidViewport(Size value) noexcept {
    return IsFinite(value) &&
        value.width >= 0.0 &&
        value.height >= 0.0;
}

} // namespace

VirtualizingStackPanel::VirtualizingStackPanel() noexcept
    : VirtualizingStackPanel(StaticTypeId()) {}

VirtualizingStackPanel::VirtualizingStackPanel(TypeId runtimeType) noexcept
    : VirtualizingPanel(runtimeType) {
    static_cast<void>(SetClipToBounds(true));
}

VirtualizingStackPanel::~VirtualizingStackPanel() {
    if (generator_ != nullptr) {
        static_cast<void>(generator_->Detach());
    }
}

Orientation
VirtualizingStackPanel::GetOrientation() const noexcept {
    return orientation_;
}

void VirtualizingStackPanel::SetOrientation(
    Orientation value) noexcept {
    if (value == GetOrientation()) return;
    const double oldMainOffset = MainOffset();
    SetValue(OrientationProperty, value);
    orientation_ = value;
    data_.horizontalOffset = 0.0;
    data_.verticalOffset = 0.0;
    SetMainOffset(oldMainOffset);
    crossExtent_ = 0.0;
    SetMainExtent(GetItemOffset(itemExtents_.Size()));
    ClampOffsets();
    (void)UpdateRealization(true);
}

std::uint32_t
VirtualizingStackPanel::GetOverscanCount() const noexcept {
    return overscanCount_;
}

void VirtualizingStackPanel::SetOverscanCount(
    std::uint32_t value) noexcept {
    if (value == GetOverscanCount()) return;
    SetValue(OverscanCountProperty, value);
    overscanCount_ = value;
    (void)UpdateRealization(true);
}

double
VirtualizingStackPanel::GetEstimatedItemExtent() const noexcept {
    return estimatedItemExtent_;
}

void VirtualizingStackPanel::SetEstimatedItemExtent(
    double value) noexcept {
    if (!ValidNonnegative(value) || Same(value, GetEstimatedItemExtent())) return;
    const std::uint32_t anchor =
        ItemIndexAtOffset(MainOffset());
    const double intraItem =
        MainOffset() - GetItemOffset(anchor);
    SetValue(EstimatedItemExtentProperty, value);
    estimatedItemExtent_ = value;
    (void)RebuildExtentTree();
    SetMainExtent(GetItemOffset(itemExtents_.Size()));
    SetMainOffset(
        GetItemOffset(anchor) + intraItem);
    ClampOffsets();
    (void)UpdateRealization(true);
}

VirtualizationCacheLength
VirtualizingStackPanel::GetCacheLength() const noexcept {
    return GetValue(CacheLengthProperty);
}

void VirtualizingStackPanel::SetCacheLength(
    VirtualizationCacheLength value) noexcept {
    SetValue(CacheLengthProperty, value);
    (void)UpdateRealization(true);
}

VirtualizationCacheLengthUnit
VirtualizingStackPanel::GetCacheLengthUnit() const noexcept {
    return GetValue(CacheLengthUnitProperty);
}

void VirtualizingStackPanel::SetCacheLengthUnit(
    VirtualizationCacheLengthUnit value) noexcept {
    SetValue(CacheLengthUnitProperty, value);
    (void)UpdateRealization(true);
}

double VirtualizingStackPanel::ExtentForIndex(
    std::uint32_t index) const noexcept {
    return index < itemExtents_.Size() &&
        itemExtents_[index] > 0.0
        ? itemExtents_[index]
        : GetEstimatedItemExtent();
}

double VirtualizingStackPanel::GetItemExtent(
    std::uint32_t index) const noexcept {
    return index < itemExtents_.Size()
        ? ExtentForIndex(index)
        : 0.0;
}

double VirtualizingStackPanel::GetItemOffset(
    std::uint32_t index) const noexcept {
    const std::uint32_t end =
        std::min(index, itemExtents_.Size());
    return static_cast<double>(end) *
        estimatedItemExtent_ +
        PrefixDeviation(end);
}

void VirtualizingStackPanel::AddExtentDeviation(
    std::uint32_t index,
    double delta) noexcept {
    std::uint32_t treeIndex = index + 1U;
    while (treeIndex < extentTree_.Size()) {
        extentTree_[treeIndex] += delta;
        treeIndex += treeIndex &
            (~treeIndex + 1U);
    }
}

double VirtualizingStackPanel::PrefixDeviation(
    std::uint32_t count) const noexcept {
    double result = 0.0;
    std::uint32_t treeIndex =
        std::min(count, itemExtents_.Size());
    while (treeIndex > 0U &&
        treeIndex < extentTree_.Size()) {
        result += extentTree_[treeIndex];
        treeIndex -= treeIndex &
            (~treeIndex + 1U);
    }
    return result;
}

Base::Result<void>
VirtualizingStackPanel::RebuildExtentTree() noexcept {
    Base::Result<void> resized =
        extentTree_.Resize(
            itemExtents_.Size() + 1U, 0.0);
    if (!resized) return resized.GetStatus();
    for (double& value : extentTree_) {
        value = 0.0;
    }
    for (std::uint32_t index = 0U;
        index < itemExtents_.Size(); ++index) {
        if (itemExtents_[index] > 0.0) {
            AddExtentDeviation(
                index,
                itemExtents_[index] -
                    estimatedItemExtent_);
        }
    }
    return {};
}

void VirtualizingStackPanel::SetMeasuredExtent(
    std::uint32_t index,
    double value) noexcept {
    if (index >= itemExtents_.Size()) return;
    const double oldDeviation =
        itemExtents_[index] > 0.0
        ? itemExtents_[index] -
            estimatedItemExtent_
        : 0.0;
    const double newDeviation =
        value - estimatedItemExtent_;
    itemExtents_[index] = value;
    AddExtentDeviation(
        index,
        newDeviation - oldDeviation);
}

double VirtualizingStackPanel::MainOffset() const noexcept {
    return GetOrientation() == Orientation::Vertical
        ? data_.verticalOffset
        : data_.horizontalOffset;
}

double VirtualizingStackPanel::MainViewport() const noexcept {
    return GetOrientation() == Orientation::Vertical
        ? data_.viewportHeight
        : data_.viewportWidth;
}

double VirtualizingStackPanel::MainExtent() const noexcept {
    return GetOrientation() == Orientation::Vertical
        ? data_.extentHeight
        : data_.extentWidth;
}

void VirtualizingStackPanel::SetMainOffset(
    double value) noexcept {
    if (GetOrientation() == Orientation::Vertical) {
        data_.verticalOffset = value;
    } else {
        data_.horizontalOffset = value;
    }
}

void VirtualizingStackPanel::SetMainExtent(
    double value) noexcept {
    if (GetOrientation() == Orientation::Vertical) {
        data_.extentHeight = value;
    } else {
        data_.extentWidth = value;
    }
}

void VirtualizingStackPanel::ClampOffsets() noexcept {
    data_.horizontalOffset = std::clamp(
        data_.horizontalOffset,
        0.0,
        std::max(
            0.0,
            data_.extentWidth -
                data_.viewportWidth));
    data_.verticalOffset = std::clamp(
        data_.verticalOffset,
        0.0,
        std::max(
            0.0,
            data_.extentHeight -
                data_.viewportHeight));
}

std::uint32_t
VirtualizingStackPanel::ItemIndexAtOffset(
    double offset) const noexcept {
    if (itemExtents_.Empty()) return 0U;
    const double target = std::max(0.0, offset);
    std::uint32_t low = 0U;
    std::uint32_t high = itemExtents_.Size();
    while (low < high) {
        const std::uint32_t middle =
            low + (high - low) / 2U;
        if (target < GetItemOffset(middle + 1U)) {
            high = middle;
        } else {
            low = middle + 1U;
        }
    }
    return std::min(
        low, itemExtents_.Size() - 1U);
}

void
VirtualizingStackPanel::CalculateRealizationRange() noexcept {
    const std::uint32_t itemCount =
        itemExtents_.Size();
    if (itemCount == 0U) {
        visibleFirstIndex_ = 0U;
        visibleCount_ = 0U;
        desiredFirstIndex_ = 0U;
        desiredCount_ = 0U;
        return;
    }
    visibleFirstIndex_ =
        ItemIndexAtOffset(MainOffset());
    const double viewport =
        std::max(0.0, MainViewport());
    std::uint32_t visibleEnd =
        visibleFirstIndex_ + 1U;
    if (viewport > 0.0) {
        const double viewportEnd =
            MainOffset() + viewport;
        visibleEnd = std::min(
            itemCount,
            ItemIndexAtOffset(std::max(
                MainOffset(),
                viewportEnd -
                    ComparisonEpsilon)) + 1U);
    }
    visibleCount_ =
        visibleEnd - visibleFirstIndex_;
    const VirtualizationCacheLength cache = GetCacheLength();
    const VirtualizationCacheLengthUnit unit = GetCacheLengthUnit();
    std::uint32_t before = 0U;
    std::uint32_t after = 0U;
    if (cache.cacheBeforeViewport > 0.0 ||
        cache.cacheAfterViewport > 0.0) {
        const double beforeValue = std::max(0.0, cache.cacheBeforeViewport);
        const double afterValue = std::max(0.0, cache.cacheAfterViewport);
        if (unit == VirtualizationCacheLengthUnit::Pixel) {
            const double firstOffset = GetItemOffset(visibleFirstIndex_);
            const double lastOffset = visibleEnd < itemCount
                ? GetItemOffset(visibleEnd) : MainExtent();
            before = visibleFirstIndex_ -
                ItemIndexAtOffset(std::max(0.0, firstOffset - beforeValue));
            const std::uint32_t afterIndex = ItemIndexAtOffset(
                lastOffset + afterValue);
            after = afterIndex >= visibleEnd
                ? afterIndex - visibleEnd + (afterIndex + 1U < itemCount ? 1U : 0U)
                : 0U;
        } else if (unit == VirtualizationCacheLengthUnit::Page) {
            const std::uint32_t page = std::max(1U, visibleCount_);
            before = static_cast<std::uint32_t>(beforeValue * page);
            after = static_cast<std::uint32_t>(afterValue * page);
        } else {
            before = static_cast<std::uint32_t>(beforeValue);
            after = static_cast<std::uint32_t>(afterValue);
        }
    } else {
        before = GetOverscanCount();
        after = GetOverscanCount();
    }
    desiredFirstIndex_ =
        visibleFirstIndex_ > before
        ? visibleFirstIndex_ - before
        : 0U;
    const std::uint32_t desiredEnd =
        std::min(
            itemCount,
            visibleEnd +
                std::min(
                    after,
                    itemCount - visibleEnd));
    desiredCount_ =
        desiredEnd - desiredFirstIndex_;
}

Base::Result<void>
VirtualizingStackPanel::UpdateRealization(
    bool notifyGenerator) noexcept {
    CalculateRealizationRange();
    if (notifyGenerator &&
        generator_ != nullptr) {
        generator_->SetRealizationRange(
            desiredFirstIndex_, desiredCount_);
    }
    return {};
}

Base::Result<void>
VirtualizingStackPanel::ResizeExtentCache(
    std::uint32_t itemCount) noexcept {
    return itemExtents_.Resize(
        itemCount, 0.0);
}

void
VirtualizingStackPanel::OnPropertyInvalidated(
    PropertyInvalidationFlags flags) noexcept {
    const double oldEstimate =
        estimatedItemExtent_;
    orientation_ = GetValue(OrientationProperty);
    overscanCount_ = GetValue(OverscanCountProperty);
    estimatedItemExtent_ = GetValue(EstimatedItemExtentProperty);
    if (!Same(
            oldEstimate,
            estimatedItemExtent_)) {
        Base::Result<void> rebuilt = RebuildExtentTree();
        if (!rebuilt) return;
        SetMainExtent(
            GetItemOffset(itemExtents_.Size()));
        ClampOffsets();
    }
    CalculateRealizationRange();
    Panel::OnPropertyInvalidated(flags);
}

Base::Result<void>
VirtualizingStackPanel::ApplyExtentDelta(
    const ItemsChangedEvent& event,
    std::uint32_t itemCount) noexcept {
    if (event.action == ItemsChangeAction::Reset) {
        itemExtents_.Clear();
        return ResizeExtentCache(itemCount);
    }
    if (event.action == ItemsChangeAction::Add &&
        event.newCount > 0U &&
        event.newIndex <= itemExtents_.Size() &&
        itemCount >= itemExtents_.Size() &&
        event.newCount ==
            itemCount - itemExtents_.Size()) {
        const std::uint32_t oldSize =
            itemExtents_.Size();
        Base::Result<void> resized =
            itemExtents_.Resize(
                oldSize + event.newCount, 0.0);
        if (!resized) return resized.GetStatus();
        for (std::uint32_t index = oldSize;
            index > event.newIndex; --index) {
            itemExtents_[
                index + event.newCount - 1U] =
                itemExtents_[index - 1U];
        }
        for (std::uint32_t offset = 0U;
            offset < event.newCount; ++offset) {
            itemExtents_[event.newIndex + offset] =
                0.0;
        }
        return {};
    }
    if (event.action == ItemsChangeAction::Remove &&
        event.oldCount > 0U &&
        event.oldIndex <= itemExtents_.Size() &&
        event.oldCount <=
            itemExtents_.Size() - event.oldIndex &&
        event.oldCount <= itemExtents_.Size() &&
        itemCount ==
            itemExtents_.Size() - event.oldCount) {
        for (std::uint32_t index = event.oldIndex;
            index + event.oldCount <
                itemExtents_.Size(); ++index) {
            itemExtents_[index] =
                itemExtents_[
                    index + event.oldCount];
        }
        return itemExtents_.Resize(itemCount);
    }
    if (event.action == ItemsChangeAction::Replace &&
        event.oldCount == event.newCount &&
        event.newIndex <= itemExtents_.Size() &&
        event.newCount <=
            itemExtents_.Size() - event.newIndex) {
        for (std::uint32_t offset = 0U;
            offset < event.newCount; ++offset) {
            itemExtents_[event.newIndex + offset] =
                0.0;
        }
        return {};
    }
    if (event.action == ItemsChangeAction::Move &&
        event.oldCount == 1U &&
        event.newCount == 1U &&
        event.oldIndex < itemExtents_.Size() &&
        event.newIndex < itemExtents_.Size()) {
        const double moving =
            itemExtents_[event.oldIndex];
        if (event.oldIndex < event.newIndex) {
            for (std::uint32_t index =
                    event.oldIndex;
                index < event.newIndex; ++index) {
                itemExtents_[index] =
                    itemExtents_[index + 1U];
            }
        } else {
            for (std::uint32_t index =
                    event.oldIndex;
                index > event.newIndex; --index) {
                itemExtents_[index] =
                    itemExtents_[index - 1U];
            }
        }
        itemExtents_[event.newIndex] = moving;
        return {};
    }
    itemExtents_.Clear();
    return ResizeExtentCache(itemCount);
}

Base::Result<void>
VirtualizingStackPanel::HandleItemsChanged(
    const ItemsChangedEvent& event,
    std::uint32_t itemCount) noexcept {
    const std::uint32_t oldCount =
        itemExtents_.Size();
    std::uint32_t anchor = oldCount > 0U
        ? ItemIndexAtOffset(MainOffset())
        : 0U;
    const double intraItem = oldCount > 0U
        ? MainOffset() - GetItemOffset(anchor)
        : 0.0;

    if (event.action == ItemsChangeAction::Add &&
        event.newIndex <= anchor) {
        anchor = event.newCount <=
                UINT32_MAX - anchor
            ? anchor + event.newCount
            : itemCount;
    } else if (
        event.action == ItemsChangeAction::Remove) {
        if (anchor >= event.oldIndex &&
            anchor - event.oldIndex >=
                event.oldCount) {
            anchor -= event.oldCount;
        } else if (anchor >= event.oldIndex) {
            anchor = event.oldIndex;
        }
    } else if (
        event.action == ItemsChangeAction::Move &&
        event.oldCount == 1U &&
        event.newCount == 1U) {
        if (anchor == event.oldIndex) {
            anchor = event.newIndex;
        } else if (
            event.oldIndex < anchor &&
            anchor <= event.newIndex) {
            --anchor;
        } else if (
            event.newIndex <= anchor &&
            anchor < event.oldIndex) {
            ++anchor;
        }
    }

    Base::Result<void> changed =
        ApplyExtentDelta(event, itemCount);
    if (!changed) return changed.GetStatus();
    Base::Result<void> rebuilt =
        RebuildExtentTree();
    if (!rebuilt) return rebuilt.GetStatus();
    if (itemCount > 0U) {
        anchor = std::min(
            anchor, itemCount - 1U);
        SetMainOffset(
            GetItemOffset(anchor) + intraItem);
    } else {
        SetMainOffset(0.0);
    }
    SetMainExtent(GetItemOffset(itemCount));
    ClampOffsets();
    CalculateRealizationRange();
    Base::Result<void> invalidated =
        InvalidateMeasure();
    if (!invalidated) {
        return invalidated.GetStatus();
    }
    return InvalidateArrange();
}

Base::Result<void>
VirtualizingStackPanel::AttachGenerator(
    ItemContainerGenerator& generator,
    std::uint32_t itemCount) noexcept {
    if (generator_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "VirtualizingStackPanel already has a generator");
    }
    generator_ = &generator;
    itemExtents_.Clear();
    Base::Result<void> resized =
        ResizeExtentCache(itemCount);
    if (!resized) {
        generator_ = nullptr;
        return resized.GetStatus();
    }
    Base::Result<void> rebuilt =
        RebuildExtentTree();
    if (!rebuilt) {
        itemExtents_.Clear();
        generator_ = nullptr;
        return rebuilt.GetStatus();
    }
    SetMainExtent(GetItemOffset(itemCount));
    ClampOffsets();
    CalculateRealizationRange();
    return {};
}

void VirtualizingStackPanel::DetachGenerator(
    ItemContainerGenerator& generator) noexcept {
    if (generator_ != &generator) return;
    generator_ = nullptr;
    itemExtents_.Clear();
    extentTree_.Clear();
    visibleFirstIndex_ = 0U;
    visibleCount_ = 0U;
    desiredFirstIndex_ = 0U;
    desiredCount_ = 0U;
    data_.extentWidth = 0.0;
    data_.extentHeight = 0.0;
    data_.horizontalOffset = 0.0;
    data_.verticalOffset = 0.0;
}

void VirtualizingStackPanel::SetViewport(
    Size viewport) noexcept {
    if (!ValidViewport(viewport)) {
        return;
    }
    const bool changed =
        !Same(data_.viewportWidth, viewport.width) ||
        !Same(data_.viewportHeight, viewport.height);
    if (!changed) return;
    data_.viewportWidth = viewport.width;
    data_.viewportHeight = viewport.height;
    ClampOffsets();
    (void)UpdateRealization(true);
    (void)InvalidateMeasure();
    (void)InvalidateArrange();
}

void VirtualizingStackPanel::SetMainScrollOffset(
    double value) noexcept {
    const double next = std::clamp(
        value,
        0.0,
        std::max(
            0.0,
            MainExtent() - MainViewport()));
    if (Same(next, MainOffset())) return;
    SetMainOffset(next);
    (void)UpdateRealization(true);
    (void)InvalidateMeasure();
    (void)InvalidateArrange();
}

void VirtualizingStackPanel::SetCrossScrollOffset(
    double value) noexcept {
    const bool vertical =
        GetOrientation() == Orientation::Vertical;
    const double extent = vertical
        ? data_.extentWidth
        : data_.extentHeight;
    const double viewport = vertical
        ? data_.viewportWidth
        : data_.viewportHeight;
    const double next = std::clamp(
        value,
        0.0,
        std::max(0.0, extent - viewport));
    double& current = vertical
        ? data_.horizontalOffset
        : data_.verticalOffset;
    if (Same(next, current)) return;
    current = next;
    (void)InvalidateArrange();
}

void VirtualizingStackPanel::SetHorizontalOffset(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return;
    }
    if (GetOrientation() == Orientation::Horizontal) {
        SetMainScrollOffset(value);
    } else {
        SetCrossScrollOffset(value);
    }
}

void VirtualizingStackPanel::SetVerticalOffset(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return;
    }
    if (GetOrientation() == Orientation::Vertical) {
        SetMainScrollOffset(value);
    } else {
        SetCrossScrollOffset(value);
    }
}

Base::Result<bool>
VirtualizingStackPanel::LineHorizontal(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Horizontal line direction must be finite");
    }
    const double old = data_.horizontalOffset;
    SetHorizontalOffset(std::max(0.0, old + direction *
        (GetOrientation() == Orientation::Horizontal
            ? GetEstimatedItemExtent() : CrossLineExtent)));
    return !Same(old, data_.horizontalOffset);
}

Base::Result<bool>
VirtualizingStackPanel::LineVertical(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Vertical line direction must be finite");
    }
    const double old = data_.verticalOffset;
    SetVerticalOffset(std::max(0.0, old + direction *
        (GetOrientation() == Orientation::Vertical
            ? GetEstimatedItemExtent() : CrossLineExtent)));
    return !Same(old, data_.verticalOffset);
}

Base::Result<bool>
VirtualizingStackPanel::PageHorizontal(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Horizontal page direction must be finite");
    }
    const double old = data_.horizontalOffset;
    SetHorizontalOffset(std::max(0.0, old + direction * data_.viewportWidth));
    return !Same(old, data_.horizontalOffset);
}

Base::Result<bool>
VirtualizingStackPanel::PageVertical(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Vertical page direction must be finite");
    }
    const double old = data_.verticalOffset;
    SetVerticalOffset(std::max(0.0, old + direction * data_.viewportHeight));
    return !Same(old, data_.verticalOffset);
}

Size
VirtualizingStackPanel::MeasureOverride(
    Size availableSize) noexcept {
    if (!ValidViewport(availableSize)) {
        return Size{};
    }
    const std::uint32_t anchor =
        itemExtents_.Empty()
        ? 0U
        : ItemIndexAtOffset(MainOffset());
    const double intraItem =
        itemExtents_.Empty()
        ? 0.0
        : MainOffset() - GetItemOffset(anchor);
    const Orientation orientation =
        GetOrientation();
    double crossExtent = 0.0;
    std::uint32_t localIndex = 0U;
    const std::uint32_t first =
        generator_ != nullptr
        ? generator_->GetFirstGeneratedIndex()
        : desiredFirstIndex_;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Size childAvailable = availableSize;
        if (orientation == Orientation::Vertical) {
            childAvailable.height = LayoutInfinity;
        } else {
            childAvailable.width = LayoutInfinity;
        }
        Base::Result<void> measured =
            MeasureChild(*child, childAvailable);
        if (!measured) return Size{};
        const Size desired = child->GetDesiredSize();
        const double extent =
            orientation == Orientation::Vertical
            ? desired.height
            : desired.width;
        const std::uint32_t itemIndex =
            first + localIndex;
        if (itemIndex < itemExtents_.Size() &&
            std::isfinite(extent) &&
            extent > 0.0) {
            SetMeasuredExtent(
                itemIndex, extent);
        }
        crossExtent = std::max(
            crossExtent,
            orientation == Orientation::Vertical
                ? desired.width
                : desired.height);
        ++localIndex;
    }
    crossExtent_ = crossExtent;
    SetMainExtent(GetItemOffset(itemExtents_.Size()));
    if (orientation == Orientation::Vertical) {
        data_.extentWidth = std::max(
            crossExtent_, availableSize.width);
    } else {
        data_.extentHeight = std::max(
            crossExtent_, availableSize.height);
    }
    if (!itemExtents_.Empty()) {
        SetMainOffset(
            GetItemOffset(anchor) + intraItem);
    }
    ClampOffsets();
    Base::Result<void> realized =
        UpdateRealization(true);
    if (!realized) return Size{};
    return orientation == Orientation::Vertical
        ? Size{crossExtent_, MainExtent()}
        : Size{MainExtent(), crossExtent_};
}

Size
VirtualizingStackPanel::ArrangeOverride(
    Size finalSize) noexcept {
    SetViewport(finalSize);
    const Orientation orientation =
        GetOrientation();
    const double mainOffset = MainOffset();
    const double crossOffset =
        orientation == Orientation::Vertical
        ? data_.horizontalOffset
        : data_.verticalOffset;
    std::uint32_t localIndex = 0U;
    const std::uint32_t first =
        generator_ != nullptr
        ? generator_->GetFirstGeneratedIndex()
        : desiredFirstIndex_;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const std::uint32_t itemIndex =
            first + localIndex;
        const double extent =
            ExtentForIndex(itemIndex);
        const double offset =
            GetItemOffset(itemIndex) - mainOffset;
        const Rect slot =
            orientation == Orientation::Vertical
            ? Rect{
                -crossOffset,
                offset,
                std::max(
                    finalSize.width,
                    crossExtent_),
                extent}
            : Rect{
                offset,
                -crossOffset,
                extent,
                std::max(
                    finalSize.height,
                    crossExtent_)};
        Base::Result<void> arranged =
            ArrangeChild(*child, slot);
        if (!arranged) return finalSize;
        ++localIndex;
    }
    return finalSize;
}

namespace {

constexpr double WrapItemFallback = 24.0;

double WrapSlotWidth(const VirtualizingWrapPanel& panel, Size available) noexcept {
    const double configured = panel.GetItemWidth();
    if (configured > 0.0) return configured;
    if (std::isfinite(available.width) && available.width > 0.0 &&
        available.width < 1.0e11) {
        return available.width;
    }
    return WrapItemFallback;
}

double WrapSlotHeight(const VirtualizingWrapPanel& panel, Size available) noexcept {
    const double configured = panel.GetItemHeight();
    if (configured > 0.0) return configured;
    if (panel.GetEstimatedItemExtent() > 0.0) {
        return panel.GetEstimatedItemExtent();
    }
    if (std::isfinite(available.height) && available.height > 0.0 &&
        available.height < 1.0e11) {
        return available.height;
    }
    return WrapItemFallback;
}

} // namespace

VirtualizingWrapPanel::VirtualizingWrapPanel() noexcept
    : VirtualizingStackPanel(StaticTypeId()) {}

double VirtualizingWrapPanel::GetItemWidth() const noexcept {
    return GetValue(ItemWidthProperty);
}

double VirtualizingWrapPanel::GetItemHeight() const noexcept {
    return GetValue(ItemHeightProperty);
}

void VirtualizingWrapPanel::SetItemWidth(double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) return;
    SetValue(ItemWidthProperty, value);
}

void VirtualizingWrapPanel::SetItemHeight(double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) return;
    SetValue(ItemHeightProperty, value);
}

void VirtualizingWrapPanel::CalculateRealizationRange() noexcept {
    const std::uint32_t itemCount = itemExtents_.Size();
    if (itemCount == 0U) {
        visibleFirstIndex_ = 0U;
        visibleCount_ = 0U;
        desiredFirstIndex_ = 0U;
        desiredCount_ = 0U;
        return;
    }
    const Size viewport{
        data_.viewportWidth, data_.viewportHeight};
    const double slotWidth = WrapSlotWidth(*this, viewport);
    const double slotHeight = WrapSlotHeight(*this, viewport);
    const std::uint32_t columns = std::max(
        1U,
        slotWidth > 0.0 && std::isfinite(viewport.width) && viewport.width > 0.0
            ? static_cast<std::uint32_t>(std::max(1.0, viewport.width / slotWidth))
            : 1U);
    const double offset = std::max(0.0, data_.verticalOffset);
    const std::uint32_t firstRow =
        slotHeight > 0.0 ? static_cast<std::uint32_t>(offset / slotHeight) : 0U;
    const std::uint32_t visibleRows = std::max(
        1U,
        slotHeight > 0.0 && std::isfinite(viewport.height) && viewport.height > 0.0
            ? static_cast<std::uint32_t>(
                  std::ceil(viewport.height / slotHeight))
            : 1U);
    visibleFirstIndex_ = std::min(itemCount, firstRow * columns);
    const std::uint32_t visibleEnd = std::min(
        itemCount, visibleFirstIndex_ + visibleRows * columns);
    visibleCount_ = visibleEnd - visibleFirstIndex_;
    const VirtualizationCacheLength cache = GetCacheLength();
    std::uint32_t before = 0U;
    std::uint32_t after = 0U;
    if (cache.cacheBeforeViewport > 0.0 || cache.cacheAfterViewport > 0.0) {
        if (GetCacheLengthUnit() == VirtualizationCacheLengthUnit::Pixel) {
            before = static_cast<std::uint32_t>(
                std::ceil(cache.cacheBeforeViewport / std::max(slotHeight, 1.0))) *
                columns;
            after = static_cast<std::uint32_t>(
                std::ceil(cache.cacheAfterViewport / std::max(slotHeight, 1.0))) *
                columns;
        } else if (GetCacheLengthUnit() == VirtualizationCacheLengthUnit::Page) {
            before = static_cast<std::uint32_t>(
                cache.cacheBeforeViewport * visibleCount_);
            after = static_cast<std::uint32_t>(
                cache.cacheAfterViewport * visibleCount_);
        } else {
            before = static_cast<std::uint32_t>(cache.cacheBeforeViewport);
            after = static_cast<std::uint32_t>(cache.cacheAfterViewport);
        }
    } else {
        before = GetOverscanCount() * columns;
        after = GetOverscanCount() * columns;
    }
    desiredFirstIndex_ = visibleFirstIndex_ > before
        ? visibleFirstIndex_ - before : 0U;
    const std::uint32_t desiredEnd = std::min(
        itemCount, visibleEnd + after);
    desiredCount_ = desiredEnd - desiredFirstIndex_;
    const std::uint32_t rows =
        (itemCount + columns - 1U) / columns;
    SetMainExtent(static_cast<double>(rows) * slotHeight);
}

Size VirtualizingWrapPanel::MeasureOverride(Size availableSize) noexcept {
    CalculateRealizationRange();
    if (generator_ != nullptr) {
        generator_->SetRealizationRange(desiredFirstIndex_, desiredCount_);
    }
    const bool horizontal = GetOrientation() == Orientation::Horizontal;
    const double primaryLimit = horizontal
        ? availableSize.width : availableSize.height;
    const bool constrained =
        std::isfinite(primaryLimit) && primaryLimit < 1.0e11;
    double linePrimary = 0.0;
    double lineCross = 0.0;
    double desiredPrimary = 0.0;
    double desiredCross = 0.0;
    const Size childAvailable{
        GetItemWidth() > 0.0 ? GetItemWidth() : availableSize.width,
        GetItemHeight() > 0.0 ? GetItemHeight() : availableSize.height};
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> measured = MeasureChild(*child, childAvailable);
        if (!measured) return Size{};
        const Size desired = child->GetDesiredSize();
        const double childPrimary = horizontal
            ? (GetItemWidth() > 0.0 ? GetItemWidth() : desired.width)
            : (GetItemHeight() > 0.0 ? GetItemHeight() : desired.height);
        const double childCross = horizontal
            ? (GetItemHeight() > 0.0 ? GetItemHeight() : desired.height)
            : (GetItemWidth() > 0.0 ? GetItemWidth() : desired.width);
        if (constrained && linePrimary > 0.0 &&
            linePrimary + childPrimary > primaryLimit) {
            desiredPrimary = std::max(desiredPrimary, linePrimary);
            desiredCross += lineCross;
            linePrimary = 0.0;
            lineCross = 0.0;
        }
        linePrimary += childPrimary;
        lineCross = std::max(lineCross, childCross);
    }
    desiredPrimary = std::max(desiredPrimary, linePrimary);
    desiredCross += lineCross;
    const Size content = horizontal
        ? Size{desiredPrimary, desiredCross}
        : Size{desiredCross, desiredPrimary};
    data_.extentWidth = std::max(content.width, availableSize.width);
    data_.extentHeight = std::max(content.height, MainExtent());
    data_.viewportWidth = availableSize.width;
    data_.viewportHeight = availableSize.height;
    return Size{
        std::min(content.width, availableSize.width),
        std::min(content.height, availableSize.height)};
}

Size VirtualizingWrapPanel::ArrangeOverride(Size finalSize) noexcept {
    const bool horizontal = GetOrientation() == Orientation::Horizontal;
    const double primaryLimit = horizontal
        ? finalSize.width : finalSize.height;
    double primary = 0.0;
    double cross = 0.0;
    double lineCross = 0.0;
    const double originCross = horizontal
        ? -data_.verticalOffset : -data_.horizontalOffset;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size desired = child->GetDesiredSize();
        const double childPrimary = horizontal
            ? (GetItemWidth() > 0.0 ? GetItemWidth() : desired.width)
            : (GetItemHeight() > 0.0 ? GetItemHeight() : desired.height);
        const double childCross = horizontal
            ? (GetItemHeight() > 0.0 ? GetItemHeight() : desired.height)
            : (GetItemWidth() > 0.0 ? GetItemWidth() : desired.width);
        if (primary > 0.0 && primary + childPrimary > primaryLimit) {
            primary = 0.0;
            cross += lineCross;
            lineCross = 0.0;
        }
        const Rect slot = horizontal
            ? Rect{primary, cross + originCross, childPrimary, childCross}
            : Rect{cross + originCross, primary, childCross, childPrimary};
        Base::Result<void> arranged = ArrangeChild(*child, slot);
        if (!arranged) return finalSize;
        primary += childPrimary;
        lineCross = std::max(lineCross, childCross);
    }
    return finalSize;
}

} // namespace Aero::Controls
