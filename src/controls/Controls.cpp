#include "gui/MetadataInternal.hpp"
#include "../render/DisplayList.hpp"
#include <Aero/Controls/Panels.hpp>
#include "../media/BrushRendering.hpp"
#include "../media/BrushInternals.hpp"
#include "../render/DrawingInternals.hpp"
#include <Aero/Documents.hpp>

#include "TextBlockLayout.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/ElementInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace Aero::Controls {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Render;

namespace {

bool IsValidTextSize(Size value) noexcept {
    return IsFinite(value) && value.width >= 0.0 && value.height >= 0.0;
}

struct EffectiveGridSpan {
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

void Panel::OnRender(
    DrawingContext& context) noexcept {
    auto& builder = Aero::Internal::DrawingPrivate::Builder(context);
    static_cast<void>(PaintBrushRect(
        builder,
        GetBackground(),
        Rect{
            0.0, 0.0,
            GetRenderSize().width,
            GetRenderSize().height}));
}

void Control::OnRender(
    DrawingContext& context) noexcept {
    auto& builder = Aero::Internal::DrawingPrivate::Builder(context);
    static_cast<void>(PaintBrushRect(
        builder,
        GetBackground(),
        Rect{
            0.0, 0.0,
            GetRenderSize().width,
            GetRenderSize().height}));
}

StackPanel::StackPanel() noexcept : StackPanel(Orientation::Vertical) {}

StackPanel::StackPanel(Orientation orientation) noexcept
    : Panel(StaticTypeId()) {
    if (orientation != Orientation::Vertical) {
        SetOrientation(orientation);
    }
}

Orientation StackPanel::GetOrientation() const noexcept {
    return GetValueOr(
        OrientationProperty, Orientation::Vertical);
}

void StackPanel::SetOrientation(Orientation value) noexcept {
    SetValue(OrientationProperty, value);
}

Size StackPanel::MeasureOverride(Size availableSize) noexcept {
    Size desired;
    const Orientation orientation = GetOrientation();
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Size childAvailable = availableSize;
        if (orientation == Orientation::Vertical) childAvailable.height = 1.0e12;
        else childAvailable.width = 1.0e12;
        Base::Result<void> measured = MeasureChild(*child, childAvailable);
        if (!measured) return Size{};
        const Size childDesired = child->GetDesiredSize();
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

Size StackPanel::ArrangeOverride(Size finalSize) noexcept {
    double offset = 0.0;
    const Orientation orientation = GetOrientation();
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size desired = child->GetDesiredSize();
        const Rect slot = orientation == Orientation::Vertical
            ? Rect{0.0, offset, finalSize.width, desired.height}
            : Rect{offset, 0.0, desired.width, finalSize.height};
        Base::Result<void> arranged = ArrangeChild(*child, slot);
        if (!arranged) return finalSize;
        offset += orientation == Orientation::Vertical
            ? desired.height : desired.width;
    }
    return finalSize;
}

bool DockPanel::GetLastChildFill() const noexcept {
    return GetValueOr(LastChildFillProperty, true);
}

void DockPanel::SetLastChildFill(
    bool value) noexcept {
    SetValue(LastChildFillProperty, value);
}

void DockPanel::SetChildDock(
    UIElement& child,
    Dock value) noexcept {
    bool attached = false;
    for (UIElement* current : LayoutChildren()) {
        attached = attached || current == &child;
    }
    if (!attached) {
        return;
    }
    child.SetValue(DockProperty, value);
}

Dock DockPanel::GetChildDock(
    const UIElement& child) const noexcept {
    return child.GetValueOr(DockProperty, Dock::Left);
}

Size DockPanel::MeasureOverride(
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
        if (!measured) return Size{};
        const Size childDesired = child->GetDesiredSize();
        const Dock dock = GetChildDock(*child);
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

Size DockPanel::ArrangeOverride(
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
            GetLastChildFill() &&
            index + 1U == children.Size();
        if (!fill) {
            const Size desired = child->GetDesiredSize();
            switch (GetChildDock(*child)) {
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
        if (!arranged) return finalSize;
    }
    return finalSize;
}

Orientation WrapPanel::GetOrientation() const noexcept {
    return GetValueOr(
        OrientationProperty,
        Orientation::Horizontal);
}

void WrapPanel::SetOrientation(
    Orientation value) noexcept {
    SetValue(OrientationProperty, value);
}

double WrapPanel::GetItemWidth() const noexcept {
    return GetValueOr(ItemWidthProperty, 0.0);
}

double WrapPanel::GetItemHeight() const noexcept {
    return GetValueOr(ItemHeightProperty, 0.0);
}

void WrapPanel::SetItemWidth(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return;
    }
    SetValue(ItemWidthProperty, value);
}

void WrapPanel::SetItemHeight(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return;
    }
    SetValue(ItemHeightProperty, value);
}

Size WrapPanel::MeasureOverride(
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
            GetItemWidth() > 0.0
                ? GetItemWidth() : availableSize.width,
            GetItemHeight() > 0.0
                ? GetItemHeight() : availableSize.height};
        Base::Result<void> measured =
            MeasureChild(*child, childAvailable);
        if (!measured) return Size{};
        const Size desired = child->GetDesiredSize();
        const double childPrimary = horizontal
            ? (GetItemWidth() > 0.0
                ? GetItemWidth() : desired.width)
            : (GetItemHeight() > 0.0
                ? GetItemHeight() : desired.height);
        const double childCross = horizontal
            ? (GetItemHeight() > 0.0
                ? GetItemHeight() : desired.height)
            : (GetItemWidth() > 0.0
                ? GetItemWidth() : desired.width);
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

Size WrapPanel::ArrangeOverride(
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
        const Size desired = child->GetDesiredSize();
        const double childPrimary = horizontal
            ? (GetItemWidth() > 0.0
                ? GetItemWidth() : desired.width)
            : (GetItemHeight() > 0.0
                ? GetItemHeight() : desired.height);
        const double childCross = horizontal
            ? (GetItemHeight() > 0.0
                ? GetItemHeight() : desired.height)
            : (GetItemWidth() > 0.0
                ? GetItemWidth() : desired.width);
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
        if (!arranged) return finalSize;
        primary += childPrimary;
        lineCross = std::max(lineCross, childCross);
    }
    return finalSize;
}

std::uint32_t UniformGrid::GetRows() const noexcept {
    return GetValueOr(RowsProperty, 0U);
}

std::uint32_t UniformGrid::GetColumns() const noexcept {
    return GetValueOr(ColumnsProperty, 0U);
}

std::uint32_t UniformGrid::GetFirstColumn() const noexcept {
    return GetValueOr(FirstColumnProperty, 0U);
}

void UniformGrid::SetRows(
    std::uint32_t value) noexcept {
    SetValue(RowsProperty, value);
}

void UniformGrid::SetColumns(
    std::uint32_t value) noexcept {
    SetValue(ColumnsProperty, value);
}

void UniformGrid::SetFirstColumn(
    std::uint32_t value) noexcept {
    SetValue(FirstColumnProperty, value);
}

void UniformGrid::ResolveDimensions(
    std::uint32_t childCount,
    std::uint32_t& rows,
    std::uint32_t& columns) const noexcept {
    rows = GetRows();
    columns = GetColumns();
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

Size UniformGrid::MeasureOverride(
    Size availableSize) noexcept {
    std::uint32_t count = 0U;
    for (UIElement* child : LayoutChildren()) {
        if (child != nullptr) ++count;
    }
    std::uint32_t rows = 0U;
    std::uint32_t columns = 0U;
    ResolveDimensions(count + GetFirstColumn(), rows, columns);
    const Size cellAvailable{
        availableSize.width / static_cast<double>(columns),
        availableSize.height / static_cast<double>(rows)};
    Size cellDesired;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> measured =
            MeasureChild(*child, cellAvailable);
        if (!measured) return Size{};
        cellDesired.width = std::max(
            cellDesired.width,
            child->GetDesiredSize().width);
        cellDesired.height = std::max(
            cellDesired.height,
            child->GetDesiredSize().height);
    }
    return Size{
        cellDesired.width * columns,
        cellDesired.height * rows};
}

Size UniformGrid::ArrangeOverride(
    Size finalSize) noexcept {
    std::uint32_t count = 0U;
    for (UIElement* child : LayoutChildren()) {
        if (child != nullptr) ++count;
    }
    std::uint32_t rows = 0U;
    std::uint32_t columns = 0U;
    ResolveDimensions(count + GetFirstColumn(), rows, columns);
    const double width =
        finalSize.width / static_cast<double>(columns);
    const double height =
        finalSize.height / static_cast<double>(rows);
    std::uint32_t index = std::min(
        GetFirstColumn(), columns - 1U);
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const std::uint32_t row = index / columns;
        const std::uint32_t column = index % columns;
        Base::Result<void> arranged = ArrangeChild(
            *child,
            {column * width, row * height, width, height});
        if (!arranged) return finalSize;
        ++index;
    }
    return finalSize;
}

