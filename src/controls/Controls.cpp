#include <Aero/Controls/Controls.hpp>
#include <Aero/Documents/Documents.hpp>

#include "TextLayoutService.hpp"
#include <Aero/Core/ObjectServices.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace Aero::Controls {

using namespace Aero::Core;
using namespace Aero::Presentation;
namespace {

bool IsValidTextSize(Size value) noexcept {
    return IsFinite(value) && value.width >= 0.0 && value.height >= 0.0;
}

struct EffectiveGridSpan final {
    std::uint32_t index = 0U;
    std::uint32_t span = 1U;
};

EffectiveGridSpan CoerceGridSpan(
    std::uint32_t index,
    std::uint32_t span,
    std::uint32_t trackCount) noexcept {
    trackCount = std::max(
        trackCount, std::uint32_t{1U});
    EffectiveGridSpan result;
    result.index = std::min(
        index, trackCount - 1U);
    result.span = std::min(
        std::max(span, 1U),
        trackCount - result.index);
    return result;
}

} // namespace

Base::Result<void> Panel::BuildDisplayList(
    DisplayListBuilder& builder) noexcept {
    return PaintBrushRect(
        builder,
        BackgroundBrush(),
        Rect{
            0.0, 0.0,
            RenderSize().width,
            RenderSize().height});
}

Base::Result<void> Control::BuildDisplayList(
    DisplayListBuilder& builder) noexcept {
    return PaintBrushRect(
        builder,
        BackgroundBrush(),
        Rect{
            0.0, 0.0,
            RenderSize().width,
            RenderSize().height});
}

StackPanel::StackPanel() noexcept : StackPanel(Orientation::Vertical) {}

StackPanel::StackPanel(Orientation orientation) noexcept
    : Panel(StaticTypeId()) {
    if (orientation != Orientation::Vertical) {
        static_cast<void>(SetOrientation(orientation));
    }
}

Orientation StackPanel::GetOrientation() const noexcept {
    return GetValueOr(
        OrientationProperty, Orientation::Vertical);
}

Base::Result<void> StackPanel::SetOrientation(Orientation value) noexcept {
    return SetValue(OrientationProperty, value);
}

Base::Result<Size> StackPanel::MeasureOverride(Size availableSize) noexcept {
    Size desired;
    const Orientation orientation = GetOrientation();
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Size childAvailable = availableSize;
        if (orientation == Orientation::Vertical) childAvailable.height = 1.0e12;
        else childAvailable.width = 1.0e12;
        Base::Result<void> measured = MeasureChild(*child, childAvailable);
        if (!measured) return measured.GetStatus();
        const Size childDesired = child->DesiredSize();
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

Base::Result<Size> StackPanel::ArrangeOverride(Size finalSize) noexcept {
    double offset = 0.0;
    const Orientation orientation = GetOrientation();
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size desired = child->DesiredSize();
        const Rect slot = orientation == Orientation::Vertical
            ? Rect{0.0, offset, finalSize.width, desired.height}
            : Rect{offset, 0.0, desired.width, finalSize.height};
        Base::Result<void> arranged = ArrangeChild(*child, slot);
        if (!arranged) return arranged.GetStatus();
        offset += orientation == Orientation::Vertical
            ? desired.height : desired.width;
    }
    return finalSize;
}

bool DockPanel::LastChildFill() const noexcept {
    return GetValueOr(LastChildFillProperty, true);
}

Base::Result<void> DockPanel::SetLastChildFill(
    bool value) noexcept {
    return SetValue(LastChildFillProperty, value);
}

Base::Result<void> DockPanel::SetChildDock(
    UIElement& child,
    Dock value) noexcept {
    bool attached = false;
    for (UIElement* current : LayoutChildren()) {
        attached = attached || current == &child;
    }
    if (!attached) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "DockPanel child must be attached before assigning Dock");
    }
    return child.SetValue(DockProperty, value);
}

Dock DockPanel::ChildDock(
    const UIElement& child) const noexcept {
    return child.GetValueOr(DockProperty, Dock::Left);
}

Base::Result<Size> DockPanel::MeasureOverride(
    Size availableSize) noexcept {
    Size desired;
    double consumedWidth = 0.0;
    double consumedHeight = 0.0;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size remaining{
            std::max(0.0, availableSize.width - consumedWidth),
            std::max(0.0, availableSize.height - consumedHeight)};
        Base::Result<void> measured =
            MeasureChild(*child, remaining);
        if (!measured) return measured.GetStatus();
        const Size childDesired = child->DesiredSize();
        const Dock dock = ChildDock(*child);
        if (dock == Dock::Left || dock == Dock::Right) {
            consumedWidth += childDesired.width;
            desired.width = std::max(
                desired.width,
                consumedWidth);
            desired.height = std::max(
                desired.height,
                consumedHeight + childDesired.height);
        } else {
            consumedHeight += childDesired.height;
            desired.height = std::max(
                desired.height,
                consumedHeight);
            desired.width = std::max(
                desired.width,
                consumedWidth + childDesired.width);
        }
    }
    return desired;
}

Base::Result<Size> DockPanel::ArrangeOverride(
    Size finalSize) noexcept {
    double left = 0.0;
    double top = 0.0;
    double right = finalSize.width;
    double bottom = finalSize.height;
    const UIElementChildRange children = LayoutChildren();
    for (std::uint32_t index = 0U;
         index < children.Size();
         ++index) {
        UIElement* child = children[index];
        if (child == nullptr) continue;
        Rect slot{
            left,
            top,
            std::max(0.0, right - left),
            std::max(0.0, bottom - top)};
        const bool fill =
            LastChildFill() &&
            index + 1U == children.Size();
        if (!fill) {
            const Size desired = child->DesiredSize();
            switch (ChildDock(*child)) {
            case Dock::Left:
                slot.width = std::min(
                    slot.width, desired.width);
                left += slot.width;
                break;
            case Dock::Top:
                slot.height = std::min(
                    slot.height, desired.height);
                top += slot.height;
                break;
            case Dock::Right:
                slot.width = std::min(
                    slot.width, desired.width);
                slot.x = right - slot.width;
                right -= slot.width;
                break;
            case Dock::Bottom:
                slot.height = std::min(
                    slot.height, desired.height);
                slot.y = bottom - slot.height;
                bottom -= slot.height;
                break;
            }
        }
        Base::Result<void> arranged =
            ArrangeChild(*child, slot);
        if (!arranged) return arranged.GetStatus();
    }
    return finalSize;
}

Orientation WrapPanel::GetOrientation() const noexcept {
    return GetValueOr(
        OrientationProperty,
        Orientation::Horizontal);
}

Base::Result<void> WrapPanel::SetOrientation(
    Orientation value) noexcept {
    return SetValue(OrientationProperty, value);
}

double WrapPanel::ItemWidth() const noexcept {
    return GetValueOr(ItemWidthProperty, 0.0);
}

double WrapPanel::ItemHeight() const noexcept {
    return GetValueOr(ItemHeightProperty, 0.0);
}

Base::Result<void> WrapPanel::SetItemWidth(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "WrapPanel ItemWidth must be finite and nonnegative");
    }
    return SetValue(ItemWidthProperty, value);
}

Base::Result<void> WrapPanel::SetItemHeight(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "WrapPanel ItemHeight must be finite and nonnegative");
    }
    return SetValue(ItemHeightProperty, value);
}

Base::Result<Size> WrapPanel::MeasureOverride(
    Size availableSize) noexcept {
    const bool horizontal =
        GetOrientation() == Orientation::Horizontal;
    const double primaryLimit = horizontal
        ? availableSize.width : availableSize.height;
    const bool constrained =
        std::isfinite(primaryLimit) &&
        primaryLimit < 1.0e11;
    double linePrimary = 0.0;
    double lineCross = 0.0;
    double desiredPrimary = 0.0;
    double desiredCross = 0.0;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size childAvailable{
            ItemWidth() > 0.0
                ? ItemWidth() : availableSize.width,
            ItemHeight() > 0.0
                ? ItemHeight() : availableSize.height};
        Base::Result<void> measured =
            MeasureChild(*child, childAvailable);
        if (!measured) return measured.GetStatus();
        const Size desired = child->DesiredSize();
        const double childPrimary = horizontal
            ? (ItemWidth() > 0.0
                ? ItemWidth() : desired.width)
            : (ItemHeight() > 0.0
                ? ItemHeight() : desired.height);
        const double childCross = horizontal
            ? (ItemHeight() > 0.0
                ? ItemHeight() : desired.height)
            : (ItemWidth() > 0.0
                ? ItemWidth() : desired.width);
        if (constrained && linePrimary > 0.0 &&
            linePrimary + childPrimary > primaryLimit) {
            desiredPrimary = std::max(
                desiredPrimary, linePrimary);
            desiredCross += lineCross;
            linePrimary = 0.0;
            lineCross = 0.0;
        }
        linePrimary += childPrimary;
        lineCross = std::max(lineCross, childCross);
    }
    desiredPrimary = std::max(
        desiredPrimary, linePrimary);
    desiredCross += lineCross;
    return horizontal
        ? Size{desiredPrimary, desiredCross}
        : Size{desiredCross, desiredPrimary};
}

