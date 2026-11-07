#include <Aero/Controls.hpp>
#include "gui/core/Describe.hpp"
#include "gui/core/ValueConversion.hpp"

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
        generator_->Detach();
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
    State().data.horizontalOffset = 0.0;
    State().data.verticalOffset = 0.0;
    SetMainOffset(oldMainOffset);
    State().crossExtent = 0.0;
    SetMainExtent(GetItemOffset(State().itemExtents.Size()));
    ClampOffsets();
    UpdateRealization(true);
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
    UpdateRealization(true);
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
    RebuildExtentTree();
    SetMainExtent(GetItemOffset(State().itemExtents.Size()));
    SetMainOffset(
        GetItemOffset(anchor) + intraItem);
    ClampOffsets();
    UpdateRealization(true);
}

VirtualizationCacheLength
VirtualizingStackPanel::GetCacheLength() const noexcept {
    return GetValue(CacheLengthProperty);
}

void VirtualizingStackPanel::SetCacheLength(
    VirtualizationCacheLength value) noexcept {
    SetValue(CacheLengthProperty, value);
    UpdateRealization(true);
}

VirtualizationCacheLengthUnit
VirtualizingStackPanel::GetCacheLengthUnit() const noexcept {
    return GetValue(CacheLengthUnitProperty);
}

void VirtualizingStackPanel::SetCacheLengthUnit(
    VirtualizationCacheLengthUnit value) noexcept {
    SetValue(CacheLengthUnitProperty, value);
    UpdateRealization(true);
}

double VirtualizingStackPanel::ExtentForIndex(
    std::uint32_t index) const noexcept {
    const VirtualizationState& state = State();
    return index < state.itemExtents.Size() &&
        state.itemExtents[index] > 0.0
        ? state.itemExtents[index]
        : GetEstimatedItemExtent();
}

double VirtualizingStackPanel::GetItemExtent(
    std::uint32_t index) const noexcept {
    return index < State().itemExtents.Size()
        ? ExtentForIndex(index)
        : 0.0;
}

double VirtualizingStackPanel::GetItemOffset(
    std::uint32_t index) const noexcept {
    const std::uint32_t end =
        std::min(index, State().itemExtents.Size());
    return static_cast<double>(end) *
        estimatedItemExtent_ +
        PrefixDeviation(end);
}

void VirtualizingStackPanel::AddExtentDeviation(
    std::uint32_t index,
    double delta) noexcept {
    VirtualizationState& state = State();
    std::uint32_t treeIndex = index + 1U;
    while (treeIndex < state.extentTree.Size()) {
        state.extentTree[treeIndex] += delta;
        treeIndex += treeIndex &
            (~treeIndex + 1U);
    }
}

double VirtualizingStackPanel::PrefixDeviation(
    std::uint32_t count) const noexcept {
    const VirtualizationState& state = State();
    double result = 0.0;
    std::uint32_t treeIndex =
        std::min(count, state.itemExtents.Size());
    while (treeIndex > 0U &&
        treeIndex < state.extentTree.Size()) {
        result += state.extentTree[treeIndex];
        treeIndex -= treeIndex &
            (~treeIndex + 1U);
    }
    return result;
}

void VirtualizingStackPanel::RebuildExtentTree() noexcept {
    VirtualizationState& state = State();
    state.extentTree.Resize(
            state.itemExtents.Size() + 1U, 0.0);
    for (double& value : state.extentTree) {
        value = 0.0;
    }
    for (std::uint32_t index = 0U;
        index < state.itemExtents.Size(); ++index) {
        if (state.itemExtents[index] > 0.0) {
            AddExtentDeviation(
                index,
                state.itemExtents[index] -
                    estimatedItemExtent_);
        }
    }
}

void VirtualizingStackPanel::SetMeasuredExtent(
    std::uint32_t index,
    double value) noexcept {
    VirtualizationState& state = State();
    if (index >= state.itemExtents.Size()) return;
    const double oldDeviation =
        state.itemExtents[index] > 0.0
        ? state.itemExtents[index] -
            estimatedItemExtent_
        : 0.0;
    const double newDeviation =
        value - estimatedItemExtent_;
    state.itemExtents[index] = value;
    AddExtentDeviation(
        index,
        newDeviation - oldDeviation);
}