Canvas::Canvas() noexcept : Panel(StaticTypeId()) {}

void Canvas::SetChildPosition(
    UIElement& child, Point position) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (!IsFinite(position)) {
        return;
    }
    bool attached = false;
    for (UIElement* current : LayoutChildren()) attached = attached || current == &child;
    if (!attached) {
        return;
    }
    child.SetValue(LeftProperty, position.x);
    child.SetValue(TopProperty, position.y);
}

Point Canvas::GetChildPosition(const UIElement& child) const noexcept {
    return {
        child.GetValueOr(LeftProperty, 0.0),
        child.GetValueOr(TopProperty, 0.0)};
}

Size Canvas::MeasureOverride(Size) noexcept {
    Size desired;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> measured = MeasureChild(*child, {1.0e12, 1.0e12});
        if (!measured) return Size{};
        const Point position = GetChildPosition(*child);
        desired.width = std::max(
            desired.width, position.x + child->GetDesiredSize().width);
        desired.height = std::max(
            desired.height, position.y + child->GetDesiredSize().height);
    }
    return desired;
}

Size Canvas::ArrangeOverride(Size finalSize) noexcept {
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size desired = child->GetDesiredSize();
        Point position = GetChildPosition(*child);
        Base::Result<EffectiveValueSource> leftSource =
            child->GetValueSource(LeftProperty.Handle());
        if (!leftSource) return finalSize;
        if (leftSource.Value() !=
            EffectiveValueSource::Local) {
            Base::Result<EffectiveValueSource> rightSource =
                child->GetValueSource(RightProperty.Handle());
            if (!rightSource) return finalSize;
            if (rightSource.Value() ==
                EffectiveValueSource::Local) {
                position.x = finalSize.width -
                    child->GetValueOr(RightProperty, 0.0) -
                    desired.width;
            }
        }
        Base::Result<EffectiveValueSource> topSource =
            child->GetValueSource(TopProperty.Handle());
        if (!topSource) return finalSize;
        if (topSource.Value() !=
            EffectiveValueSource::Local) {
            Base::Result<EffectiveValueSource> bottomSource =
                child->GetValueSource(BottomProperty.Handle());
            if (!bottomSource) return finalSize;
            if (bottomSource.Value() ==
                EffectiveValueSource::Local) {
                position.y = finalSize.height -
                    child->GetValueOr(BottomProperty, 0.0) -
                    desired.height;
            }
        }
        Base::Result<void> arranged = ArrangeChild(*child,
            {position.x, position.y, desired.width, desired.height});
        if (!arranged) return finalSize;
    }
    return finalSize;
}