Base::Result<Size> WrapPanel::ArrangeOverride(
    Size finalSize) noexcept {
    const bool horizontal =
        GetOrientation() == Orientation::Horizontal;
    const double primaryLimit = horizontal
        ? finalSize.width : finalSize.height;
    double primary = 0.0;
    double cross = 0.0;
    double lineCross = 0.0;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size desired = child->DesiredSize();
        const double childPrimary = horizontal
            ? (ItemWidth() > 0.0
                ? ItemWidth() : desired.width)
            : (ItemHeight() > 0.0
                ? ItemHeight() : desired.height);
        const double childCross = horizontal
            ? (ItemHeight() > 0.0
                ? ItemHeight() : desired.height)
            : (ItemWidth() > 0.0
                ? ItemWidth() : desired.width);
        if (primary > 0.0 &&
            primary + childPrimary > primaryLimit) {
            primary = 0.0;
            cross += lineCross;
            lineCross = 0.0;
        }
        const Rect slot = horizontal
            ? Rect{primary, cross, childPrimary, childCross}
            : Rect{cross, primary, childCross, childPrimary};
        Base::Result<void> arranged =
            ArrangeChild(*child, slot);
        if (!arranged) return arranged.GetStatus();
        primary += childPrimary;
        lineCross = std::max(lineCross, childCross);
    }
    return finalSize;
}

std::uint32_t UniformGrid::Rows() const noexcept {
    return GetValueOr(RowsProperty, 0U);
}

std::uint32_t UniformGrid::Columns() const noexcept {
    return GetValueOr(ColumnsProperty, 0U);
}

std::uint32_t UniformGrid::FirstColumn() const noexcept {
    return GetValueOr(FirstColumnProperty, 0U);
}

Base::Result<void> UniformGrid::SetRows(
    std::uint32_t value) noexcept {
    return SetValue(RowsProperty, value);
}

Base::Result<void> UniformGrid::SetColumns(
    std::uint32_t value) noexcept {
    return SetValue(ColumnsProperty, value);
}

Base::Result<void> UniformGrid::SetFirstColumn(
    std::uint32_t value) noexcept {
    return SetValue(FirstColumnProperty, value);
}

void UniformGrid::ResolveDimensions(
    std::uint32_t childCount,
    std::uint32_t& rows,
    std::uint32_t& columns) const noexcept {
    rows = Rows();
    columns = Columns();
    if (childCount == 0U) {
        rows = rows == 0U ? 1U : rows;
        columns = columns == 0U ? 1U : columns;
        return;
    }
    if (rows == 0U && columns == 0U) {
        columns = static_cast<std::uint32_t>(
            std::ceil(std::sqrt(
                static_cast<double>(childCount))));
        rows = (childCount + columns - 1U) / columns;
    } else if (rows == 0U) {
        rows = (childCount + columns - 1U) / columns;
    } else if (columns == 0U) {
        columns = (childCount + rows - 1U) / rows;
    }
}

Base::Result<Size> UniformGrid::MeasureOverride(
    Size availableSize) noexcept {
    std::uint32_t count = 0U;
    for (UIElement* child : LayoutChildren()) {
        if (child != nullptr) ++count;
    }
    std::uint32_t rows = 0U;
    std::uint32_t columns = 0U;
    ResolveDimensions(count + FirstColumn(), rows, columns);
    const Size cellAvailable{
        availableSize.width / static_cast<double>(columns),
        availableSize.height / static_cast<double>(rows)};
    Size cellDesired;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> measured =
            MeasureChild(*child, cellAvailable);
        if (!measured) return measured.GetStatus();
        cellDesired.width = std::max(
            cellDesired.width,
            child->DesiredSize().width);
        cellDesired.height = std::max(
            cellDesired.height,
            child->DesiredSize().height);
    }
    return Size{
        cellDesired.width * columns,
        cellDesired.height * rows};
}

Base::Result<Size> UniformGrid::ArrangeOverride(
    Size finalSize) noexcept {
    std::uint32_t count = 0U;
    for (UIElement* child : LayoutChildren()) {
        if (child != nullptr) ++count;
    }
    std::uint32_t rows = 0U;
    std::uint32_t columns = 0U;
    ResolveDimensions(count + FirstColumn(), rows, columns);
    const double width =
        finalSize.width / static_cast<double>(columns);
    const double height =
        finalSize.height / static_cast<double>(rows);
    std::uint32_t index = std::min(
        FirstColumn(), columns - 1U);
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const std::uint32_t row = index / columns;
        const std::uint32_t column = index % columns;
        Base::Result<void> arranged = ArrangeChild(
            *child,
            {column * width, row * height, width, height});
        if (!arranged) return arranged.GetStatus();
        ++index;
    }
    return finalSize;
}

Canvas::Canvas() noexcept : Panel(StaticTypeId()) {}

Base::Result<void> Canvas::SetChildPosition(
    UIElement& child, Point position) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (!IsFinite(position)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Canvas child position must be finite");
    }
    bool attached = false;
    for (UIElement* current : LayoutChildren()) attached = attached || current == &child;
    if (!attached) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Canvas child must be attached before positioning");
    }
    Base::Result<void> left =
        child.SetValue(LeftProperty, position.x);
    return left
        ? child.SetValue(TopProperty, position.y)
        : left;
}

Point Canvas::ChildPosition(const UIElement& child) const noexcept {
    return {
        child.GetValueOr(LeftProperty, 0.0),
        child.GetValueOr(TopProperty, 0.0)};
}

Base::Result<Size> Canvas::MeasureOverride(Size) noexcept {
    Size desired;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> measured = MeasureChild(*child, {1.0e12, 1.0e12});
        if (!measured) return measured.GetStatus();
        const Point position = ChildPosition(*child);
        desired.width = std::max(
            desired.width, position.x + child->DesiredSize().width);
        desired.height = std::max(
            desired.height, position.y + child->DesiredSize().height);
    }
    return desired;
}

Base::Result<Size> Canvas::ArrangeOverride(Size finalSize) noexcept {
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size desired = child->DesiredSize();
        Point position = ChildPosition(*child);
        Base::Result<EffectiveValueSource> leftSource =
            child->GetValueSource(LeftProperty.Handle());
        if (!leftSource) return leftSource.GetStatus();
        if (leftSource.Value() !=
            EffectiveValueSource::Local) {
            Base::Result<EffectiveValueSource> rightSource =
                child->GetValueSource(RightProperty.Handle());
            if (!rightSource) return rightSource.GetStatus();
            if (rightSource.Value() ==
                EffectiveValueSource::Local) {
                position.x = finalSize.width -
                    child->GetValueOr(RightProperty, 0.0) -
                    desired.width;
            }
        }
        Base::Result<EffectiveValueSource> topSource =
            child->GetValueSource(TopProperty.Handle());
        if (!topSource) return topSource.GetStatus();
        if (topSource.Value() !=
            EffectiveValueSource::Local) {
            Base::Result<EffectiveValueSource> bottomSource =
                child->GetValueSource(BottomProperty.Handle());
            if (!bottomSource) return bottomSource.GetStatus();
            if (bottomSource.Value() ==
                EffectiveValueSource::Local) {
                position.y = finalSize.height -
                    child->GetValueOr(BottomProperty, 0.0) -
                    desired.height;
            }
        }
        Base::Result<void> arranged = ArrangeChild(*child,
            {position.x, position.y, desired.width, desired.height});
        if (!arranged) return arranged.GetStatus();
    }
    return finalSize;
}

Base::Result<void> ColumnDefinition::SetWidth(
    GridLength value) noexcept {
    if (!std::isfinite(value.value) ||
        value.value < 0.0 ||
        (value.unit == GridUnitType::Star &&
            value.value <= 0.0)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ColumnDefinition Width is invalid");
    }
    width_ = value;
    return {};
}

Base::Result<void> ColumnDefinition::SetMaxWidth(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ColumnDefinition MaxWidth is invalid");
    }
    maxWidth_ = value;
    return {};
}

Base::Result<void> ColumnDefinition::SetSharedSizeGroup(
    Base::StringView value) noexcept {
    return sharedSizeGroup_.TryAssign(value);
}

Base::Result<void> RowDefinition::SetHeight(
    GridLength value) noexcept {
    if (!std::isfinite(value.value) ||
        value.value < 0.0 ||
        (value.unit == GridUnitType::Star &&
            value.value <= 0.0)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "RowDefinition Height is invalid");
    }
    height_ = value;
    return {};
}

Base::Result<void> RowDefinition::SetMaxHeight(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "RowDefinition MaxHeight is invalid");
    }
    maxHeight_ = value;
    return {};
}

Base::Result<void> RowDefinition::SetSharedSizeGroup(
    Base::StringView value) noexcept {
    return sharedSizeGroup_.TryAssign(value);
}

Grid::Grid() noexcept
    : Panel(StaticTypeId()), columns_(), rows_(),
      desiredColumns_(), desiredRows_() {}

Base::Result<void> Grid::SetColumnDefinitions(
    Base::Span<const GridLength> definitions) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    Base::Result<void> valid = ValidateDefinitions(definitions);
    if (!valid) return valid;
    Base::Vector<GridLength> next;
    Base::Result<void> copied = next.TryAssign(definitions);
    if (!copied) return copied.GetStatus();
    columns_ = std::move(next);
    columnDefinitionObjects_.Clear();
    return InvalidateMeasure();
}

Base::Result<void> Grid::SetRowDefinitions(
    Base::Span<const GridLength> definitions) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    Base::Result<void> valid = ValidateDefinitions(definitions);
    if (!valid) return valid;
    Base::Vector<GridLength> next;
    Base::Result<void> copied = next.TryAssign(definitions);
    if (!copied) return copied.GetStatus();
    rows_ = std::move(next);
    rowDefinitionObjects_.Clear();
    return InvalidateMeasure();
}

