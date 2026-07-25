#include <Aero/Controls/Virtualization.hpp>

#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>

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
    : Panel(StaticTypeId()) {
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

Base::Result<void>
VirtualizingStackPanel::SetOrientation(
    Orientation value) noexcept {
    if (value > Orientation::Vertical) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VirtualizingStackPanel orientation is invalid");
    }
    if (value == GetOrientation()) return {};
    const double oldMainOffset = MainOffset();
    Base::Result<void> stored = SetValue(
        OrientationProperty,
        Value::FromUnsignedInteger(
            BuiltinTypes::Orientation,
            static_cast<std::uint64_t>(value)));
    if (!stored) return stored.GetStatus();
    orientation_ = value;
    data_.horizontalOffset = 0.0;
    data_.verticalOffset = 0.0;
    SetMainOffset(oldMainOffset);
    crossExtent_ = 0.0;
    SetMainExtent(ItemOffset(itemExtents_.Size()));
    ClampOffsets();
    return UpdateRealization(true);
}

std::uint32_t
VirtualizingStackPanel::OverscanCount() const noexcept {
    return overscanCount_;
}

Base::Result<void>
VirtualizingStackPanel::SetOverscanCount(
    std::uint32_t value) noexcept {
    if (value == OverscanCount()) return {};
    Base::Result<void> stored = SetValue(
        OverscanCountProperty,
        Value::FromUnsignedInteger(
            BuiltinTypes::UnsignedInteger,
            value));
    if (!stored) return stored.GetStatus();
    overscanCount_ = value;
    return UpdateRealization(true);
}

double
VirtualizingStackPanel::EstimatedItemExtent() const noexcept {
    return estimatedItemExtent_;
}

Base::Result<void>
VirtualizingStackPanel::SetEstimatedItemExtent(
    double value) noexcept {
    if (!std::isfinite(value) || value <= 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Estimated item extent must be positive and finite");
    }
    if (Same(value, EstimatedItemExtent())) return {};
    const std::uint32_t anchor =
        ItemIndexAtOffset(MainOffset());
    const double intraItem =
        MainOffset() - ItemOffset(anchor);
    Base::Result<void> stored = SetValue(
        EstimatedItemExtentProperty,
        Value::FromDouble(
            BuiltinTypes::Double, value));
    if (!stored) return stored.GetStatus();
    estimatedItemExtent_ = value;
    Base::Result<void> rebuilt =
        RebuildExtentTree();
    if (!rebuilt) return rebuilt.GetStatus();
    SetMainExtent(ItemOffset(itemExtents_.Size()));
    SetMainOffset(
        ItemOffset(anchor) + intraItem);
    ClampOffsets();
    return UpdateRealization(true);
}

double VirtualizingStackPanel::ExtentForIndex(
    std::uint32_t index) const noexcept {
    return index < itemExtents_.Size() &&
        itemExtents_[index] > 0.0
        ? itemExtents_[index]
        : EstimatedItemExtent();
}

double VirtualizingStackPanel::ItemExtent(
    std::uint32_t index) const noexcept {
    return index < itemExtents_.Size()
        ? ExtentForIndex(index)
        : 0.0;
}