double VirtualizingStackPanel::MainOffset() const noexcept {
    return GetOrientation() == Orientation::Vertical
        ? State().data.verticalOffset
        : State().data.horizontalOffset;
}

double VirtualizingStackPanel::MainViewport() const noexcept {
    return GetOrientation() == Orientation::Vertical
        ? State().data.viewportHeight
        : State().data.viewportWidth;
}

double VirtualizingStackPanel::MainExtent() const noexcept {
    return GetOrientation() == Orientation::Vertical
        ? State().data.extentHeight
        : State().data.extentWidth;
}

void VirtualizingStackPanel::SetMainOffset(
    double value) noexcept {
    if (GetOrientation() == Orientation::Vertical) {
        State().data.verticalOffset = value;
    } else {
        State().data.horizontalOffset = value;
    }
}

void VirtualizingStackPanel::SetMainExtent(
    double value) noexcept {
    if (GetOrientation() == Orientation::Vertical) {
        State().data.extentHeight = value;
    } else {
        State().data.extentWidth = value;
    }
}

void VirtualizingStackPanel::ClampOffsets() noexcept {
    State().data.horizontalOffset = std::clamp(
        State().data.horizontalOffset,
        0.0,
        std::max(
            0.0,
            State().data.extentWidth -
                State().data.viewportWidth));
    State().data.verticalOffset = std::clamp(
        State().data.verticalOffset,
        0.0,
        std::max(
            0.0,
            State().data.extentHeight -
                State().data.viewportHeight));
}

std::uint32_t
VirtualizingStackPanel::ItemIndexAtOffset(
    double offset) const noexcept {
    if (State().itemExtents.Empty()) return 0U;
    const double target = std::max(0.0, offset);
    std::uint32_t low = 0U;
    std::uint32_t high = State().itemExtents.Size();
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
        low, State().itemExtents.Size() - 1U);
}