Base::Result<void> Grid::SetChildCell(
    UIElement& child, std::uint32_t row, std::uint32_t column) noexcept {
    return SetChildCell(child, row, column, 1U, 1U);
}

Base::Result<void> Grid::SetChildCell(
    UIElement& child,
    std::uint32_t row,
    std::uint32_t column,
    std::uint32_t rowSpan,
    std::uint32_t columnSpan) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (rowSpan == 0U || columnSpan == 0U) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange,
            "Grid child span must be positive");
    }
    bool attached = false;
    for (UIElement* current : LayoutChildren()) attached = attached || current == &child;
    if (!attached) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Grid child must be attached before assigning a cell");
    }
    Base::Result<void> rowResult =
        child.SetValue(RowProperty, row);
    if (!rowResult) return rowResult;
    Base::Result<void> columnResult =
        child.SetValue(ColumnProperty, column);
    if (!columnResult) return columnResult;
    Base::Result<void> rowSpanResult =
        child.SetValue(RowSpanProperty, rowSpan);
    return rowSpanResult
        ? child.SetValue(ColumnSpanProperty, columnSpan)
        : rowSpanResult;
}

Base::Result<void> Grid::AddColumnDefinition(
    Base::Ref<ColumnDefinition> definition) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (!definition) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Grid ColumnDefinition is null");
    }
    Base::Result<void> objectAdded =
        columnDefinitionObjects_.TryPushBack(definition);
    if (!objectAdded) return objectAdded.GetStatus();
    Base::Result<void> lengthAdded =
        columns_.TryPushBack(definition->Width());
    if (!lengthAdded) {
        columnDefinitionObjects_.PopBack();
        return lengthAdded.GetStatus();
    }
    return InvalidateMeasure();
}

Base::Result<void> Grid::AddRowDefinition(
    Base::Ref<RowDefinition> definition) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (!definition) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Grid RowDefinition is null");
    }
    Base::Result<void> objectAdded =
        rowDefinitionObjects_.TryPushBack(definition);
    if (!objectAdded) return objectAdded.GetStatus();
    Base::Result<void> lengthAdded =
        rows_.TryPushBack(definition->Height());
    if (!lengthAdded) {
        rowDefinitionObjects_.PopBack();
        return lengthAdded.GetStatus();
    }
    return InvalidateMeasure();
}

Base::Result<void>
Grid::ClearColumnDefinitionObjects() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    columnDefinitionObjects_.Clear();
    columns_.Clear();
    return InvalidateMeasure();
}

Base::Result<void>
Grid::ClearRowDefinitionObjects() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    rowDefinitionObjects_.Clear();
    rows_.Clear();
    return InvalidateMeasure();
}

Base::Result<void> Grid::AddInputBinding(
    Base::Ref<Presentation::KeyBinding> binding) noexcept {
    if (!binding) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Grid InputBinding cannot be null");
    }
    Base::Result<void> finalized = binding->Finalize();
    if (!finalized) return finalized.GetStatus();
    return inputBindings_.TryPushBack(std::move(binding));
}

Base::StringView Grid::ColumnDefinitionsText() const noexcept {
    return GetValueOr(
        ColumnDefinitionsTextProperty,
        Base::StringView{});
}

Base::StringView Grid::RowDefinitionsText() const noexcept {
    return GetValueOr(
        RowDefinitionsTextProperty,
        Base::StringView{});
}

Base::Result<void> Grid::SetColumnDefinitionsText(
    Base::StringView value) noexcept {
    return SetValue(
        ColumnDefinitionsTextProperty, value);
}

Base::Result<void> Grid::SetRowDefinitionsText(
    Base::StringView value) noexcept {
    return SetValue(
        RowDefinitionsTextProperty, value);
}

Base::Result<Size> Grid::MeasureOverride(
    Size availableSize) noexcept {
    const std::uint32_t columns = ColumnCount();
    const std::uint32_t rows = RowCount();
    Base::Vector<double> desiredColumns;
    Base::Vector<double> desiredRows;
    Base::Result<void> resized = desiredColumns.TryResize(columns, 0.0);
    if (!resized) return resized.GetStatus();
    resized = desiredRows.TryResize(rows, 0.0);
    if (!resized) return resized.GetStatus();

    for (std::uint32_t index = 0U; index < columns; ++index) {
        const GridLength definition = ColumnAt(index);
        if (definition.unit == GridUnitType::Pixel)
            desiredColumns[index] = definition.value;
    }
    for (std::uint32_t index = 0U; index < rows; ++index) {
        const GridLength definition = RowAt(index);
        if (definition.unit == GridUnitType::Pixel)
            desiredRows[index] = definition.value;
    }

    constexpr double Unconstrained = 1.0e12;
    constexpr double FiniteConstraintLimit =
        Unconstrained * 0.5;
    double pixelWidth = 0.0;
    double pixelHeight = 0.0;
    double columnStarWeight = 0.0;
    double rowStarWeight = 0.0;
    for (std::uint32_t index = 0U;
         index < columns; ++index) {
        const GridLength definition = ColumnAt(index);
        if (definition.unit == GridUnitType::Pixel) {
            pixelWidth += definition.value;
        } else if (
            definition.unit == GridUnitType::Star) {
            columnStarWeight += definition.value;
        }
    }
    for (std::uint32_t index = 0U;
         index < rows; ++index) {
        const GridLength definition = RowAt(index);
        if (definition.unit == GridUnitType::Pixel) {
            pixelHeight += definition.value;
        } else if (
            definition.unit == GridUnitType::Star) {
            rowStarWeight += definition.value;
        }
    }

    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const EffectiveGridSpan rowPlacement =
            CoerceGridSpan(
                ChildRow(*child),
                ChildRowSpan(*child),
                rows);
        const EffectiveGridSpan columnPlacement =
            CoerceGridSpan(
                ChildColumn(*child),
                ChildColumnSpan(*child),
                columns);
        const std::uint32_t row =
            rowPlacement.index;
        const std::uint32_t column =
            columnPlacement.index;
        const std::uint32_t rowSpan =
            rowPlacement.span;
        const std::uint32_t columnSpan =
            columnPlacement.span;
        double fixedWidth = 0.0;
        double fixedHeight = 0.0;
        double spanColumnStarWeight = 0.0;
        double spanRowStarWeight = 0.0;
        bool autoWidth = false;
        bool autoHeight = false;
        for (std::uint32_t offset = 0U;
             offset < columnSpan; ++offset) {
            const GridLength definition =
                ColumnAt(column + offset);
            fixedWidth += definition.unit ==
                    GridUnitType::Pixel
                ? definition.value : 0.0;
            autoWidth = autoWidth ||
                definition.unit == GridUnitType::Auto;
            spanColumnStarWeight +=
                definition.unit == GridUnitType::Star
                ? definition.value : 0.0;
        }
        for (std::uint32_t offset = 0U;
             offset < rowSpan; ++offset) {
            const GridLength definition =
                RowAt(row + offset);
            fixedHeight += definition.unit ==
                    GridUnitType::Pixel
                ? definition.value : 0.0;
            autoHeight = autoHeight ||
                definition.unit == GridUnitType::Auto;
            spanRowStarWeight +=
                definition.unit == GridUnitType::Star
                ? definition.value : 0.0;
        }
        double childWidth = fixedWidth;
        if (spanColumnStarWeight > 0.0) {
            childWidth =
                availableSize.width < FiniteConstraintLimit &&
                    columnStarWeight > 0.0
                ? fixedWidth +
                    std::max(
                        0.0,
                        availableSize.width -
                            pixelWidth) *
                    spanColumnStarWeight /
                    columnStarWeight
                : Unconstrained;
        } else if (autoWidth) {
            childWidth = Unconstrained;
        }
        double childHeight = fixedHeight;
        if (spanRowStarWeight > 0.0) {
            childHeight =
                availableSize.height <
                        FiniteConstraintLimit &&
                    rowStarWeight > 0.0
                ? fixedHeight +
                    std::max(
                        0.0,
                        availableSize.height -
                            pixelHeight) *
                    spanRowStarWeight /
                    rowStarWeight
                : Unconstrained;
        } else if (autoHeight) {
            childHeight = Unconstrained;
        }
        const Size childAvailable{
            childWidth, childHeight};
        Base::Result<void> measured = MeasureChild(*child, childAvailable);
        if (!measured) return measured.GetStatus();
        const Size childDesired = child->DesiredSize();
        const double widthShare =
            std::max(0.0, childDesired.width - fixedWidth) /
            static_cast<double>(columnSpan);
        const double heightShare =
            std::max(0.0, childDesired.height - fixedHeight) /
            static_cast<double>(rowSpan);
        for (std::uint32_t offset = 0U;
             offset < columnSpan; ++offset) {
            if (ColumnAt(column + offset).unit !=
                GridUnitType::Pixel) {
                desiredColumns[column + offset] = std::max(
                    desiredColumns[column + offset],
                    widthShare);
            }
        }
        for (std::uint32_t offset = 0U;
             offset < rowSpan; ++offset) {
            if (RowAt(row + offset).unit !=
                GridUnitType::Pixel) {
                desiredRows[row + offset] = std::max(
                    desiredRows[row + offset],
                    heightShare);
            }
        }
    }

    double width = 0.0;
    double height = 0.0;
    for (double value : desiredColumns) width += value;
    for (double value : desiredRows) height += value;
    desiredColumns_ = std::move(desiredColumns);
    desiredRows_ = std::move(desiredRows);
    return Size{width, height};
}