double VirtualizingStackPanel::ItemOffset(
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
        extentTree_.TryResize(
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
        if (target < ItemOffset(middle + 1U)) {
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
    const std::uint32_t overscan =
        OverscanCount();
    desiredFirstIndex_ =
        visibleFirstIndex_ > overscan
        ? visibleFirstIndex_ - overscan
        : 0U;
    const std::uint32_t desiredEnd =
        std::min(
            itemCount,
            visibleEnd +
                std::min(
                    overscan,
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
        Base::Result<bool> realized =
            generator_->SetRealizationRange(
                desiredFirstIndex_,
                desiredCount_);
        if (!realized) {
            return realized.GetStatus();
        }
    }
    return {};
}

Base::Result<void>
VirtualizingStackPanel::ResizeExtentCache(
    std::uint32_t itemCount) noexcept {
    return itemExtents_.TryResize(
        itemCount, 0.0);
}

Base::Result<void>
VirtualizingStackPanel::OnPropertyInvalidated(
    PropertyInvalidationFlags flags) noexcept {
    const double oldEstimate =
        estimatedItemExtent_;
    Base::Result<Value> orientation =
        GetValue(OrientationProperty);
    if (orientation) {
        orientation_ = static_cast<Orientation>(
            orientation.Value()
                .AsUnsignedInteger());
    }
    Base::Result<Value> overscan =
        GetValue(OverscanCountProperty);
    if (overscan) {
        overscanCount_ =
            static_cast<std::uint32_t>(
                overscan.Value()
                    .AsUnsignedInteger());
    }
    Base::Result<Value> estimate =
        GetValue(EstimatedItemExtentProperty);
    if (estimate) {
        estimatedItemExtent_ =
            estimate.Value().AsDouble();
    }
    if (!Same(
            oldEstimate,
            estimatedItemExtent_)) {
        Base::Result<void> rebuilt =
            RebuildExtentTree();
        if (!rebuilt) return rebuilt.GetStatus();
        SetMainExtent(
            ItemOffset(itemExtents_.Size()));
        ClampOffsets();
    }
    CalculateRealizationRange();
    return Panel::OnPropertyInvalidated(flags);
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
            itemExtents_.TryResize(
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
        return itemExtents_.TryResize(itemCount);
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
VirtualizingStackPanel::OnItemsChanged(
    const ItemsChangedEvent& event,
    std::uint32_t itemCount) noexcept {
    const std::uint32_t oldCount =
        itemExtents_.Size();
    std::uint32_t anchor = oldCount > 0U
        ? ItemIndexAtOffset(MainOffset())
        : 0U;
    const double intraItem = oldCount > 0U
        ? MainOffset() - ItemOffset(anchor)
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
            ItemOffset(anchor) + intraItem);
    } else {
        SetMainOffset(0.0);
    }
    SetMainExtent(ItemOffset(itemCount));
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
    SetMainExtent(ItemOffset(itemCount));
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

Base::Result<bool>
VirtualizingStackPanel::SetViewport(
    Size viewport) noexcept {
    if (!ValidViewport(viewport)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VirtualizingStackPanel viewport is invalid");
    }
    const bool changed =
        !Same(data_.viewportWidth, viewport.width) ||
        !Same(data_.viewportHeight, viewport.height);
    if (!changed) return false;
    data_.viewportWidth = viewport.width;
    data_.viewportHeight = viewport.height;
    ClampOffsets();
    Base::Result<void> realized =
        UpdateRealization(true);
    if (!realized) return realized.GetStatus();
    Base::Result<void> invalidated =
        InvalidateMeasure();
    if (!invalidated) {
        return invalidated.GetStatus();
    }
    invalidated = InvalidateArrange();
    return invalidated
        ? Base::Result<bool>(true)
        : Base::Result<bool>(
            invalidated.GetStatus());
}

Base::Result<bool>
VirtualizingStackPanel::SetMainScrollOffset(
    double value) noexcept {
    const double next = std::clamp(
        value,
        0.0,
        std::max(
            0.0,
            MainExtent() - MainViewport()));
    if (Same(next, MainOffset())) return false;
    SetMainOffset(next);
    Base::Result<void> realized =
        UpdateRealization(true);
    if (!realized) return realized.GetStatus();
    Base::Result<void> invalidated =
        InvalidateMeasure();
    if (!invalidated) {
        return invalidated.GetStatus();
    }
    invalidated = InvalidateArrange();
    return invalidated
        ? Base::Result<bool>(true)
        : Base::Result<bool>(
            invalidated.GetStatus());
}

Base::Result<bool>
VirtualizingStackPanel::SetCrossScrollOffset(
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
    if (Same(next, current)) return false;
    current = next;
    Base::Result<void> invalidated =
        InvalidateArrange();
    return invalidated
        ? Base::Result<bool>(true)
        : Base::Result<bool>(
            invalidated.GetStatus());
}

Base::Result<bool>
VirtualizingStackPanel::SetHorizontalOffset(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Horizontal offset must be finite and nonnegative");
    }
    return GetOrientation() == Orientation::Horizontal
        ? SetMainScrollOffset(value)
        : SetCrossScrollOffset(value);
}

Base::Result<bool>
VirtualizingStackPanel::SetVerticalOffset(
    double value) noexcept {
    if (!ValidNonnegative(value)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Vertical offset must be finite and nonnegative");
    }
    return GetOrientation() == Orientation::Vertical
        ? SetMainScrollOffset(value)
        : SetCrossScrollOffset(value);
}

Base::Result<bool>
VirtualizingStackPanel::LineHorizontal(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Horizontal line direction must be finite");
    }
    return SetHorizontalOffset(
        std::max(
            0.0,
            data_.horizontalOffset +
                direction *
                    (GetOrientation() ==
                            Orientation::Horizontal
                        ? EstimatedItemExtent()
                        : CrossLineExtent)));
}

Base::Result<bool>
VirtualizingStackPanel::LineVertical(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Vertical line direction must be finite");
    }
    return SetVerticalOffset(
        std::max(
            0.0,
            data_.verticalOffset +
                direction *
                    (GetOrientation() ==
                            Orientation::Vertical
                        ? EstimatedItemExtent()
                        : CrossLineExtent)));
}

Base::Result<bool>
VirtualizingStackPanel::PageHorizontal(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Horizontal page direction must be finite");
    }
    return SetHorizontalOffset(
        std::max(
            0.0,
            data_.horizontalOffset +
                direction *
                    data_.viewportWidth));
}