void ColumnDefinition::SetWidth(
    GridLength value) noexcept {
    if (!std::isfinite(value.value) ||
        value.value < 0.0 ||
        (value.unit == GridUnitType::Star &&
            value.value <= 0.0)) {
        return;
    }
    width_ = value;
}

void ColumnDefinition::SetMaxWidth(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return;
    }
    maxWidth_ = value;
}

void ColumnDefinition::SetSharedSizeGroup(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(value)) return;
    sharedSizeGroup_ = std::move(candidate);
}

void RowDefinition::SetHeight(
    GridLength value) noexcept {
    if (!std::isfinite(value.value) ||
        value.value < 0.0 ||
        (value.unit == GridUnitType::Star &&
            value.value <= 0.0)) {
        return;
    }
    height_ = value;
}

void RowDefinition::SetMaxHeight(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return;
    }
    maxHeight_ = value;
}

void RowDefinition::SetSharedSizeGroup(
    Base::StringView value) noexcept {
    Base::String candidate;
    if (!candidate.Assign(value)) return;
    sharedSizeGroup_ = std::move(candidate);
}

Grid::Grid() noexcept
    : Panel(StaticTypeId()), columns_(), rows_(),
      desiredColumns_(), desiredRows_() {}

void Grid::SetColumnDefinitions(
    Base::Span<const GridLength> definitions) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    Base::Result<void> valid = ValidateDefinitions(definitions);
    if (!valid) return;
    Base::Vector<GridLength> next;
    Base::Result<void> copied = next.Assign(definitions);
    if (!copied) return;
    columns_ = std::move(next);
    columnDefinitionObjects_.Clear();
    (void)InvalidateMeasure();
}

void Grid::SetRowDefinitions(
    Base::Span<const GridLength> definitions) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    Base::Result<void> valid = ValidateDefinitions(definitions);
    if (!valid) return;
    Base::Vector<GridLength> next;
    Base::Result<void> copied = next.Assign(definitions);
    if (!copied) return;
    rows_ = std::move(next);
    rowDefinitionObjects_.Clear();
    (void)InvalidateMeasure();
}

void Grid::SetChildCell(
    UIElement& child, std::uint32_t row, std::uint32_t column) noexcept {
    SetChildCell(child, row, column, 1U, 1U);
}

void Grid::SetChildCell(
    UIElement& child,
    std::uint32_t row,
    std::uint32_t column,
    std::uint32_t rowSpan,
    std::uint32_t columnSpan) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (rowSpan == 0U || columnSpan == 0U) {
        return;
    }
    bool attached = false;
    for (UIElement* current : LayoutChildren()) attached = attached || current == &child;
    if (!attached) {
        return;
    }
    child.SetValue(RowProperty, row);
    child.SetValue(ColumnProperty, column);
    child.SetValue(RowSpanProperty, rowSpan);
    child.SetValue(ColumnSpanProperty, columnSpan);
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
        columnDefinitionObjects_.PushBack(definition);
    if (!objectAdded) return objectAdded.GetStatus();
    Base::Result<void> lengthAdded =
        columns_.PushBack(definition->GetWidth());
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
        rowDefinitionObjects_.PushBack(definition);
    if (!objectAdded) return objectAdded.GetStatus();
    Base::Result<void> lengthAdded =
        rows_.PushBack(definition->GetHeight());
    if (!lengthAdded) {
        rowDefinitionObjects_.PopBack();
        return lengthAdded.GetStatus();
    }
    return InvalidateMeasure();
}

void
Grid::ClearColumnDefinitionObjects() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    columnDefinitionObjects_.Clear();
    columns_.Clear();
    (void)InvalidateMeasure();
}

void
Grid::ClearRowDefinitionObjects() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    rowDefinitionObjects_.Clear();
    rows_.Clear();
    (void)InvalidateMeasure();
}

Base::Result<void> Grid::AddInputBinding(
    Base::Ref<Input::KeyBinding> binding) noexcept {
    if (!binding) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Grid InputBinding cannot be null");
    }
    Base::Result<void> finalized = binding->Finalize();
    if (!finalized) return finalized.GetStatus();
    return inputBindings_.PushBack(std::move(binding));
}

Base::StringView Grid::GetColumnDefinitionsText() const noexcept {
    return GetValueOr(
        ColumnDefinitionsTextProperty,
        Base::StringView{});
}