Base::Result<Size> Grid::ArrangeOverride(Size finalSize) noexcept {
    Base::Vector<double> columns;
    Base::Vector<double> rows;
    Base::Result<void> resolved = ResolveTracks(
        {columns_.Data(), columns_.Size()},
        {desiredColumns_.Data(), desiredColumns_.Size()},
        finalSize.width, columns);
    if (!resolved) return resolved.GetStatus();
    resolved = ResolveTracks(
        {rows_.Data(), rows_.Size()},
        {desiredRows_.Data(), desiredRows_.Size()},
        finalSize.height, rows);
    if (!resolved) return resolved.GetStatus();

    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const EffectiveGridSpan rowPlacement =
            CoerceGridSpan(
                ChildRow(*child),
                ChildRowSpan(*child),
                rows.Size());
        const EffectiveGridSpan columnPlacement =
            CoerceGridSpan(
                ChildColumn(*child),
                ChildColumnSpan(*child),
                columns.Size());
        const std::uint32_t row =
            rowPlacement.index;
        const std::uint32_t column =
            columnPlacement.index;
        const std::uint32_t rowSpan =
            rowPlacement.span;
        const std::uint32_t columnSpan =
            columnPlacement.span;
        double x = 0.0;
        double y = 0.0;
        for (std::uint32_t index = 0U; index < column; ++index) x += columns[index];
        for (std::uint32_t index = 0U; index < row; ++index) y += rows[index];
        double width = 0.0;
        double height = 0.0;
        for (std::uint32_t offset = 0U;
             offset < columnSpan; ++offset) {
            width += columns[column + offset];
        }
        for (std::uint32_t offset = 0U;
             offset < rowSpan; ++offset) {
            height += rows[row + offset];
        }
        Base::Result<void> arranged = ArrangeChild(*child,
            {x, y, width, height});
        if (!arranged) return arranged.GetStatus();
    }
    return finalSize;
}

std::uint32_t Grid::ColumnCount() const noexcept {
    return columns_.Empty() ? 1U : columns_.Size();
}

std::uint32_t Grid::RowCount() const noexcept {
    return rows_.Empty() ? 1U : rows_.Size();
}

GridLength Grid::ColumnAt(std::uint32_t index) const noexcept {
    return columns_.Empty() ? GridLength::Star() : columns_[index];
}

GridLength Grid::RowAt(std::uint32_t index) const noexcept {
    return rows_.Empty() ? GridLength::Star() : rows_[index];
}

Base::Result<void> Grid::ValidateDefinitions(
    Base::Span<const GridLength> definitions) const noexcept {
    for (const GridLength& definition : definitions) {
        if (!std::isfinite(definition.value) || definition.value < 0.0 ||
            (definition.unit == GridUnitType::Star && definition.value <= 0.0)) {
            return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
                "Grid track definition is invalid");
        }
    }
    return {};
}

std::uint32_t Grid::ChildRow(const UIElement& child) const noexcept {
    return child.GetValueOr(RowProperty, 0U);
}

std::uint32_t Grid::ChildColumn(const UIElement& child) const noexcept {
    return child.GetValueOr(ColumnProperty, 0U);
}

std::uint32_t Grid::ChildRowSpan(
    const UIElement& child) const noexcept {
    return std::max(
        1U,
        child.GetValueOr(RowSpanProperty, 1U));
}

std::uint32_t Grid::ChildColumnSpan(
    const UIElement& child) const noexcept {
    return std::max(
        1U,
        child.GetValueOr(ColumnSpanProperty, 1U));
}

Base::Result<void> Grid::ResolveTracks(
    Base::Span<const GridLength> definitions,
    Base::Span<const double> desired,
    double available,
    Base::Vector<double>& resolved) const noexcept {
    const std::uint32_t count = definitions.Empty() ? 1U : definitions.Size();
    if (!std::isfinite(available) || available < 0.0 ||
        (!desired.Empty() && desired.Size() != count)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Grid track resolution input is invalid");
    }
    Base::Result<void> resized = resolved.TryResize(count, 0.0);
    if (!resized) return resized.GetStatus();
    double occupied = 0.0;
    double totalStarWeight = 0.0;
    for (std::uint32_t index = 0U; index < count; ++index) {
        const GridLength definition = definitions.Empty()
            ? GridLength::Star() : definitions[index];
        if (definition.unit == GridUnitType::Pixel) {
            resolved[index] = definition.value;
            occupied += definition.value;
        } else if (definition.unit == GridUnitType::Auto) {
            resolved[index] = desired.Empty() ? 0.0 : desired[index];
            occupied += resolved[index];
        } else {
            totalStarWeight += definition.value;
        }
    }
    const double remaining = std::max(0.0, available - occupied);
    if (totalStarWeight > 0.0) {
        for (std::uint32_t index = 0U; index < count; ++index) {
            const GridLength definition = definitions.Empty()
                ? GridLength::Star() : definitions[index];
            if (definition.unit == GridUnitType::Star) {
                resolved[index] = remaining *
                    (definition.value / totalStarWeight);
            }
        }
    }
    return {};
}

Stretch Viewbox::GetStretch() const noexcept {
    return GetValueOr(StretchProperty, Stretch::Uniform);
}

StretchDirection
Viewbox::GetStretchDirection() const noexcept {
    return GetValueOr(
        StretchDirectionProperty,
        StretchDirection::Both);
}

Base::Result<void> Viewbox::SetStretch(
    Stretch value) noexcept {
    return SetValue(StretchProperty, value);
}

Base::Result<void> Viewbox::SetStretchDirection(
    StretchDirection value) noexcept {
    return SetValue(StretchDirectionProperty, value);
}

Base::Result<Size> Viewbox::MeasureOverride(
    Size availableSize) noexcept {
    UIElement* child = Child();
    if (child == nullptr) {
        if (!LayoutChildren().Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Viewbox must have at most one child");
        }
        return Size{};
    }

    // The layout kernel keeps all constraints finite. A large finite measure
    // gives Viewbox content its natural size while preserving that invariant.
    constexpr double NaturalConstraint = 1.0e12;
    Base::Result<void> measured = MeasureChild(
        *child,
        {NaturalConstraint, NaturalConstraint});
    if (!measured) return measured.GetStatus();

    const Size natural = child->DesiredSize();
    if (natural.width <= 0.0 || natural.height <= 0.0) {
        return Size{};
    }

    double scaleX = availableSize.width / natural.width;
    double scaleY = availableSize.height / natural.height;
    switch (GetStretch()) {
    case Stretch::None:
        scaleX = 1.0;
        scaleY = 1.0;
        break;
    case Stretch::Uniform: {
        const double scale = std::min(scaleX, scaleY);
        scaleX = scale;
        scaleY = scale;
        break;
    }
    case Stretch::UniformToFill: {
        const double scale = std::max(scaleX, scaleY);
        scaleX = scale;
        scaleY = scale;
        break;
    }
    case Stretch::Fill:
        break;
    }

    const StretchDirection direction =
        GetStretchDirection();
    if (direction == StretchDirection::UpOnly) {
        scaleX = std::max(1.0, scaleX);
        scaleY = std::max(1.0, scaleY);
    } else if (
        direction == StretchDirection::DownOnly) {
        scaleX = std::min(1.0, scaleX);
        scaleY = std::min(1.0, scaleY);
    }
    return Size{
        natural.width * scaleX,
        natural.height * scaleY};
}

Base::Result<void> Viewbox::ApplyViewTransform(
    double scaleX,
    double scaleY,
    double offsetX,
    double offsetY) noexcept {
    UIElement* child = Child();
    if (child == nullptr) {
        viewTransform_.Reset();
        return {};
    }
    Base::Ref<Transform> current =
        child->RenderTransform();
    if (current &&
        (!viewTransform_ ||
         current.Get() != viewTransform_.Get())) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Viewbox cannot combine its scale with a child RenderTransform");
    }
    if (!viewTransform_) {
        Base::Result<Base::Ref<MatrixTransform>> made =
            Base::MakeRef<MatrixTransform>();
        if (!made) return made.GetStatus();
        viewTransform_ = std::move(made).Value();
        Base::Result<void> assigned =
            child->SetRenderTransform(viewTransform_);
        if (!assigned) {
            viewTransform_.Reset();
            return assigned.GetStatus();
        }
    }

    Base::Transform2D matrix;
    matrix.m11 = scaleX;
    matrix.m22 = scaleY;
    matrix.dx = offsetX;
    matrix.dy = offsetY;
    return viewTransform_->SetValue(matrix);
}