void
VirtualizingStackPanel::CalculateRealizationRange() noexcept {
    const std::uint32_t itemCount =
        State().itemExtents.Size();
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

void VirtualizingStackPanel::UpdateRealization(
    bool notifyGenerator) noexcept {
    CalculateRealizationRange();
    if (notifyGenerator &&
        generator_ != nullptr) {
        generator_->SetRealizationRange(
            desiredFirstIndex_, desiredCount_);
    }
}

void VirtualizingStackPanel::ResizeExtentCache(
    std::uint32_t itemCount) noexcept {
    State().itemExtents.Resize(
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
        RebuildExtentTree();
        SetMainExtent(
            GetItemOffset(State().itemExtents.Size()));
        ClampOffsets();
    }
    CalculateRealizationRange();
    Panel::OnPropertyInvalidated(flags);
}

void VirtualizingStackPanel::ApplyExtentDelta(
    const ItemsChangedEvent& event,
    std::uint32_t itemCount) noexcept {
    VirtualizationState& state = State();
    if (event.action == ItemsChangeAction::Reset) {
        state.itemExtents.Clear();
        return ResizeExtentCache(itemCount);
    }
    if (event.action == ItemsChangeAction::Add &&
        event.newCount > 0U &&
        event.newIndex <= state.itemExtents.Size() &&
        itemCount >= state.itemExtents.Size() &&
        event.newCount ==
            itemCount - state.itemExtents.Size()) {
        const std::uint32_t oldSize =
            state.itemExtents.Size();
        state.itemExtents.Resize(
                oldSize + event.newCount, 0.0);
        for (std::uint32_t index = oldSize;
            index > event.newIndex; --index) {
            state.itemExtents[
                index + event.newCount - 1U] =
                state.itemExtents[index - 1U];
        }
        for (std::uint32_t offset = 0U;
            offset < event.newCount; ++offset) {
            state.itemExtents[event.newIndex + offset] =
                0.0;
        }
        return;
    }
    if (event.action == ItemsChangeAction::Remove &&
        event.oldCount > 0U &&
        event.oldIndex <= state.itemExtents.Size() &&
        event.oldCount <=
            state.itemExtents.Size() - event.oldIndex &&
        event.oldCount <= state.itemExtents.Size() &&
        itemCount ==
            state.itemExtents.Size() - event.oldCount) {
        for (std::uint32_t index = event.oldIndex;
            index + event.oldCount <
                state.itemExtents.Size(); ++index) {
            state.itemExtents[index] =
                state.itemExtents[
                    index + event.oldCount];
        }
        state.itemExtents.Resize(itemCount);
        return;
    }
    if (event.action == ItemsChangeAction::Replace &&
        event.oldCount == event.newCount &&
        event.newIndex <= state.itemExtents.Size() &&
        event.newCount <=
            state.itemExtents.Size() - event.newIndex) {
        for (std::uint32_t offset = 0U;
            offset < event.newCount; ++offset) {
            state.itemExtents[event.newIndex + offset] =
                0.0;
        }
        return;
    }
    if (event.action == ItemsChangeAction::Move &&
        event.oldCount == 1U &&
        event.newCount == 1U &&
        event.oldIndex < state.itemExtents.Size() &&
        event.newIndex < state.itemExtents.Size()) {
        const double moving =
            state.itemExtents[event.oldIndex];
        if (event.oldIndex < event.newIndex) {
            for (std::uint32_t index =
                    event.oldIndex;
                index < event.newIndex; ++index) {
                state.itemExtents[index] =
                    state.itemExtents[index + 1U];
            }
        } else {
            for (std::uint32_t index =
                    event.oldIndex;
                index > event.newIndex; --index) {
                state.itemExtents[index] =
                    state.itemExtents[index - 1U];
            }
        }
        state.itemExtents[event.newIndex] = moving;
        return;
    }
    state.itemExtents.Clear();
    return ResizeExtentCache(itemCount);
}

void VirtualizingStackPanel::HandleItemsChanged(
    const ItemsChangedEvent& event,
    std::uint32_t itemCount) noexcept {
    const std::uint32_t oldCount =
        State().itemExtents.Size();
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

    ApplyExtentDelta(event, itemCount);
    RebuildExtentTree();
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
    InvalidateMeasure();
    InvalidateArrange();
}

bool
VirtualizingStackPanel::AttachGenerator(
    ItemContainerGenerator& generator,
    std::uint32_t itemCount) noexcept {
    if (generator_ != nullptr) {
        return false;
    }
    generator_ = &generator;
    State().itemExtents.Clear();
    ResizeExtentCache(itemCount);
    RebuildExtentTree();
    SetMainExtent(GetItemOffset(itemCount));
    ClampOffsets();
    CalculateRealizationRange();
    return true;
}

void VirtualizingStackPanel::DetachGenerator(
    ItemContainerGenerator& generator) noexcept {
    if (generator_ != &generator) return;
    generator_ = nullptr;
    visibleFirstIndex_ = 0U;
    visibleCount_ = 0U;
    desiredFirstIndex_ = 0U;
    desiredCount_ = 0U;
    if (state_ != nullptr) {
        state_->itemExtents.Clear();
        state_->extentTree.Clear();
        state_->data.extentWidth = 0.0;
        state_->data.extentHeight = 0.0;
        state_->data.horizontalOffset = 0.0;
        state_->data.verticalOffset = 0.0;
    }
}

void VirtualizingStackPanel::SetViewport(
    Size viewport) noexcept {
    if (!ValidViewport(viewport)) {
        return;
    }
    const bool changed =
        !Same(State().data.viewportWidth, viewport.width) ||
        !Same(State().data.viewportHeight, viewport.height);
    if (!changed) return;
    State().data.viewportWidth = viewport.width;
    State().data.viewportHeight = viewport.height;
    ClampOffsets();
    UpdateRealization(true);
    InvalidateMeasure();
    InvalidateArrange();
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
    UpdateRealization(true);
    InvalidateMeasure();
    InvalidateArrange();
}

void VirtualizingStackPanel::SetCrossScrollOffset(
    double value) noexcept {
    const bool vertical =
        GetOrientation() == Orientation::Vertical;
    const double extent = vertical
        ? State().data.extentWidth
        : State().data.extentHeight;
    const double viewport = vertical
        ? State().data.viewportWidth
        : State().data.viewportHeight;
    const double next = std::clamp(
        value,
        0.0,
        std::max(0.0, extent - viewport));
    double& current = vertical
        ? State().data.horizontalOffset
        : State().data.verticalOffset;
    if (Same(next, current)) return;
    current = next;
    InvalidateArrange();
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

bool
VirtualizingStackPanel::LineHorizontal(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return false;
    }
    const double old = State().data.horizontalOffset;
    SetHorizontalOffset(std::max(0.0, old + direction *
        (GetOrientation() == Orientation::Horizontal
            ? GetEstimatedItemExtent() : CrossLineExtent)));
    return !Same(old, State().data.horizontalOffset);
}

bool
VirtualizingStackPanel::LineVertical(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return false;
    }
    const double old = State().data.verticalOffset;
    SetVerticalOffset(std::max(0.0, old + direction *
        (GetOrientation() == Orientation::Vertical
            ? GetEstimatedItemExtent() : CrossLineExtent)));
    return !Same(old, State().data.verticalOffset);
}