Base::StringView Grid::GetRowDefinitionsText() const noexcept {
    return GetValueOr(
        RowDefinitionsTextProperty,
        Base::StringView{});
}

void Grid::SetColumnDefinitionsText(
    Base::StringView value) noexcept {
    SetValue(
        ColumnDefinitionsTextProperty, value);
}

void Grid::SetRowDefinitionsText(
    Base::StringView value) noexcept {
    SetValue(
        RowDefinitionsTextProperty, value);
}

Size Grid::MeasureOverride(
    Size availableSize) noexcept {
    const std::uint32_t columns = GetColumnCount();
    const std::uint32_t rows = GetRowCount();
    Base::Vector<double> desiredColumns;
    Base::Vector<double> desiredRows;
    Base::Result<void> resized = desiredColumns.Resize(columns, 0.0);
    if (!resized) return Size{};
    resized = desiredRows.Resize(rows, 0.0);
    if (!resized) return Size{};

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
                GetChildRow(*child),
                GetChildRowSpan(*child),
                rows);
        const EffectiveGridSpan columnPlacement =
            CoerceGridSpan(
                GetChildColumn(*child),
                GetChildColumnSpan(*child),
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
        if (!measured) return Size{};
        const Size childDesired = child->GetDesiredSize();
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

Size Grid::ArrangeOverride(Size finalSize) noexcept {
    Base::Vector<double> columns;
    Base::Vector<double> rows;
    Base::Result<void> resolved = ResolveTracks(
        {columns_.Data(), columns_.Size()},
        {desiredColumns_.Data(), desiredColumns_.Size()},
        finalSize.width, columns);
    if (!resolved) return finalSize;
    resolved = ResolveTracks(
        {rows_.Data(), rows_.Size()},
        {desiredRows_.Data(), desiredRows_.Size()},
        finalSize.height, rows);
    if (!resolved) return finalSize;

    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const EffectiveGridSpan rowPlacement =
            CoerceGridSpan(
                GetChildRow(*child),
                GetChildRowSpan(*child),
                rows.Size());
        const EffectiveGridSpan columnPlacement =
            CoerceGridSpan(
                GetChildColumn(*child),
                GetChildColumnSpan(*child),
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
        if (!arranged) return finalSize;
    }
    return finalSize;
}

std::uint32_t Grid::GetColumnCount() const noexcept {
    return columns_.Empty() ? 1U : columns_.Size();
}

std::uint32_t Grid::GetRowCount() const noexcept {
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

std::uint32_t Grid::GetChildRow(const UIElement& child) const noexcept {
    return child.GetValueOr(RowProperty, 0U);
}

std::uint32_t Grid::GetChildColumn(const UIElement& child) const noexcept {
    return child.GetValueOr(ColumnProperty, 0U);
}

std::uint32_t Grid::GetChildRowSpan(
    const UIElement& child) const noexcept {
    return std::max(
        1U,
        child.GetValueOr(RowSpanProperty, 1U));
}

std::uint32_t Grid::GetChildColumnSpan(
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
    Base::Result<void> resized = resolved.Resize(count, 0.0);
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

void Viewbox::SetStretch(
    Stretch value) noexcept {
    SetValue(StretchProperty, value);
}

void Viewbox::SetStretchDirection(
    StretchDirection value) noexcept {
    SetValue(StretchDirectionProperty, value);
}

Size Viewbox::MeasureOverride(
    Size availableSize) noexcept {
    UIElement* child = GetChild();
    if (child == nullptr) {
        if (!LayoutChildren().Empty()) {
            return Size{};
        }
        return Size{};
    }

    // The layout kernel keeps all constraints finite. A large finite measure
    // gives Viewbox content its natural size while preserving that invariant.
    constexpr double NaturalConstraint = 1.0e12;
    Base::Result<void> measured = MeasureChild(
        *child,
        {NaturalConstraint, NaturalConstraint});
    if (!measured) return Size{};

    const Size natural = child->GetDesiredSize();
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
    UIElement* child = GetChild();
    if (child == nullptr) {
        viewTransform_.Reset();
        return {};
    }
    Base::Ref<Transform> current =
        child->GetRenderTransform();
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
        child->SetRenderTransform(viewTransform_);
    }

    Base::Transform2D matrix;
    matrix.m11 = scaleX;
    matrix.m22 = scaleY;
    matrix.dx = offsetX;
    matrix.dy = offsetY;
    viewTransform_->SetMatrixValue(matrix);
    return {};
}

Size Viewbox::ArrangeOverride(
    Size finalSize) noexcept {
    UIElement* child = GetChild();
    if (child == nullptr) {
        Base::Result<void> reset =
            ApplyViewTransform(1.0, 1.0, 0.0, 0.0);
        if (!reset) return finalSize;
        return finalSize;
    }

    const Size natural = child->GetDesiredSize();
    if (natural.width <= 0.0 || natural.height <= 0.0) {
        Base::Result<void> arranged = ArrangeChild(
            *child, {0.0, 0.0, 0.0, 0.0});
        if (!arranged) return finalSize;
        Base::Result<void> reset =
            ApplyViewTransform(1.0, 1.0, 0.0, 0.0);
        if (!reset) return finalSize;
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
    if (!arranged) return finalSize;

    Base::Result<void> transformed = ApplyViewTransform(
        scaleX,
        scaleY,
        0.0,
        0.0);
    if (!transformed) return finalSize;
    return finalSize;
}

Border::Border() noexcept : Decorator(StaticTypeId()) {}

Base::Ref<Brush> Border::GetBackground() const noexcept {
    return GetValueOr(
        BackgroundProperty, Base::Ref<Brush>{});
}

Base::Ref<Brush> Border::GetBorderBrush() const noexcept {
    return GetValueOr(
        BorderBrushProperty, Base::Ref<Brush>{});
}

Thickness Border::GetBorderThickness() const noexcept {
    return GetValueOr(
        BorderThicknessProperty, Thickness{});
}

CornerRadius Border::GetCornerRadius() const noexcept {
    return GetValueOr(
        CornerRadiusProperty, CornerRadius{});
}

Thickness Border::GetPadding() const noexcept {
    return GetValueOr(PaddingProperty, Thickness{});
}

void Border::SetBackground(
    Base::Ref<Brush> value) noexcept {
    SetValue(
        BackgroundProperty, std::move(value));
}

void Border::SetBorderBrush(
    Base::Ref<Brush> value) noexcept {
    SetValue(
        BorderBrushProperty, std::move(value));
}

void Border::SetBorderThickness(
    Thickness value) noexcept {
    SetValue(BorderThicknessProperty, value);
}

void Border::SetBorderThickness(
    double value) noexcept {
    SetBorderThickness({value, value, value, value});
}

void Border::SetCornerRadius(
    CornerRadius value) noexcept {
    SetValue(CornerRadiusProperty, value);
}

void Border::SetCornerRadius(
    double value) noexcept {
    SetCornerRadius({value, value, value, value});
}

void Border::SetPadding(Thickness value) noexcept {
    SetValue(PaddingProperty, value);
}

Size Border::MeasureOverride(Size availableSize) noexcept {
    const Thickness border = GetBorderThickness();
    const Thickness padding = GetPadding();
    const Thickness chrome{
        border.left + padding.left,
        border.top + padding.top,
        border.right + padding.right,
        border.bottom + padding.bottom};
    UIElement* child = GetChild();
    if (child == nullptr) return Inflate({}, chrome);
    Base::Result<void> measured = MeasureChild(
        *child, Deflate(availableSize, chrome));
    if (!measured) return Size{};
    return Inflate(child->GetDesiredSize(), chrome);
}

Size Border::ArrangeOverride(Size finalSize) noexcept {
    UIElement* child = GetChild();
    if (child == nullptr) return finalSize;
    const Thickness border = GetBorderThickness();
    const Thickness padding = GetPadding();
    const Thickness chrome{
        border.left + padding.left,
        border.top + padding.top,
        border.right + padding.right,
        border.bottom + padding.bottom};
    const Size childSize = Deflate(finalSize, chrome);
    Base::Result<void> arranged = ArrangeChild(*child,
        {chrome.left, chrome.top,
         childSize.width, childSize.height});
    if (!arranged) return finalSize;
    return finalSize;
}

void Border::OnRender(
    DrawingContext& context) noexcept {
    auto& builder = Aero::Internal::DrawingPrivate::Builder(context);
    const Rect bounds{0.0, 0.0, GetRenderSize().width, GetRenderSize().height};
    if (bounds.width <= 0.0 ||
        bounds.height <= 0.0) {
        return;
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
    const Color brush = ::Aero::Internal::SampleBrush(GetBorderBrush());
    const Thickness thickness = GetBorderThickness();
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
        if (!border) return;
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
            return;
        }
        static_cast<void>(builder.FillRoundedRect(
            inner,
            ::Aero::Internal::SampleBrush(GetBackground()),
            std::min(
                std::max(0.0, radius - inset),
                std::min(
                    inner.width,
                    inner.height) * 0.5)));
        return;
    }
    Base::Result<void> fill = radius > 0.0
        ? builder.FillRoundedRect(
              bounds, ::Aero::Internal::SampleBrush(GetBackground()), radius)
        : builder.FillRect(bounds, ::Aero::Internal::SampleBrush(GetBackground()));
    if (!fill) return;
    if (uniformThickness > 0.0 &&
        brush.alpha > 0.0F) {
        static_cast<void>(builder.StrokeRect(
            bounds, brush, uniformThickness));
        return;
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
        if (!side) return;
        side = fillSide({
            std::max(0.0, bounds.width - thickness.right),
            0.0,
            std::min(bounds.width, thickness.right),
            bounds.height});
        if (!side) return;
        side = fillSide({
            0.0, 0.0,
            bounds.width,
            std::min(bounds.height, thickness.top)});
        if (!side) return;
        static_cast<void>(fillSide({
            0.0,
            std::max(0.0, bounds.height - thickness.bottom),
            bounds.width,
            std::min(bounds.height, thickness.bottom)}));
        return;
    }
    return;
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

Base::StringView TextBlock::GetText() const noexcept {
    return GetValueOr(TextProperty, Base::StringView());
}

Base::Ref<Brush> TextBlock::GetForeground() const noexcept {
    return GetValueOr(
        ForegroundProperty, Base::Ref<Brush>{});
}

Base::Ref<Brush> TextBlock::GetBackground() const noexcept {
    return GetValueOr(
        BackgroundProperty, Base::Ref<Brush>{});
}

double TextBlock::GetFontSize() const noexcept {
    return GetValueOr(FontSizeProperty, 16.0);
}

Base::StringView TextBlock::GetFontFamily() const noexcept {
    return FrameworkElement::GetFontFamily();
}

FontWeight TextBlock::GetFontWeight() const noexcept {
    if (RuntimeType() == Documents::Bold::StaticTypeId()) {
        return FontWeight::Bold;
    }
    return GetValueOr(
        FontWeightProperty,
        FontWeight::Normal);
}

FontStyle TextBlock::GetFontStyle() const noexcept {
    if (RuntimeType() == Documents::Italic::StaticTypeId()) {
        return FontStyle::Italic;
    }
    return GetValueOr(
        FontStyleProperty,
        FontStyle::Normal);
}

TextDecorations TextBlock::GetTextDecorations() const noexcept {
    if (RuntimeType() == Documents::Underline::StaticTypeId()) {
        return TextDecorations::Underline;
    }
    return GetValueOr(
        TextDecorationsProperty,
        TextDecorations::None);
}

TextWrapping TextBlock::GetTextWrapping() const noexcept {
    return GetValueOr(
        TextWrappingProperty,
        TextWrapping::NoWrap);
}

TextTrimming TextBlock::GetTextTrimming() const noexcept {
    return GetValueOr(
        TextTrimmingProperty,
        TextTrimming::None);
}

TextAlignment TextBlock::GetTextAlignment() const noexcept {
    return GetValueOr(
        TextAlignmentProperty,
        TextAlignment::Start);
}

void TextBlock::SetText(Base::StringView value) noexcept {
    SetValue(TextProperty, value);
    textHitRegions_.Clear();
}

void TextBlock::SetForeground(
    Base::Ref<Brush> value) noexcept {
    SetValue(
        ForegroundProperty, std::move(value));
}

void TextBlock::SetBackground(
    Base::Ref<Brush> value) noexcept {
    SetValue(
        BackgroundProperty, std::move(value));
}

void TextBlock::SetFontSize(double value) noexcept {
    SetValue(FontSizeProperty, value);
}

void TextBlock::SetFontFamily(
    Base::StringView value) noexcept {
    SetValue(FontFamilyProperty, value);
}

void TextBlock::SetFontWeight(
    FontWeight value) noexcept {
    SetValue(FontWeightProperty, value);
}

void TextBlock::SetFontStyle(
    FontStyle value) noexcept {
    SetValue(FontStyleProperty, value);
}

void TextBlock::SetTextDecorations(
    TextDecorations value) noexcept {
    SetValue(TextDecorationsProperty, value);
}

void TextBlock::SetTextWrapping(
    TextWrapping value) noexcept {
    SetValue(TextWrappingProperty, value);
}

void TextBlock::SetTextTrimming(
    TextTrimming value) noexcept {
    SetValue(TextTrimmingProperty, value);
}

void TextBlock::SetTextAlignment(
    TextAlignment value) noexcept {
    SetValue(TextAlignmentProperty, value);
}

Meta::Value TextBlock::GetMetadataInlines() const noexcept {
    if (pendingInline_) {
        return Meta::Value::FromObject(
            pendingInline_->RuntimeType(),
            pendingInline_);
    }
    return Meta::Value::NullObject(
        Meta::TypeOf<Base::Object>());
}

void TextBlock::SetInlineValue(
    Meta::Value value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (value.Kind() == Meta::ValueKind::Object &&
        !value.IsNullObject() &&
        value.AsObject()) {
        Base::Ref<Base::Object> inlineObject = value.AsObject();
        if (!PropertyRegistry().Types().IsDerivedFrom(
                inlineObject->RuntimeType(),
                Documents::Inline::StaticTypeId())) {
            return;
        }
        (void)AddOwnedInline(inlineObject);
        return;
    }
    if (value.Kind() != Meta::ValueKind::String) {
        return;
    }
    Base::Result<Base::Ref<Documents::Run>> created =
        Base::MakeRef<Documents::Run>();
    if (!created) return;
    created.Value()->SetText(value.AsString());
    pendingInline_ = Base::Ref<Base::Object>(
        created.Value());
    return;
}

Base::Result<void> TextBlock::AddOwnedInline(
    const Base::Ref<Base::Object>& inlineObject) noexcept {
    if (!inlineObject) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "TextBlock inline cannot be null");
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
        ownedInlines_.PushBack(inlineObject);
    if (!appended) return appended.GetStatus();
    auto& inlineValue = *static_cast<Documents::Inline*>(inlineObject.Get());
    Aero::Internal::ElementPrivate::Attach(
        inlineValue, this, this, nullptr);
    pendingInline_ = inlineObject;
    return InvalidateMeasure();
}

void TextBlock::ClearOwnedInlines() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    for (Base::Ref<Base::Object>& item : ownedInlines_) {
        if (item) {
            Aero::Internal::ElementPrivate::Detach(
                *static_cast<Documents::Inline*>(item.Get()));
        }
    }
    ownedInlines_.Clear();
    pendingInline_.Reset();
    (void)InvalidateMeasure();
}

Base::StringView TextBlock::EffectiveFontFamily() const noexcept {
    const Base::StringView configured = GetFontFamily();
    const bool defaultFamily =
        configured.Empty() ||
        configured == Base::StringView("Segoe UI");
    if (!defaultFamily) return configured;
    const bool bold =
        GetFontWeight() == FontWeight::Bold ||
        GetFontWeight() == FontWeight::SemiBold;
    const bool italic =
        GetFontStyle() != FontStyle::Normal;
    if (bold && italic) {
        return Base::StringView("Segoe UI Bold Italic");
    }
    if (bold) return Base::StringView("Segoe UI Bold");
    if (italic) return Base::StringView("Segoe UI Italic");
    return configured;
}

void TextBlock::SetGlyphRun(
    RenderGlyphRunId glyphRun, Size size) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (!IsValidTextSize(size) ||
        (glyphRun == InvalidRenderGlyphRunId &&
            (size.width != 0.0 || size.height != 0.0))) {
        return;
    }
    if (glyphRuns_.Size() == 1U && glyphRuns_[0] == glyphRun &&
        glyphRunSize_.width == size.width &&
        glyphRunSize_.height == size.height &&
        !serviceOwnsGlyphRun_) return;
    ReleaseServiceGlyphRun();
    glyphRuns_.Clear();
    textHitRegions_.Clear();
    if (glyphRun != InvalidRenderGlyphRunId) {
        Base::Result<void> appended =
            glyphRuns_.PushBack(glyphRun);
        if (!appended) return;
    }
    glyphRunSize_ = size;
    (void)InvalidateMeasure();
    (void)InvalidateVisual();
}