Base::Result<Size> Viewbox::ArrangeOverride(
    Size finalSize) noexcept {
    UIElement* child = Child();
    if (child == nullptr) {
        Base::Result<void> reset =
            ApplyViewTransform(1.0, 1.0, 0.0, 0.0);
        if (!reset) return reset.GetStatus();
        return finalSize;
    }

    const Size natural = child->DesiredSize();
    if (natural.width <= 0.0 || natural.height <= 0.0) {
        Base::Result<void> arranged = ArrangeChild(
            *child, {0.0, 0.0, 0.0, 0.0});
        if (!arranged) return arranged.GetStatus();
        Base::Result<void> reset =
            ApplyViewTransform(1.0, 1.0, 0.0, 0.0);
        if (!reset) return reset.GetStatus();
        return finalSize;
    }

    double scaleX = finalSize.width / natural.width;
    double scaleY = finalSize.height / natural.height;
    switch (GetStretch()) {
    case Stretch::None:
        scaleX = 1.0;
        scaleY = 1.0;
        break;
    case Stretch::Uniform: {
        const double scale = std::min(scaleX, scaleY);
        scaleX = scale;
        scaleY = scale;
        break;
    }
    case Stretch::UniformToFill: {
        const double scale = std::max(scaleX, scaleY);
        scaleX = scale;
        scaleY = scale;
        break;
    }
    case Stretch::Fill:
        break;
    }

    const StretchDirection direction =
        GetStretchDirection();
    if (direction == StretchDirection::UpOnly) {
        scaleX = std::max(1.0, scaleX);
        scaleY = std::max(1.0, scaleY);
    } else if (
        direction == StretchDirection::DownOnly) {
        scaleX = std::min(1.0, scaleX);
        scaleY = std::min(1.0, scaleY);
    }

    const double renderedWidth =
        natural.width * scaleX;
    const double renderedHeight =
        natural.height * scaleY;
    const double offsetX =
        (finalSize.width - renderedWidth) * 0.5;
    const double offsetY =
        (finalSize.height - renderedHeight) * 0.5;
    Base::Result<void> arranged = ArrangeChild(
        *child,
        {offsetX, offsetY,
         natural.width, natural.height});
    if (!arranged) return arranged.GetStatus();

    Base::Result<void> transformed = ApplyViewTransform(
        scaleX,
        scaleY,
        0.0,
        0.0);
    if (!transformed) return transformed.GetStatus();
    return finalSize;
}

Border::Border() noexcept : Decorator(StaticTypeId()) {}

Color Border::Background() const noexcept {
    return SampleBrush(BackgroundBrush());
}

Base::Ref<Brush> Border::BackgroundBrush() const noexcept {
    return GetValueOr(
        BackgroundProperty, Base::Ref<Brush>{});
}

Color Border::BorderBrush() const noexcept {
    return SampleBrush(BorderBrushObject());
}

Base::Ref<Brush> Border::BorderBrushObject() const noexcept {
    return GetValueOr(
        BorderBrushProperty, Base::Ref<Brush>{});
}

Thickness Border::BorderThickness() const noexcept {
    return GetValueOr(
        BorderThicknessProperty, Thickness{});
}

CornerRadius Border::GetCornerRadius() const noexcept {
    return GetValueOr(
        CornerRadiusProperty, CornerRadius{});
}

Thickness Border::Padding() const noexcept {
    return GetValueOr(PaddingProperty, Thickness{});
}

Base::Result<void> Border::SetBackground(Color value) noexcept {
    Base::Result<Base::Ref<Brush>> brush =
        MakeSolidColorBrush(value);
    return brush
        ? SetBackgroundBrush(std::move(brush).Value())
        : brush.GetStatus();
}

Base::Result<void> Border::SetBackgroundBrush(
    Base::Ref<Brush> value) noexcept {
    return SetValue(
        BackgroundProperty, std::move(value));
}

Base::Result<void> Border::SetStroke(Color value, double thickness) noexcept {
    Base::Result<void> brush = SetBorderBrush(value);
    return brush ? SetBorderThickness(thickness) : brush;
}

Base::Result<void> Border::SetBorderBrush(Color value) noexcept {
    Base::Result<Base::Ref<Brush>> brush =
        MakeSolidColorBrush(value);
    return brush
        ? SetBorderBrushObject(std::move(brush).Value())
        : brush.GetStatus();
}

Base::Result<void> Border::SetBorderBrushObject(
    Base::Ref<Brush> value) noexcept {
    return SetValue(
        BorderBrushProperty, std::move(value));
}

Base::Result<void> Border::SetBorderThickness(
    Thickness value) noexcept {
    return SetValue(BorderThicknessProperty, value);
}

Base::Result<void> Border::SetBorderThickness(
    double value) noexcept {
    return SetBorderThickness(
        {value, value, value, value});
}

Base::Result<void> Border::SetCornerRadius(
    CornerRadius value) noexcept {
    return SetValue(CornerRadiusProperty, value);
}

Base::Result<void> Border::SetCornerRadius(
    double value) noexcept {
    return SetCornerRadius(
        {value, value, value, value});
}

Base::Result<void> Border::SetPadding(Thickness value) noexcept {
    return SetValue(PaddingProperty, value);
}

Base::Result<Size> Border::MeasureOverride(Size availableSize) noexcept {
    const Thickness border = BorderThickness();
    const Thickness padding = Padding();
    const Thickness chrome{
        border.left + padding.left,
        border.top + padding.top,
        border.right + padding.right,
        border.bottom + padding.bottom};
    UIElement* child = Child();
    if (child == nullptr) return Inflate({}, chrome);
    Base::Result<void> measured = MeasureChild(
        *child, Deflate(availableSize, chrome));
    if (!measured) return measured.GetStatus();
    return Inflate(child->DesiredSize(), chrome);
}

Base::Result<Size> Border::ArrangeOverride(Size finalSize) noexcept {
    UIElement* child = Child();
    if (child == nullptr) return finalSize;
    const Thickness border = BorderThickness();
    const Thickness padding = Padding();
    const Thickness chrome{
        border.left + padding.left,
        border.top + padding.top,
        border.right + padding.right,
        border.bottom + padding.bottom};
    const Size childSize = Deflate(finalSize, chrome);
    Base::Result<void> arranged = ArrangeChild(*child,
        {chrome.left, chrome.top,
         childSize.width, childSize.height});
    if (!arranged) return arranged.GetStatus();
    return finalSize;
}

Base::Result<void> Border::BuildDisplayList(
    DisplayListBuilder& builder) noexcept {
    const Rect bounds{0.0, 0.0, RenderSize().width, RenderSize().height};
    if (bounds.width <= 0.0 ||
        bounds.height <= 0.0) {
        return {};
    }
    const CornerRadius radii = GetCornerRadius();
    const double radius = std::min(
        std::max(
            std::max(
                radii.topLeft,
                radii.topRight),
            std::max(
                radii.bottomRight,
                radii.bottomLeft)),
        std::min(
            bounds.width,
            bounds.height) * 0.5);
    const Color brush = BorderBrush();
    const Thickness thickness = BorderThickness();
    const bool uniform =
        thickness.left == thickness.top &&
        thickness.left == thickness.right &&
        thickness.left == thickness.bottom;
    const double uniformThickness =
        uniform ? thickness.left : 0.0;
    if (radius > 0.0 && uniformThickness > 0.0 &&
        brush.alpha > 0.0F) {
        Base::Result<void> border =
            builder.FillRoundedRect(
                bounds, brush, radius);
        if (!border) return border.GetStatus();
        const double inset = std::min(
            uniformThickness,
            std::min(
                bounds.width * 0.5,
                bounds.height * 0.5));
        const Rect inner{
            inset,
            inset,
            std::max(0.0, bounds.width -
                inset * 2.0),
            std::max(0.0, bounds.height -
                inset * 2.0)};
        if (inner.width <= 0.0 ||
            inner.height <= 0.0) {
            return {};
        }
        return builder.FillRoundedRect(
            inner,
            Background(),
            std::min(
                std::max(0.0, radius - inset),
                std::min(
                    inner.width,
                    inner.height) * 0.5));
    }
    Base::Result<void> fill = radius > 0.0
        ? builder.FillRoundedRect(
              bounds, Background(), radius)
        : builder.FillRect(bounds, Background());
    if (!fill) return fill.GetStatus();
    if (uniformThickness > 0.0 &&
        brush.alpha > 0.0F) {
        return builder.StrokeRect(
            bounds, brush, uniformThickness);
    }
    if (!uniform && brush.alpha > 0.0F) {
        const auto fillSide =
            [&](Rect side) noexcept -> Base::Result<void> {
                return side.width > 0.0 && side.height > 0.0
                    ? builder.FillRect(side, brush)
                    : Base::Result<void>();
            };
        Base::Result<void> side = fillSide({
            0.0, 0.0,
            std::min(bounds.width, thickness.left),
            bounds.height});
        if (!side) return side;
        side = fillSide({
            std::max(0.0, bounds.width - thickness.right),
            0.0,
            std::min(bounds.width, thickness.right),
            bounds.height});
        if (!side) return side;
        side = fillSide({
            0.0, 0.0,
            bounds.width,
            std::min(bounds.height, thickness.top)});
        if (!side) return side;
        return fillSide({
            0.0,
            std::max(0.0, bounds.height - thickness.bottom),
            bounds.width,
            std::min(bounds.height, thickness.bottom)});
    }
    return {};
}

TextBlock::TextBlock() noexcept
    : TextBlock(StaticTypeId()) {}

TextBlock::TextBlock(TypeId runtimeType) noexcept
    : FrameworkElement(runtimeType),
      layoutService_(nullptr),
      textHitRegions_(),
      ownedInlines_(),
      pendingInline_() {}

TextBlock::~TextBlock() {
    ReleaseServiceGlyphRun();
}

Base::StringView TextBlock::Text() const noexcept {
    return GetValueOr(TextProperty, Base::StringView());
}

