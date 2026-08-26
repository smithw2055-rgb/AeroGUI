#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ListBox.hpp>
#include <Aero/Controls/TreeView.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Media/Transforms.hpp>
#include "gui/media/BrushRendering.hpp"
#include "gui/media/MediaState.hpp"
#include <Aero/Documents.hpp>
#include "RichText.hpp"

#include "TextBlockLayout.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>


namespace Aero::Controls {
using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Render;

namespace {

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
    ::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingPrivate::Builder(context);
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
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size desired = child->GetDesiredSize();
        const Rect slot = orientation == Orientation::Vertical
            ? Rect{0.0, offset, finalSize.width, desired.height}
            : (isRtl
                ? Rect{finalSize.width - offset - desired.width, 0.0, desired.width, finalSize.height}
                : Rect{offset, 0.0, desired.width, finalSize.height});
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
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
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
            Dock dock = GetChildDock(*child);
            if (isRtl) {
                if (dock == Dock::Left) dock = Dock::Right;
                else if (dock == Dock::Right) dock = Dock::Left;
            }
            switch (dock) {
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
    const bool isRtl = horizontal && GetFlowDirection() == FlowDirection::RightToLeft;
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
            ? (isRtl
                ? Rect{finalSize.width - primary - childPrimary, cross, childPrimary, childCross}
                : Rect{primary, cross, childPrimary, childCross})
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
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    std::uint32_t index = std::min(
        GetFirstColumn(), columns - 1U);
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const std::uint32_t row = index / columns;
        const std::uint32_t column = index % columns;
        const double x = isRtl
            ? finalSize.width - (column + 1U) * width
            : column * width;
        Base::Result<void> arranged = ArrangeChild(
            *child,
            {x, row * height, width, height});
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
    if (!columnDefinitionObjects_.Empty()) {
        columns_.Clear();
        for (const auto& colDef : columnDefinitionObjects_) {
            if (colDef) (void)columns_.PushBack(colDef->GetWidth());
        }
    }
    if (!rowDefinitionObjects_.Empty()) {
        rows_.Clear();
        for (const auto& rowDef : rowDefinitionObjects_) {
            if (rowDef) (void)rows_.PushBack(rowDef->GetHeight());
        }
    }

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
    if (!columnDefinitionObjects_.Empty()) {
        columns_.Clear();
        for (const auto& colDef : columnDefinitionObjects_) {
            if (colDef) (void)columns_.PushBack(colDef->GetWidth());
        }
    }
    if (!rowDefinitionObjects_.Empty()) {
        rows_.Clear();
        for (const auto& rowDef : rowDefinitionObjects_) {
            if (rowDef) (void)rows_.PushBack(rowDef->GetHeight());
        }
    }

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

    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
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
        if (isRtl) {
            x = finalSize.width - x - width;
        }
        Base::Result<void> arranged = ArrangeChild(*child,
            {x, y, width, height});
        if (!arranged) return finalSize;
    }
    return finalSize;
}
std::uint32_t Grid::GetColumnCount() const noexcept {
    if (!columnDefinitionObjects_.Empty()) {
        return columnDefinitionObjects_.Size();
    }
    return columns_.Empty() ? 1U : columns_.Size();
}
std::uint32_t Grid::GetRowCount() const noexcept {
    if (!rowDefinitionObjects_.Empty()) {
        return rowDefinitionObjects_.Size();
    }
    return rows_.Empty() ? 1U : rows_.Size();
}
GridLength Grid::ColumnAt(std::uint32_t index) const noexcept {
    if (index < columnDefinitionObjects_.Size() && columnDefinitionObjects_[index]) {
        return columnDefinitionObjects_[index]->GetWidth();
    }
    return columns_.Empty() ? GridLength::Star() : (index < columns_.Size() ? columns_[index] : GridLength::Star());
}
GridLength Grid::RowAt(std::uint32_t index) const noexcept {
    if (index < rowDefinitionObjects_.Size() && rowDefinitionObjects_[index]) {
        return rowDefinitionObjects_[index]->GetHeight();
    }
    return rows_.Empty() ? GridLength::Star() : (index < rows_.Size() ? rows_[index] : GridLength::Star());
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

} // namespace Aero