Size TextBlock::MeasureOverride(Size availableSize) noexcept {
    Base::String flattened;
    Base::Result<void> copied = Documents::CopyText(*this, flattened);
    if (!copied) return Size{};

    if (layoutService_ != nullptr) {
        const Base::StringView text = flattened.View();
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
                Base::Result<void> invalidated = InvalidateVisual();
                if (!invalidated) return Size{};
            }
        } else {
            Internal::TextLayoutRequest request;
            request.text = text;
            request.availableSize = availableSize;
            request.dpiScale = GetDpiScale();
            request.pixelSize = static_cast<float>(GetFontSize());
            request.fontFamily = EffectiveFontFamily();
            request.wrapping = GetTextWrapping();
            request.trimming = GetTextTrimming();
            request.alignment = GetTextAlignment();
            Internal::TextLayoutResult output;
            Base::Result<void> prepared =
                layoutService_->ShapeAndPrepare(request, output);
            if (!prepared) return Size{};
            bool validGlyphRuns = true;
            for (RenderGlyphRunId glyphRun : output.glyphRuns) {
                if (glyphRun == InvalidRenderGlyphRunId) {
                    validGlyphRuns = false;
                    break;
                }
            }
            if (!IsValidTextSize(output.desiredSize) || !validGlyphRuns) {
                for (RenderGlyphRunId glyphRun : output.glyphRuns) {
                    if (glyphRun != InvalidRenderGlyphRunId) {
                        layoutService_->ReleaseGlyphRun(glyphRun);
                    }
                }
                return Size{};
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
                Base::Result<void> invalidated = InvalidateVisual();
                if (!invalidated) return Size{};
            }
        }
    }
    return Size{
        std::min(glyphRunSize_.width, availableSize.width),
        std::min(glyphRunSize_.height, availableSize.height)};
}