bool
VirtualizingStackPanel::PageHorizontal(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return false;
    }
    const double old = State().data.horizontalOffset;
    SetHorizontalOffset(std::max(0.0, old + direction * State().data.viewportWidth));
    return !Same(old, State().data.horizontalOffset);
}

bool
VirtualizingStackPanel::PageVertical(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return false;
    }
    const double old = State().data.verticalOffset;
    SetVerticalOffset(std::max(0.0, old + direction * State().data.viewportHeight));
    return !Same(old, State().data.verticalOffset);
}

Size
VirtualizingStackPanel::MeasureOverride(
    Size availableSize) noexcept {
    if (!ValidViewport(availableSize)) {
        return Size{};
    }
    const std::uint32_t anchor =
        State().itemExtents.Empty()
        ? 0U
        : ItemIndexAtOffset(MainOffset());
    const double intraItem =
        State().itemExtents.Empty()
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
        MeasureChild(*child, childAvailable);
        const Size desired = child->GetDesiredSize();
        const double extent =
            orientation == Orientation::Vertical
            ? desired.height
            : desired.width;
        const std::uint32_t itemIndex =
            first + localIndex;
        if (itemIndex < State().itemExtents.Size() &&
            std::isfinite(extent) &&
            extent > 0.0) {
            SetMeasuredExtent(
                itemIndex, extent);
        }
        crossExtent = std::max(
            crossExtent,
            static_cast<double>(orientation == Orientation::Vertical
                ? desired.width
                : desired.height));
        ++localIndex;
    }
    State().crossExtent = crossExtent;
    SetMainExtent(GetItemOffset(State().itemExtents.Size()));
    if (orientation == Orientation::Vertical) {
        State().data.extentWidth = std::max(
            State().crossExtent, static_cast<double>(availableSize.width));
    } else {
        State().data.extentHeight = std::max(
            State().crossExtent, static_cast<double>(availableSize.height));
    }
    if (!State().itemExtents.Empty()) {
        SetMainOffset(
            GetItemOffset(anchor) + intraItem);
    }
    ClampOffsets();
    UpdateRealization(true);
    return orientation == Orientation::Vertical
        ? Size{State().crossExtent, MainExtent()}
        : Size{MainExtent(), State().crossExtent};
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
        ? State().data.horizontalOffset
        : State().data.verticalOffset;
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
                    static_cast<float>(State().crossExtent)),
                extent}
            : Rect{
                offset,
                -crossOffset,
                extent,
                std::max(
                    finalSize.height,
                    static_cast<float>(State().crossExtent))};
        ArrangeChild(*child, slot);
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
    (void)available;
    return WrapItemFallback;
}

} // namespace

bool VirtualizingPanel::AttachGenerator(
    ItemContainerGenerator& generator,
    std::uint32_t itemCount) noexcept {
    if (generator_ != nullptr) {
        return false;
    }
    generator_ = &generator;
    VirtualizationState& state = State();
    state.itemExtents.Clear();
    for (std::uint32_t index = 0U; index < itemCount; ++index) {
        state.itemExtents.PushBack(1.0);
    }
    return true;
}

void VirtualizingPanel::DetachGenerator(
    ItemContainerGenerator& generator) noexcept {
    if (generator_ != &generator) return;
    generator_ = nullptr;
    visibleFirstIndex_ = 0U;
    visibleCount_ = 0U;
    desiredFirstIndex_ = 0U;
    desiredCount_ = 0U;
    if (state_ != nullptr) {
        state_->itemExtents.Clear();
        state_->data = {};
    }
}