Color TextBlock::Foreground() const noexcept {
    return SampleBrush(
        ForegroundBrush(),
        0.5,
        Color{0.0F, 0.0F, 0.0F, 1.0F});
}

Base::Ref<Brush> TextBlock::ForegroundBrush() const noexcept {
    Base::Ref<Brush> brush = GetValueOr(
        ForegroundProperty, Base::Ref<Brush>{});
    const FrameworkElement* parent =
        RenderParent();
    while (!brush && parent != nullptr) {
        brush = parent->GetValueOr(
            ForegroundProperty,
            Base::Ref<Brush>{});
        parent = parent->RenderParent();
    }
    return brush;
}

Color TextBlock::Background() const noexcept {
    return SampleBrush(BackgroundBrush());
}

Base::Ref<Brush> TextBlock::BackgroundBrush() const noexcept {
    return GetValueOr(
        BackgroundProperty, Base::Ref<Brush>{});
}

double TextBlock::FontSize() const noexcept {
    return GetValueOr(FontSizeProperty, 16.0);
}

Base::StringView TextBlock::FontFamily() const noexcept {
    return FrameworkElement::FontFamily();
}

FontWeight TextBlock::GetFontWeight() const noexcept {
    if (RuntimeType() == Documents::Bold::StaticTypeId()) {
        return FontWeight::Bold;
    }
    return GetValueOr(
        FontWeightProperty,
        FontWeight::Normal);
}

Text::FontStyle TextBlock::GetFontStyle() const noexcept {
    if (RuntimeType() == Documents::Italic::StaticTypeId()) {
        return Text::FontStyle::Italic;
    }
    return GetValueOr(
        FontStyleProperty,
        Text::FontStyle::Normal);
}

TextDecorations TextBlock::GetTextDecorations() const noexcept {
    if (RuntimeType() == Documents::Underline::StaticTypeId()) {
        return TextDecorations::Underline;
    }
    return GetValueOr(
        TextDecorationsProperty,
        TextDecorations::None);
}

Text::TextWrapping TextBlock::TextWrapping() const noexcept {
    return GetValueOr(
        TextWrappingProperty,
        Text::TextWrapping::NoWrap);
}

Text::TextTrimming TextBlock::TextTrimming() const noexcept {
    return GetValueOr(
        TextTrimmingProperty,
        Text::TextTrimming::None);
}

Text::TextAlignment TextBlock::TextAlignment() const noexcept {
    return GetValueOr(
        TextAlignmentProperty,
        Text::TextAlignment::Start);
}

Base::Result<void> TextBlock::SetText(Base::StringView value) noexcept {
    Base::Result<void> changed = SetValue(TextProperty, value);
    if (!changed) return changed.GetStatus();
    textHitRegions_.Clear();
    return CoerceDocumentSelection();
}

bool TextBlock::IsTextSelectionEnabled() const noexcept {
    return GetValueOr(IsTextSelectionEnabledProperty, false);
}

Color TextBlock::SelectionBrush() const noexcept {
    return GetValueOr(SelectionBrushProperty,
        Color{46.0F / 255.0F, 174.0F / 255.0F,
              235.0F / 255.0F, 1.0F});
}

double TextBlock::SelectionOpacity() const noexcept {
    return GetValueOr(SelectionOpacityProperty, 0.25);
}

Color TextBlock::CaretBrush() const noexcept {
    return GetValueOr(CaretBrushProperty,
        Color{0.0F, 0.0F, 0.0F, 1.0F});
}

Base::Result<void> TextBlock::SetTextSelectionEnabled(bool value) noexcept {
    return SetValue(IsTextSelectionEnabledProperty, value);
}

Base::Result<void> TextBlock::SetSelectionBrush(Color value) noexcept {
    return SetValue(SelectionBrushProperty, value);
}

Base::Result<void> TextBlock::SetSelectionOpacity(double value) noexcept {
    return SetValue(SelectionOpacityProperty, value);
}

Base::Result<void> TextBlock::SetCaretBrush(Color value) noexcept {
    return SetValue(CaretBrushProperty, value);
}

Base::Result<void> TextBlock::SetForeground(Color value) noexcept {
    Base::Result<Base::Ref<Brush>> brush =
        MakeSolidColorBrush(value);
    return brush
        ? SetForegroundBrush(std::move(brush).Value())
        : brush.GetStatus();
}

Base::Result<void> TextBlock::SetBackground(Color value) noexcept {
    Base::Result<Base::Ref<Brush>> brush =
        MakeSolidColorBrush(value);
    return brush
        ? SetBackgroundBrush(std::move(brush).Value())
        : brush.GetStatus();
}

Base::Result<void> TextBlock::SetForegroundBrush(
    Base::Ref<Brush> value) noexcept {
    return SetValue(
        ForegroundProperty, std::move(value));
}

Base::Result<void> TextBlock::SetBackgroundBrush(
    Base::Ref<Brush> value) noexcept {
    return SetValue(
        BackgroundProperty, std::move(value));
}

Base::Result<void> TextBlock::SetFontSize(double value) noexcept {
    return SetValue(FontSizeProperty, value);
}

Base::Result<void> TextBlock::SetFontFamily(
    Base::StringView value) noexcept {
    return SetValue(FontFamilyProperty, value);
}

Base::Result<void> TextBlock::SetFontWeight(
    FontWeight value) noexcept {
    return SetValue(FontWeightProperty, value);
}

Base::Result<void> TextBlock::SetFontStyle(
    Text::FontStyle value) noexcept {
    return SetValue(FontStyleProperty, value);
}

Base::Result<void> TextBlock::SetTextDecorations(
    TextDecorations value) noexcept {
    return SetValue(TextDecorationsProperty, value);
}

Base::Result<void> TextBlock::SetTextWrapping(
    Text::TextWrapping value) noexcept {
    return SetValue(TextWrappingProperty, value);
}

Base::Result<void> TextBlock::SetTextTrimming(
    Text::TextTrimming value) noexcept {
    return SetValue(TextTrimmingProperty, value);
}

Base::Result<void> TextBlock::SetTextAlignment(
    Text::TextAlignment value) noexcept {
    return SetValue(TextAlignmentProperty, value);
}

Core::Value TextBlock::MetadataInlines() const noexcept {
    if (pendingInline_) {
        return Core::Value::FromObject(
            pendingInline_->RuntimeType(),
            pendingInline_);
    }
    return Core::Value::NullObject(
        Core::TypeOf<Base::Object>());
}

Base::Result<void> TextBlock::SetInlineValue(
    Core::Value value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (value.Kind() == Core::ValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject()) {
        Base::Ref<Base::Object> inlineObject =
            value.AsObject();
        if (!PropertyRegistry().Types().IsDerivedFrom(
                inlineObject->RuntimeType(),
                UIElement::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "TextBlock inline object must be a UIElement");
        }
        return AddOwnedInline(
            inlineObject,
            *static_cast<UIElement*>(
                inlineObject.Get()));
    }
    if (value.Kind() != Core::ValueKind::String) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBlock inline content must be text or an object");
    }
    Base::Result<Base::Ref<Documents::Run>> created =
        Base::MakeRef<Documents::Run>();
    if (!created) return created.GetStatus();
    Base::Result<void> text =
        created.Value()->SetText(value.AsString());
    if (!text) return text.GetStatus();
    pendingInline_ = Base::Ref<Base::Object>(
        created.Value());
    return {};
}

Base::Result<void> TextBlock::AddOwnedInline(
    const Base::Ref<Base::Object>& inlineObject,
    UIElement& inlineElement) noexcept {
    if (!inlineObject ||
        inlineObject.Get() != &inlineElement) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBlock owned inline does not match its UIElement");
    }
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    const TypeRegistry& types = PropertyRegistry().Types();
    const TypeId type = inlineObject->RuntimeType();
    const bool supported = types.IsDerivedFrom(
        type, Documents::Inline::StaticTypeId());
    if (!supported) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBlock content must derive from Documents::Inline");
    }
    for (const Base::Ref<Base::Object>& owned :
         ownedInlines_) {
        if (owned.Get() == inlineObject.Get()) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "TextBlock already owns the inline");
        }
    }
    Base::Result<void> appended =
        ownedInlines_.TryPushBack(inlineObject);
    if (!appended) return appended.GetStatus();
    pendingInline_ = inlineObject;
    return InvalidateMeasure();
}

Base::Result<void> TextBlock::ClearOwnedInlines() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (!LayoutChildren().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TextBlock inlines must be detached before releasing ownership");
    }
    ownedInlines_.Clear();
    pendingInline_.Reset();
    return InvalidateMeasure();
}

Base::StringView TextBlock::EffectiveFontFamily() const noexcept {
    const Base::StringView configured = FontFamily();
    const bool defaultFamily =
        configured.Empty() ||
        configured == Base::StringView("Segoe UI");
    if (!defaultFamily) return configured;
    const bool bold =
        GetFontWeight() == FontWeight::Bold ||
        GetFontWeight() == FontWeight::SemiBold;
    const bool italic =
        GetFontStyle() != Text::FontStyle::Normal;
    if (bold && italic) {
        return Base::StringView("Segoe UI Bold Italic");
    }
    if (bold) return Base::StringView("Segoe UI Bold");
    if (italic) return Base::StringView("Segoe UI Italic");
    return configured;
}

bool TextBlock::IsLineBreak(
    const UIElement& child) const noexcept {
    return child.RuntimeType() ==
        Documents::LineBreak::StaticTypeId();
}