Size TextBlock::ArrangeOverride(Size finalSize) noexcept {
    return finalSize;
}

void TextBlock::OnRender(
    DrawingContext& context) noexcept {
    auto& builder = Aero::Internal::DrawingPrivate::Builder(context);
    const Color background = ::Aero::Internal::SampleBrush(GetBackground());
    if (background.alpha > 0.0F) {
        Base::Result<void> filled =
            builder.FillRect(
                {0.0, 0.0,
                 GetRenderSize().width,
                 GetRenderSize().height},
                background);
        if (!filled) return;
    }
    for (RenderGlyphRunId glyphRun : glyphRuns_) {
        Base::Result<void> drawn =
            builder.DrawGlyphRun(glyphRun, ::Aero::Internal::SampleBrush(GetForeground(), 0.5,
                Color{0.0F, 0.0F, 0.0F, 1.0F}));
        if (!drawn) return;
    }
    if (GetTextDecorations() ==
            TextDecorations::Underline &&
        glyphRunSize_.width > 0.0) {
        const double thickness =
            std::max(1.0, GetFontSize() * 0.06);
        const double y = std::max(
            0.0,
            glyphRunSize_.height - thickness * 1.5);
        static_cast<void>(builder.FillRect(
            {0.0, y, glyphRunSize_.width, thickness},
            ::Aero::Internal::SampleBrush(GetForeground(), 0.5,
                Color{0.0F, 0.0F, 0.0F, 1.0F})));
    }
    return;
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
    ::Aero::DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs&
        change) noexcept {
    auto& presenter =
        static_cast<ContentPresenter&>(object);
    presenter.contentValue_ = change.GetNewValue();
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
    case Meta::ValueKind::String:
        {
            Base::Result<void> assigned =
                text.Assign(
                    contentValue_.AsString());
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        break;
    case Meta::ValueKind::Boolean:
        {
            Base::Result<void> assigned =
                text.Assign(
                    contentValue_.AsBoolean()
                    ? Base::StringView("True")
                    : Base::StringView("False"));
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        break;
    case Meta::ValueKind::SignedInteger:
    case Meta::ValueKind::UnsignedInteger:
    case Meta::ValueKind::Double:
        {
            char raw[64]{};
            if (contentValue_.Kind() ==
                Meta::ValueKind::SignedInteger) {
                std::snprintf(
                    raw, sizeof(raw), "%lld",
                    static_cast<long long>(
                        contentValue_.
                            AsSignedInteger()));
            } else if (contentValue_.Kind() ==
                       Meta::ValueKind::
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
                text.Assign(raw);
            if (!assigned) {
                return assigned.GetStatus();
            }
        }
        break;
    case Meta::ValueKind::Object:
        if (!contentValue_.IsNullObject()) {
            return {};
        }
        break;
    default:
        return {};
    }
    static_cast<TextBlock*>(content_)->SetText(text.View());
    return {};
}

void ContentPresenter::SetContentSource(
    Base::StringView value) noexcept {
    SetValue(
        ContentSourceProperty, value);
}

bool ContentPresenter::IsOnlyAttachedContent(
    const UIElement& content) const noexcept {
    const UIElementChildRange children = LayoutChildren();
    return children.Size() == 1U && children[0] == &content;
}

void ContentPresenter::SetContent(UIElement* content) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    Base::Result<void> validated = ValidateContent(content);
    if (!validated) return;
    if (content == content_) return;
    content_ = content;
    if (content == nullptr) ownedContent_.Reset();
    (void)InvalidateMeasure();
}

void ContentPresenter::SetOwnedContent(
    const Base::Ref<Base::Object>& contentObject,
    UIElement& content) noexcept {
    if (!contentObject || contentObject.Get() != &content) {
        return;
    }
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    Base::Result<void> validated = ValidateContent(&content);
    if (!validated) return;
    content_ = &content;
    ownedContent_ = contentObject;
    (void)UpdatePresentedText();
    (void)InvalidateMeasure();
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

Size ContentPresenter::MeasureOverride(
    Size availableSize) noexcept {
    if (content_ == nullptr) {
        if (!LayoutChildren().Empty()) {
            return Size{};
        }
        return Size{};
    }
    if (!IsOnlyAttachedContent(*content_)) {
        return Size{};
    }
    Base::Result<void> measured = MeasureChild(*content_, availableSize);
    if (!measured) return Size{};
    return content_->GetDesiredSize();
}

Size ContentPresenter::ArrangeOverride(Size finalSize) noexcept {
    if (content_ == nullptr) return finalSize;
    if (!IsOnlyAttachedContent(*content_)) {
        return finalSize;
    }
    Base::Result<void> arranged = ArrangeChild(*content_,
        {0.0, 0.0, finalSize.width, finalSize.height});
    if (!arranged) return finalSize;
    return finalSize;
}

} // namespace Aero::Controls

namespace Aero::Controls {

std::uint32_t UIElementCollection::GetCount() const noexcept {
    return owner_ != nullptr ? owner_->ChildCountCore() : 0U;
}

UIElement* UIElementCollection::GetItem(std::uint32_t index) const noexcept {
    if (owner_ == nullptr) return nullptr;
    Base::Ref<Base::Object> child = owner_->ChildAtCore(index);
    return child ? static_cast<UIElement*>(child.Get()) : nullptr;
}

Base::Result<void> UIElementCollection::Add(Base::Ref<UIElement> child) noexcept {
    if (owner_ == nullptr || !child) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "UIElementCollection requires an owner and child");
    }
    Base::Ref<Base::Object> object(child);
    return owner_->AddChildCore(object, *child);
}