void VirtualizingPanel::HandleItemsChanged(
    const ItemsChangedEvent&,
    std::uint32_t itemCount) noexcept {
    VirtualizationState& state = State();
    state.itemExtents.Clear();
    for (std::uint32_t index = 0U; index < itemCount; ++index) {
        state.itemExtents.PushBack(1.0);
    }
    InvalidateMeasure();
    InvalidateArrange();
}

VirtualizingWrapPanel::VirtualizingWrapPanel() noexcept
    : VirtualizingPanel(StaticTypeId()) {}

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

Orientation VirtualizingWrapPanel::GetOrientation() const noexcept {
    return GetValue(OrientationProperty);
}

void VirtualizingWrapPanel::SetOrientation(Orientation value) noexcept {
    SetValue(OrientationProperty, value);
}

std::uint32_t VirtualizingWrapPanel::GetOverscanCount() const noexcept {
    return GetValue(OverscanCountProperty);
}

void VirtualizingWrapPanel::SetOverscanCount(std::uint32_t value) noexcept {
    SetValue(OverscanCountProperty, value);
}

VirtualizationCacheLength VirtualizingWrapPanel::GetCacheLength() const noexcept {
    return GetValue(CacheLengthProperty);
}

void VirtualizingWrapPanel::SetCacheLength(VirtualizationCacheLength value) noexcept {
    SetValue(CacheLengthProperty, value);
}

VirtualizationCacheLengthUnit VirtualizingWrapPanel::GetCacheLengthUnit() const noexcept {
    return GetValue(CacheLengthUnitProperty);
}

void VirtualizingWrapPanel::SetCacheLengthUnit(VirtualizationCacheLengthUnit value) noexcept {
    SetValue(CacheLengthUnitProperty, value);
}

double VirtualizingWrapPanel::MainExtent() const noexcept {
    return GetOrientation() == Orientation::Vertical
        ? State().data.extentHeight
        : State().data.extentWidth;
}

void VirtualizingWrapPanel::SetMainExtent(double value) noexcept {
    if (GetOrientation() == Orientation::Vertical) {
        State().data.extentHeight = value;
    } else {
        State().data.extentWidth = value;
    }
}

void VirtualizingWrapPanel::SetMainOffset(double value) noexcept {
    const double limit = std::max(0.0, MainExtent() - (
        GetOrientation() == Orientation::Vertical
            ? State().data.viewportHeight
            : State().data.viewportWidth));
    const double next = std::clamp(value, 0.0, limit);
    if (GetOrientation() == Orientation::Vertical) {
        State().data.verticalOffset = next;
    } else {
        State().data.horizontalOffset = next;
    }
}

void VirtualizingWrapPanel::SetViewport(Size viewport) noexcept {
    State().data.viewportWidth = viewport.width;
    State().data.viewportHeight = viewport.height;
    CalculateRealizationRange();
    InvalidateMeasure();
    InvalidateArrange();
}

void VirtualizingWrapPanel::SetHorizontalOffset(double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) return;
    if (GetOrientation() == Orientation::Horizontal) {
        SetMainOffset(value);
    } else {
        State().data.horizontalOffset = value;
    }
    InvalidateArrange();
}

void VirtualizingWrapPanel::SetVerticalOffset(double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) return;
    if (GetOrientation() == Orientation::Vertical) {
        SetMainOffset(value);
    } else {
        State().data.verticalOffset = value;
    }
    InvalidateArrange();
}

bool VirtualizingWrapPanel::LineHorizontal(double direction) noexcept {
    if (!std::isfinite(direction)) {
        return false;
    }
    SetHorizontalOffset(std::max(0.0, State().data.horizontalOffset + direction * 16.0));
    return true;
}

bool VirtualizingWrapPanel::LineVertical(double direction) noexcept {
    if (!std::isfinite(direction)) {
        return false;
    }
    SetVerticalOffset(std::max(0.0, State().data.verticalOffset + direction * 16.0));
    return true;
}

bool VirtualizingWrapPanel::PageHorizontal(double direction) noexcept {
    if (!std::isfinite(direction)) {
        return false;
    }
    SetHorizontalOffset(std::max(
        0.0, State().data.horizontalOffset + direction * State().data.viewportWidth));
    return true;
}