Base::Result<bool>
VirtualizingStackPanel::PageVertical(
    double direction) noexcept {
    if (!std::isfinite(direction)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Vertical page direction must be finite");
    }
    return SetVerticalOffset(
        std::max(
            0.0,
            data_.verticalOffset +
                direction *
                    data_.viewportHeight));
}

Base::Result<Size>
VirtualizingStackPanel::MeasureOverride(
    Size availableSize) noexcept {
    if (!ValidViewport(availableSize)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "VirtualizingStackPanel measure size is invalid");
    }
    const std::uint32_t anchor =
        itemExtents_.Empty()
        ? 0U
        : ItemIndexAtOffset(MainOffset());
    const double intraItem =
        itemExtents_.Empty()
        ? 0.0
        : MainOffset() - ItemOffset(anchor);
    const Orientation orientation =
        GetOrientation();
    double crossExtent = 0.0;
    std::uint32_t localIndex = 0U;
    const std::uint32_t first =
        generator_ != nullptr
        ? generator_->FirstGeneratedIndex()
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
        if (!measured) return measured.GetStatus();
        const Size desired = child->DesiredSize();
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
    SetMainExtent(ItemOffset(itemExtents_.Size()));
    if (orientation == Orientation::Vertical) {
        data_.extentWidth = std::max(
            crossExtent_, availableSize.width);
    } else {
        data_.extentHeight = std::max(
            crossExtent_, availableSize.height);
    }
    if (!itemExtents_.Empty()) {
        SetMainOffset(
            ItemOffset(anchor) + intraItem);
    }
    ClampOffsets();
    Base::Result<void> realized =
        UpdateRealization(true);
    if (!realized) return realized.GetStatus();
    return orientation == Orientation::Vertical
        ? Size{crossExtent_, MainExtent()}
        : Size{MainExtent(), crossExtent_};
}

Base::Result<Size>
VirtualizingStackPanel::ArrangeOverride(
    Size finalSize) noexcept {
    Base::Result<bool> viewport =
        SetViewport(finalSize);
    if (!viewport) return viewport.GetStatus();
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
        ? generator_->FirstGeneratedIndex()
        : desiredFirstIndex_;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const std::uint32_t itemIndex =
            first + localIndex;
        const double extent =
            ExtentForIndex(itemIndex);
        const double offset =
            ItemOffset(itemIndex) - mainOffset;
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
        if (!arranged) return arranged.GetStatus();
        ++localIndex;
    }
    return finalSize;
}

} // namespace Aero::Controls