Base::Result<void> UIElementCollection::Remove(UIElement& child) noexcept {
    if (owner_ == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState, "UIElementCollection has no owner");
    }
    Base::Result<bool> removed = owner_->RemoveChildCore(child);
    return removed ? Base::Result<void>() : Base::Result<void>(removed.GetStatus());
}

void UIElementCollection::Clear() noexcept {
    if (owner_ == nullptr) {
        return;
    }
    owner_->ClearChildrenCore();
}

Base::Result<void> Panel::AddChildCore(const Base::Ref<Base::Object>& childObject, UIElement& child) noexcept {
    if (!childObject || childObject.Get() != &child) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Panel child ownership does not match its UIElement");
    }
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    for (const Base::Ref<Base::Object>& owned : ownedChildren_) {
        if (owned.Get() == &child) {
            return Base::Status::Failure(Base::ErrorCode::AlreadyExists, "Panel already contains the child");
        }
    }
    Base::Result<void> appended = ownedChildren_.PushBack(childObject);
    if (!appended) return appended.GetStatus();
    return InvalidateMeasure();
}

Base::Result<bool> Panel::RemoveChildCore(UIElement& child) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access.GetStatus();
    if (!LayoutChildren().Empty()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState, "Mounted Panel children must be removed by the presentation runtime");
    }
    for (std::uint32_t index = 0U; index < ownedChildren_.Size(); ++index) {
        if (ownedChildren_[index].Get() != &child) continue;
        for (std::uint32_t next = index + 1U; next < ownedChildren_.Size(); ++next) {
            ownedChildren_[next - 1U] = std::move(ownedChildren_[next]);
        }
        ownedChildren_.PopBack();
        Base::Result<void> invalidated = InvalidateMeasure();
        return invalidated ? Base::Result<bool>(true) : Base::Result<bool>(invalidated.GetStatus());
    }
    return false;
}

void Panel::ClearChildrenCore() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (!LayoutChildren().Empty()) {
        return;
    }
    ownedChildren_.Clear();
    (void)InvalidateMeasure();
}

} // namespace Aero::Controls