bool VirtualizingWrapPanel::PageVertical(double direction) noexcept {
    if (!std::isfinite(direction)) {
        return false;
    }
    SetVerticalOffset(std::max(
        0.0, State().data.verticalOffset + direction * State().data.viewportHeight));
    return true;
}

void VirtualizingWrapPanel::CalculateRealizationRange() noexcept {
    const std::uint32_t itemCount = State().itemExtents.Size();
    if (itemCount == 0U) {
        visibleFirstIndex_ = 0U;
        visibleCount_ = 0U;
        desiredFirstIndex_ = 0U;
        desiredCount_ = 0U;
        return;
    }
    const Size viewport{
        State().data.viewportWidth, State().data.viewportHeight};
    const double slotWidth = WrapSlotWidth(*this, viewport);
    const double slotHeight = WrapSlotHeight(*this, viewport);
    const std::uint32_t columns = std::max(
        1U,
        slotWidth > 0.0 && std::isfinite(viewport.width) && viewport.width > 0.0
            ? static_cast<std::uint32_t>(std::max(1.0, viewport.width / slotWidth))
            : 1U);
    const double offset = std::max(0.0, State().data.verticalOffset);
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
        MeasureChild(*child, childAvailable);
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
    State().data.extentWidth = std::max(content.width, availableSize.width);
    State().data.extentHeight = std::max(static_cast<double>(content.height), MainExtent());
    State().data.viewportWidth = availableSize.width;
    State().data.viewportHeight = availableSize.height;
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
        ? -State().data.verticalOffset : -State().data.horizontalOffset;
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
        ArrangeChild(*child, slot);
        primary += childPrimary;
        lineCross = std::max(lineCross, childCross);
    }
    return finalSize;
}

AERO_DESCRIBE(VirtualizingPanel) {
    using namespace Aero::Meta;
    Register<VirtualizingPanel>(context, TypeFlags::Abstract)
        .Property(VirtualizingPanel::ScrollUnitProperty, ScrollUnit::Item, AffectsParentMeasure)
        .Property(VirtualizingPanel::VirtualizationModeProperty, VirtualizationMode::Standard, AffectsParentMeasure);
}

AERO_DESCRIBE(VirtualizingStackPanel) {
    using namespace Aero::Meta;
    Register<VirtualizationCacheLength>(context)
        .Field<&VirtualizationCacheLength::cacheBeforeViewport>("CacheBeforeViewport")
        .Field<&VirtualizationCacheLength::cacheAfterViewport>("CacheAfterViewport")
        .ValueSemantics();

    Register<VirtualizingStackPanel>(context)
        .Property(VirtualizingStackPanel::OrientationProperty, Orientation::Vertical, AffectsMeasure)
        .Property(VirtualizingStackPanel::OverscanCountProperty, std::uint32_t{2}, AffectsMeasure)
        .Property(VirtualizingStackPanel::EstimatedItemExtentProperty, 24.0, AffectsMeasure, &Base::Validate::Positive<double>)
        .Property(VirtualizingStackPanel::CacheLengthProperty, VirtualizationCacheLength{}, AffectsMeasure)
        .Property(VirtualizingStackPanel::CacheLengthUnitProperty, VirtualizationCacheLengthUnit::Item, AffectsMeasure)
        .Factory();
}

AERO_DESCRIBE(VirtualizingWrapPanel) {
    using namespace Aero::Meta;
    Register<VirtualizingWrapPanel>(context)
        .Property(VirtualizingWrapPanel::ItemWidthProperty, 0.0, AffectsMeasure, &Base::Validate::NonNegative<double>)
        .Property(VirtualizingWrapPanel::ItemHeightProperty, 0.0, AffectsMeasure, &Base::Validate::NonNegative<double>)
        .Property(VirtualizingWrapPanel::OrientationProperty, Orientation::Vertical, AffectsMeasure)
        .Property(VirtualizingWrapPanel::OverscanCountProperty, std::uint32_t{2}, AffectsMeasure)
        .Property(VirtualizingWrapPanel::CacheLengthProperty, VirtualizationCacheLength{}, AffectsMeasure)
        .Property(VirtualizingWrapPanel::CacheLengthUnitProperty, VirtualizationCacheLengthUnit::Item, AffectsMeasure)
        .Factory();
}

} // namespace Aero::Controls