Base::Result<void> TextBlock::SynchronizeInlineStyle(
    UIElement& child) noexcept {
    if (!PropertyRegistry().Types().IsDerivedFrom(
            child.RuntimeType(),
            TextBlock::StaticTypeId())) {
        return {};
    }
    auto& text = static_cast<TextBlock&>(child);
    const auto syncBrush = [&](
        const auto& property,
        Base::Ref<Brush> value) noexcept -> Base::Result<void> {
        Base::Result<EffectiveValueSource> source =
            text.GetValueSource(property.Handle());
        if (!source) return source.GetStatus();
        return source.Value() == EffectiveValueSource::Local
            ? Base::Result<void>()
            : text.SetCurrentValue(property, value);
    };
    const auto syncDouble = [&](
        const auto& property,
        double value) noexcept -> Base::Result<void> {
        Base::Result<EffectiveValueSource> source =
            text.GetValueSource(property.Handle());
        if (!source) return source.GetStatus();
        return source.Value() == EffectiveValueSource::Local
            ? Base::Result<void>()
            : text.SetCurrentValue(property, value);
    };
    Base::Result<void> result =
        syncBrush(
            ForegroundProperty,
            ForegroundBrush());
    if (result) {
        result = syncDouble(
            FontSizeProperty,
            FontSize());
    }
    if (result) {
        Base::Result<EffectiveValueSource> source =
            text.GetValueSource(
                FrameworkElement::FontFamilyProperty.Handle());
        if (!source) {
            result = source.GetStatus();
        } else if (source.Value() !=
                   EffectiveValueSource::Local) {
            Base::Result<Core::Value> encoded =
                Core::Value::TryFromString(
                    Core::TypeOf<Base::String>(),
                    FontFamily());
            result = encoded
                ? text.SetCurrentValue(
                    FrameworkElement::FontFamilyProperty.Handle(),
                    encoded.Value())
                : Base::Result<void>(encoded.GetStatus());
        }
    }
    if (!result) return result.GetStatus();
    Base::Result<EffectiveValueSource> wrappingSource =
        text.GetValueSource(
            TextWrappingProperty.Handle());
    if (!wrappingSource) {
        return wrappingSource.GetStatus();
    }
    if (wrappingSource.Value() !=
        EffectiveValueSource::Local) {
        result = text.SetCurrentValue(
            TextWrappingProperty,
            TextWrapping());
        if (!result) return result.GetStatus();
    }
    Base::Result<EffectiveValueSource> trimmingSource =
        text.GetValueSource(
            TextTrimmingProperty.Handle());
    if (!trimmingSource) {
        return trimmingSource.GetStatus();
    }
    if (trimmingSource.Value() !=
        EffectiveValueSource::Local) {
        result = text.SetCurrentValue(
            TextTrimmingProperty,
            TextTrimming());
        if (!result) return result.GetStatus();
    }

    Base::Result<EffectiveValueSource> weightSource =
        text.GetValueSource(
            FontWeightProperty.Handle());
    if (!weightSource) return weightSource.GetStatus();
    if (weightSource.Value() !=
            EffectiveValueSource::Local &&
        child.RuntimeType() != Documents::Bold::StaticTypeId()) {
        result = text.SetCurrentValue(
            FontWeightProperty,
            GetFontWeight());
        if (!result) return result.GetStatus();
    }
    Base::Result<EffectiveValueSource> styleSource =
        text.GetValueSource(
            FontStyleProperty.Handle());
    if (!styleSource) return styleSource.GetStatus();
    if (styleSource.Value() !=
            EffectiveValueSource::Local &&
        child.RuntimeType() != Documents::Italic::StaticTypeId()) {
        result = text.SetCurrentValue(
            FontStyleProperty,
            GetFontStyle());
        if (!result) return result.GetStatus();
    }
    Base::Result<EffectiveValueSource> decorationSource =
        text.GetValueSource(
            TextDecorationsProperty.Handle());
    if (!decorationSource) {
        return decorationSource.GetStatus();
    }
    if (decorationSource.Value() !=
            EffectiveValueSource::Local &&
        child.RuntimeType() !=
            Documents::Underline::StaticTypeId()) {
        result = text.SetCurrentValue(
            TextDecorationsProperty,
            GetTextDecorations());
    }
    return result;
}

Base::Result<void> TextBlock::SetGlyphRun(
    RenderGlyphRunId glyphRun, Size size) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (!IsValidTextSize(size) ||
        (glyphRun == InvalidRenderGlyphRunId &&
            (size.width != 0.0 || size.height != 0.0))) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "TextBlock glyph run and size are invalid");
    }
    if (glyphRuns_.Size() == 1U && glyphRuns_[0] == glyphRun &&
        glyphRunSize_.width == size.width &&
        glyphRunSize_.height == size.height &&
        !serviceOwnsGlyphRun_) return {};
    ReleaseServiceGlyphRun();
    glyphRuns_.Clear();
    textHitRegions_.Clear();
    if (glyphRun != InvalidRenderGlyphRunId) {
        Base::Result<void> appended =
            glyphRuns_.TryPushBack(glyphRun);
        if (!appended) return appended.GetStatus();
    }
    glyphRunSize_ = size;
    Base::Result<void> measure = InvalidateMeasure();
    if (!measure) return measure.GetStatus();
    return InvalidateRender();
}

Base::Result<Size> TextBlock::MeasureOverride(Size availableSize) noexcept {
    Base::Result<void> selection = CoerceDocumentSelection();
    if (!selection) return selection.GetStatus();
    if (layoutService_ != nullptr) {
        const Base::StringView text = Text();
        if (text.Empty()) {
            const bool changed =
                !glyphRuns_.Empty() ||
                glyphRunSize_.width != 0.0 ||
                glyphRunSize_.height != 0.0;
            ReleaseServiceGlyphRun();
            glyphRuns_.Clear();
            glyphRunSize_ = {};
            textHitRegions_.Clear();
            if (changed) {
                Base::Result<void> invalidated = InvalidateRender();
                if (!invalidated) return invalidated.GetStatus();
            }
        } else {
        Detail::TextLayoutRequest request;
        request.text = text;
        request.availableSize = availableSize;
        request.dpiScale = DpiScale();
        request.pixelSize =
            static_cast<float>(FontSize());
        request.fontFamily = EffectiveFontFamily();
        request.wrapping = TextWrapping();
        request.trimming = TextTrimming();
        request.alignment = TextAlignment();
        Detail::TextLayoutResult output;
        Base::Result<void> prepared =
            layoutService_->ShapeAndPrepare(request, output);
        if (!prepared) return prepared.GetStatus();
        bool validGlyphRuns = true;
        for (RenderGlyphRunId glyphRun : output.glyphRuns) {
            if (glyphRun == InvalidRenderGlyphRunId) {
                validGlyphRuns = false;
                break;
            }
        }
        if (!IsValidTextSize(output.desiredSize) ||
            !validGlyphRuns) {
            for (RenderGlyphRunId glyphRun : output.glyphRuns) {
                if (glyphRun != InvalidRenderGlyphRunId) {
                    layoutService_->ReleaseGlyphRun(glyphRun);
                }
            }
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Text layout service returned an invalid result");
        }

        bool changed =
            glyphRuns_.Size() != output.glyphRuns.Size() ||
            glyphRunSize_.width != output.desiredSize.width ||
            glyphRunSize_.height != output.desiredSize.height ||
            !serviceOwnsGlyphRun_;
        if (!changed) {
            for (std::uint32_t index = 0U;
                 index < glyphRuns_.Size(); ++index) {
                if (glyphRuns_[index] != output.glyphRuns[index]) {
                    changed = true;
                    break;
                }
            }
        }
        ReleaseServiceGlyphRun();
        glyphRuns_ = std::move(output.glyphRuns);
        textHitRegions_ = std::move(output.hitRegions);
        glyphRunSize_ = output.desiredSize;
        serviceOwnsGlyphRun_ = !glyphRuns_.Empty();
        if (changed) {
            Base::Result<void> invalidated = InvalidateRender();
            if (!invalidated) return invalidated.GetStatus();
        }
        }
    }
    double lineWidth = glyphRunSize_.width;
    double lineHeight = glyphRunSize_.height;
    double desiredWidth = lineWidth;
    double desiredHeight = 0.0;
    const bool wrapping =
        TextWrapping() != Text::TextWrapping::NoWrap;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        if (IsLineBreak(*child)) {
            desiredWidth =
                std::max(desiredWidth, lineWidth);
            desiredHeight += std::max(
                lineHeight,
                FontSize() * 1.2);
            lineWidth = 0.0;
            lineHeight = 0.0;
            continue;
        }
        Base::Result<void> synchronized =
            SynchronizeInlineStyle(*child);
        if (!synchronized) {
            return synchronized.GetStatus();
        }
        Size childAvailable = availableSize;
        if (!wrapping) {
            childAvailable.width = 1.0e12;
        } else {
            childAvailable.width =
                std::max(
                    0.0,
                    availableSize.width -
                        lineWidth);
        }
        Base::Result<void> measured =
            MeasureChild(*child, childAvailable);
        if (!measured) return measured.GetStatus();
        Size childDesired = child->DesiredSize();
        if (wrapping &&
            lineWidth > 0.0 &&
            lineWidth + childDesired.width >
                availableSize.width) {
            desiredWidth =
                std::max(desiredWidth, lineWidth);
            desiredHeight += std::max(
                lineHeight,
                FontSize() * 1.2);
            lineWidth = 0.0;
            lineHeight = 0.0;
            childAvailable.width =
                availableSize.width;
            measured = MeasureChild(
                *child, childAvailable);
            if (!measured) {
                return measured.GetStatus();
            }
            childDesired = child->DesiredSize();
        }
        lineWidth += childDesired.width;
        lineHeight =
            std::max(lineHeight, childDesired.height);
    }
    desiredWidth =
        std::max(desiredWidth, lineWidth);
    desiredHeight += lineHeight;
    return Size{
        std::min(desiredWidth, availableSize.width),
        std::min(desiredHeight, availableSize.height)};
}

Base::Result<Size> TextBlock::ArrangeOverride(
    Size finalSize) noexcept {
    double x = glyphRunSize_.width;
    double y = 0.0;
    double lineHeight = glyphRunSize_.height;
    const bool wrapping =
        TextWrapping() != Text::TextWrapping::NoWrap;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        if (IsLineBreak(*child)) {
            y += std::max(
                lineHeight,
                FontSize() * 1.2);
            x = 0.0;
            lineHeight = 0.0;
            Base::Result<void> arranged =
                ArrangeChild(*child, {x, y, 0.0, 0.0});
            if (!arranged) return arranged.GetStatus();
            continue;
        }
        const Size desired = child->DesiredSize();
        if (wrapping && x > 0.0 &&
            x + desired.width > finalSize.width) {
            y += std::max(
                lineHeight,
                FontSize() * 1.2);
            x = 0.0;
            lineHeight = 0.0;
        }
        Base::Result<void> arranged =
            ArrangeChild(
                *child,
                {x, y, desired.width, desired.height});
        if (!arranged) return arranged.GetStatus();
        x += desired.width;
        lineHeight =
            std::max(lineHeight, desired.height);
    }
    return finalSize;
}

Base::Result<void> TextBlock::BuildDisplayList(
    DisplayListBuilder& builder) noexcept {
    const Color background = Background();
    if (background.alpha > 0.0F) {
        Base::Result<void> filled = builder.FillRect(
            {0.0, 0.0, RenderSize().width, RenderSize().height},
            background);
        if (!filled) return filled.GetStatus();
    }
    const Documents::TextSelection selection = Selection();
    if (IsTextSelectionEnabled() && !selection.IsEmpty()) {
        Base::Result<Documents::TextRange> range = selection.Range();
        if (!range) return range.GetStatus();
        Base::Vector<Rect> rectangles;
        Base::Result<void> geometry =
            Documents::GetTextRangeRectangles(range.Value(), rectangles);
        if (!geometry) return geometry.GetStatus();
        Color selectionColor = SelectionBrush();
        selectionColor.alpha *= static_cast<float>(SelectionOpacity());
        for (const Rect& rect : rectangles) {
            Base::Result<void> filled = builder.FillRect(rect, selectionColor);
            if (!filled) return filled.GetStatus();
        }
    }
    for (RenderGlyphRunId glyphRun : glyphRuns_) {
        Base::Result<void> drawn =
            builder.DrawGlyphRun(glyphRun, Foreground());
        if (!drawn) return drawn.GetStatus();
    }
    if (GetTextDecorations() == TextDecorations::Underline &&
        glyphRunSize_.width > 0.0) {
        const double thickness = std::max(1.0, FontSize() * 0.06);
        const double y = std::max(
            0.0, glyphRunSize_.height - thickness * 1.5);
        Base::Result<void> line = builder.FillRect(
            {0.0, y, glyphRunSize_.width, thickness}, Foreground());
        if (!line) return line.GetStatus();
    }
    if (IsTextSelectionEnabled() && IsKeyboardFocused() &&
        selection.IsEmpty() && caretBlinkVisible_) {
        Base::Result<Rect> caret = CaretRectangle();
        if (!caret) return caret.GetStatus();
        Base::Result<void> drawn = builder.FillRect(caret.Value(), CaretBrush());
        if (!drawn) return drawn.GetStatus();
    }
    return {};
}

void TextBlock::ReleaseServiceGlyphRun() noexcept {
    if (serviceOwnsGlyphRun_ &&
        layoutService_ != nullptr) {
        for (RenderGlyphRunId glyphRun : glyphRuns_) {
            layoutService_->ReleaseGlyphRun(glyphRun);
        }
    }
    serviceOwnsGlyphRun_ = false;
}

ContentPresenter::ContentPresenter() noexcept
    : FrameworkElement(StaticTypeId()) {}

void ContentPresenter::OnContentPropertyChanged(
    Core::DependencyObject& object,
    const Core::DependencyPropertyChangedEventArgs&
        change) noexcept {
    auto& presenter =
        static_cast<ContentPresenter&>(object);
    presenter.contentValue_ = change.newValue;
    static_cast<void>(
        presenter.UpdatePresentedText());
}

Base::Result<void>
ContentPresenter::UpdatePresentedText() noexcept {
    if (content_ == nullptr ||
        !PropertyRegistry().Types().IsDerivedFrom(
            content_->RuntimeType(),
            TextBlock::StaticTypeId())) {
        return {};
    }
    Base::String text;
    switch (contentValue_.Kind()) {
    case Core::ValueKind::String:
        {
            Base::Result<void> assigned =
                text.TryAssign(
                    contentValue_.AsString());
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        break;
    case Core::ValueKind::Boolean:
        {
            Base::Result<void> assigned =
                text.TryAssign(
                    contentValue_.AsBoolean()
                    ? Base::StringView("True")
                    : Base::StringView("False"));
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        break;
    case Core::ValueKind::SignedInteger:
    case Core::ValueKind::UnsignedInteger:
    case Core::ValueKind::Double:
        {
            char raw[64]{};
            if (contentValue_.Kind() ==
                Core::ValueKind::SignedInteger) {
                std::snprintf(
                    raw, sizeof(raw), "%lld",
                    static_cast<long long>(
                        contentValue_.
                            AsSignedInteger()));
            } else if (contentValue_.Kind() ==
                       Core::ValueKind::
                           UnsignedInteger) {
                std::snprintf(
                    raw, sizeof(raw), "%llu",
                    static_cast<
                        unsigned long long>(
                            contentValue_.
                                AsUnsignedInteger()));
            } else {
                std::snprintf(
                    raw, sizeof(raw), "%.15g",
                    contentValue_.AsDouble());
            }
            Base::Result<void> assigned =
                text.TryAssign(raw);
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        break;
    case Core::ValueKind::Object:
        if (!contentValue_.IsNullObject()) {
            return {};
        }
        break;
    default:
        return {};
    }
    return static_cast<TextBlock*>(
        content_)->SetText(text.View());
}

Base::Result<void> ContentPresenter::SetContentSource(
    Base::StringView value) noexcept {
    return SetValue(
        ContentSourceProperty, value);
}

bool ContentPresenter::IsOnlyAttachedContent(
    const UIElement& content) const noexcept {
    const UIElementChildRange children = LayoutChildren();
    return children.Size() == 1U && children[0] == &content;
}

Base::Result<void> ContentPresenter::SetContent(UIElement* content) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    Base::Result<void> validated = ValidateContent(content);
    if (!validated) return validated;
    if (content == content_) return {};
    content_ = content;
    if (content == nullptr) ownedContent_.Reset();
    return InvalidateMeasure();
}

Base::Result<void> ContentPresenter::SetOwnedContent(
    const Base::Ref<Base::Object>& contentObject,
    UIElement& content) noexcept {
    if (!contentObject || contentObject.Get() != &content) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "ContentPresenter owned content does not match its UIElement");
    }
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    Base::Result<void> validated = ValidateContent(&content);
    if (!validated) return validated;
    content_ = &content;
    ownedContent_ = contentObject;
    Base::Result<void> updated =
        UpdatePresentedText();
    return updated
        ? InvalidateMeasure()
        : updated;
}

Base::Result<void> ContentPresenter::ValidateContent(
    UIElement* content) const noexcept {
    if (content == nullptr) {
        if (!LayoutChildren().Empty()) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "ContentPresenter content must be detached before clearing it");
        }
    } else if (!LayoutChildren().Empty() && !IsOnlyAttachedContent(*content)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "ContentPresenter content must be its only attached layout child");
    }
    return {};
}

Base::Result<Size> ContentPresenter::MeasureOverride(
    Size availableSize) noexcept {
    if (content_ == nullptr) {
        if (!LayoutChildren().Empty()) {
            return Base::Status::Failure(Base::ErrorCode::InvalidState,
                "ContentPresenter has attached children without content");
        }
        return Size{};
    }
    if (!IsOnlyAttachedContent(*content_)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "ContentPresenter content attachment is invalid");
    }
    Base::Result<void> measured = MeasureChild(*content_, availableSize);
    if (!measured) return measured.GetStatus();
    return content_->DesiredSize();
}

Base::Result<Size> ContentPresenter::ArrangeOverride(Size finalSize) noexcept {
    if (content_ == nullptr) return finalSize;
    if (!IsOnlyAttachedContent(*content_)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "ContentPresenter content attachment is invalid");
    }
    Base::Result<void> arranged = ArrangeChild(*content_,
        {0.0, 0.0, finalSize.width, finalSize.height});
    if (!arranged) return arranged.GetStatus();
    return finalSize;
}

} // namespace Aero::Controls
