#include "gui/core/Describe.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include <Aero/Controls/Grid.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace Aero::Controls {
using namespace Aero::Meta;
using namespace Aero::Threading;

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

GridLength ColumnDefinition::GetWidth() const noexcept {
    return GetValue(WidthProperty);
}
double ColumnDefinition::GetMaxWidth() const noexcept {
    return GetValue(MaxWidthProperty);
}
Base::StringView ColumnDefinition::GetSharedSizeGroup() const noexcept {
    return GetValue(SharedSizeGroupProperty);
}
void ColumnDefinition::SetWidth(
    GridLength value) noexcept {
    if (!std::isfinite(value.value) ||
        value.value < 0.0 ||
        (value.unit == GridUnitType::Star &&
            value.value <= 0.0)) {
        return;
    }
    SetValue(WidthProperty, value);
}
void ColumnDefinition::SetMaxWidth(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return;
    }
    SetValue(MaxWidthProperty, value);
}
void ColumnDefinition::SetSharedSizeGroup(
    Base::StringView value) noexcept {
    SetValue(SharedSizeGroupProperty, value);
}
GridLength RowDefinition::GetHeight() const noexcept {
    return GetValue(HeightProperty);
}
double RowDefinition::GetMaxHeight() const noexcept {
    return GetValue(MaxHeightProperty);
}
Base::StringView RowDefinition::GetSharedSizeGroup() const noexcept {
    return GetValue(SharedSizeGroupProperty);
}
void RowDefinition::SetHeight(
    GridLength value) noexcept {
    if (!std::isfinite(value.value) ||
        value.value < 0.0 ||
        (value.unit == GridUnitType::Star &&
            value.value <= 0.0)) {
        return;
    }
    SetValue(HeightProperty, value);
}
void RowDefinition::SetMaxHeight(
    double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return;
    }
    SetValue(MaxHeightProperty, value);
}
void RowDefinition::SetSharedSizeGroup(
    Base::StringView value) noexcept {
    SetValue(SharedSizeGroupProperty, value);
}
Grid::Grid() noexcept
    : Panel(StaticTypeId()), columns_(), rows_(),
      desiredColumns_(), desiredRows_() {}

void Grid::SetColumnDefinitions(
    Base::Span<const GridLength> definitions) noexcept {
    if (!CheckAccess() || !ValidateDefinitions(definitions)) return;
    Base::Vector<GridLength> next;
    next.Assign(definitions);
    columns_ = std::move(next);
    columnDefinitionObjects_.Clear();
    InvalidateMeasure();
}
void Grid::SetRowDefinitions(
    Base::Span<const GridLength> definitions) noexcept {
    if (!CheckAccess() || !ValidateDefinitions(definitions)) return;
    Base::Vector<GridLength> next;
    next.Assign(definitions);
    rows_ = std::move(next);
    rowDefinitionObjects_.Clear();
    InvalidateMeasure();
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
void Grid::AddColumnDefinition(
    Base::Ref<ColumnDefinition> definition) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) { AERO_ASSERT(false); return; }
    if (!definition) { AERO_ASSERT(false); return; }
    columnDefinitionObjects_.PushBack(definition);
    columns_.PushBack(definition->GetWidth());
    InvalidateMeasure();
}
void Grid::AddRowDefinition(
    Base::Ref<RowDefinition> definition) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) { AERO_ASSERT(false); return; }
    if (!definition) { AERO_ASSERT(false); return; }
    rowDefinitionObjects_.PushBack(definition);
    rows_.PushBack(definition->GetHeight());
    InvalidateMeasure();
}
void
Grid::ClearColumnDefinitionObjects() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    columnDefinitionObjects_.Clear();
    columns_.Clear();
    InvalidateMeasure();
}
void
Grid::ClearRowDefinitionObjects() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    rowDefinitionObjects_.Clear();
    rows_.Clear();
    InvalidateMeasure();
}
Base::StringView Grid::GetColumnDefinitionsText() const noexcept {
    return GetValue(ColumnDefinitionsTextProperty);
}
Base::StringView Grid::GetRowDefinitionsText() const noexcept {
    return GetValue(RowDefinitionsTextProperty);
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

bool Grid::ValidateValueCore(
    DependencyPropertyHandle property,
    const PropertyValue& value) const noexcept {
    if (property == ColumnDefinitionsTextProperty.Handle() ||
        property == RowDefinitionsTextProperty.Handle()) {
        if (value.Kind() != Meta::ValueKind::String) {
            return false;
        }
        Base::Vector<GridLength> parsed;
        return Controls::Grid::ParseDefinitions(
            value.AsString(), parsed);
    }
    return Panel::ValidateValueCore(property, value);
}

void Grid::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == ColumnDefinitionsTextProperty.Handle()) {
        if (args.GetNewValue().Kind() == Meta::ValueKind::String) {
            Base::Vector<GridLength> parsed;
            if (Controls::Grid::ParseDefinitions(
                    args.GetNewValue().AsString(), parsed)) {
                SetColumnDefinitions(parsed.AsSpan());
            }
        }
    } else if (prop == RowDefinitionsTextProperty.Handle()) {
        if (args.GetNewValue().Kind() == Meta::ValueKind::String) {
            Base::Vector<GridLength> parsed;
            if (Controls::Grid::ParseDefinitions(
                    args.GetNewValue().AsString(), parsed)) {
                SetRowDefinitions(parsed.AsSpan());
            }
        }
    }
    Panel::OnPropertyChanged(args);
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
    Base::Vector<float> desiredColumns;
    Base::Vector<float> desiredRows;
    desiredColumns.Resize(columns, 0.0f);
    desiredRows.Resize(rows, 0.0f);

    for (std::uint32_t index = 0U; index < columns; ++index) {
        const GridLength definition = ColumnAt(index);
        if (definition.unit == GridUnitType::Pixel)
            desiredColumns[index] = std::min(definition.value, MaxWidthAt(index));
    }
    for (std::uint32_t index = 0U; index < rows; ++index) {
        const GridLength definition = RowAt(index);
        if (definition.unit == GridUnitType::Pixel)
            desiredRows[index] = std::min(definition.value, MaxHeightAt(index));
    }

    constexpr float Unconstrained = 1.0e12f;
    constexpr float FiniteConstraintLimit =
        Unconstrained * 0.5f;
    float pixelWidth = 0.0f;
    float pixelHeight = 0.0f;
    float columnStarWeight = 0.0f;
    float rowStarWeight = 0.0f;
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

    struct SpanningChild {
        UIElement* child = nullptr;
        std::uint32_t column = 0U;
        std::uint32_t row = 0U;
        std::uint32_t columnSpan = 1U;
        std::uint32_t rowSpan = 1U;
    };
    Base::Vector<SpanningChild> spanningChildren;

    struct NonSpanningChild {
        UIElement* child = nullptr;
        std::uint32_t column = 0U;
        std::uint32_t row = 0U;
        bool hasStarCol = false;
        bool hasStarRow = false;
    };
    Base::Vector<NonSpanningChild> nonSpanningChildren;

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
        const std::uint32_t row = rowPlacement.index;
        const std::uint32_t column = columnPlacement.index;
        const std::uint32_t rowSpan = rowPlacement.span;
        const std::uint32_t columnSpan = columnPlacement.span;

        if (columnSpan > 1U || rowSpan > 1U) {
            spanningChildren.PushBack({child, column, row, columnSpan, rowSpan});
            continue;
        }

        const bool hasStarCol = ColumnAt(column).unit == GridUnitType::Star;
        const bool hasStarRow = RowAt(row).unit == GridUnitType::Star;
        nonSpanningChildren.PushBack({child, column, row, hasStarCol, hasStarRow});
    }

    // Pass 1A: Non-spanning elements with non-star columns to establish Auto column widths.
    // If an element's row is Star, measure with childHeight = Unconstrained so its width
    // contribution to the Auto column is captured before Star columns subtract nonStarWidth.
    for (const NonSpanningChild& item : nonSpanningChildren) {
        if (item.hasStarCol) continue;
        UIElement* child = item.child;
        const std::uint32_t column = item.column;
        const std::uint32_t row = item.row;

        float childWidth = Unconstrained;
        const GridLength colDef = ColumnAt(column);
        if (colDef.unit == GridUnitType::Pixel) {
            childWidth = colDef.value;
        }

        float childHeight = Unconstrained;
        const GridLength rowDef = RowAt(row);
        if (rowDef.unit == GridUnitType::Pixel) {
            childHeight = rowDef.value;
        }

        MeasureChild(*child, {childWidth, childHeight});
        const Size childDesired = child->GetDesiredSize();
        if (colDef.unit != GridUnitType::Pixel) {
            desiredColumns[column] = std::max(desiredColumns[column], childDesired.width);
        }
        if (rowDef.unit != GridUnitType::Pixel && !item.hasStarRow) {
            desiredRows[row] = std::max(desiredRows[row], childDesired.height);
        }
    }

    // Pass 1B: Non-spanning elements with non-star rows (and Star columns) to establish Auto row heights.
    for (const NonSpanningChild& item : nonSpanningChildren) {
        if (item.hasStarRow || !item.hasStarCol) continue;
        UIElement* child = item.child;
        const std::uint32_t row = item.row;

        float childWidth = Unconstrained;
        float childHeight = Unconstrained;
        const GridLength rowDef = RowAt(row);
        if (rowDef.unit == GridUnitType::Pixel) {
            childHeight = rowDef.value;
        }

        MeasureChild(*child, {childWidth, childHeight});
        const Size childDesired = child->GetDesiredSize();
        if (rowDef.unit != GridUnitType::Pixel) {
            desiredRows[row] = std::max(desiredRows[row], childDesired.height);
        }
    }

    // Compute non-star widths and heights (Pixel + Auto desired)
    float nonStarWidth = 0.0f;
    for (std::uint32_t index = 0U; index < columns; ++index) {
        const GridLength definition = ColumnAt(index);
        if (definition.unit == GridUnitType::Pixel) {
            nonStarWidth += definition.value;
        } else if (definition.unit == GridUnitType::Auto) {
            nonStarWidth += desiredColumns[index];
        }
    }
    float nonStarHeight = 0.0f;
    for (std::uint32_t index = 0U; index < rows; ++index) {
        const GridLength definition = RowAt(index);
        if (definition.unit == GridUnitType::Pixel) {
            nonStarHeight += definition.value;
        } else if (definition.unit == GridUnitType::Auto) {
            nonStarHeight += desiredRows[index];
        }
    }

    // Pass 1C: Non-spanning elements with star tracks
    for (const NonSpanningChild& item : nonSpanningChildren) {
        if (!item.hasStarCol && !item.hasStarRow) continue;
        UIElement* child = item.child;
        const std::uint32_t column = item.column;
        const std::uint32_t row = item.row;

        float childWidth = Unconstrained;
        const GridLength colDef = ColumnAt(column);
        if (colDef.unit == GridUnitType::Pixel) {
            childWidth = colDef.value;
        } else if (colDef.unit == GridUnitType::Auto) {
            childWidth = Unconstrained;
        } else if (colDef.unit == GridUnitType::Star) {
            childWidth = (availableSize.width < FiniteConstraintLimit && columnStarWeight > 0.0f)
                ? std::max(0.0f, availableSize.width - nonStarWidth) * colDef.value / columnStarWeight
                : Unconstrained;
        }

        float childHeight = Unconstrained;
        const GridLength rowDef = RowAt(row);
        if (rowDef.unit == GridUnitType::Pixel) {
            childHeight = rowDef.value;
        } else if (rowDef.unit == GridUnitType::Auto) {
            childHeight = Unconstrained;
        } else if (rowDef.unit == GridUnitType::Star) {
            childHeight = (availableSize.height < FiniteConstraintLimit && rowStarWeight > 0.0f)
                ? std::max(0.0f, availableSize.height - nonStarHeight) * rowDef.value / rowStarWeight
                : Unconstrained;
        }

        MeasureChild(*child, {childWidth, childHeight});
        const Size childDesired = child->GetDesiredSize();
        if (colDef.unit != GridUnitType::Pixel) {
            desiredColumns[column] = std::max(desiredColumns[column], childDesired.width);
        }
        if (rowDef.unit != GridUnitType::Pixel) {
            desiredRows[row] = std::max(desiredRows[row], childDesired.height);
        }
    }

    // Pass 2: Spanning elements sorted by max(columnSpan, rowSpan).
    // Cells that do not cross a star track run first so Auto tracks (for
    // example a GroupBox header) are final before a later cell that spans
    // those Auto tracks plus a star track distributes only its remainder
    // onto the star. Doing both in one span-sorted pass assigns the whole
    // child to the star, then adds the Auto header again.
    std::sort(spanningChildren.begin(), spanningChildren.end(),
        [](const SpanningChild& a, const SpanningChild& b) {
            return std::max(a.columnSpan, a.rowSpan) < std::max(b.columnSpan, b.rowSpan);
        });

    for (int starPhase = 0; starPhase < 2; ++starPhase) {
    if (starPhase == 1) {
        nonStarWidth = 0.0f;
        for (std::uint32_t index = 0U; index < columns; ++index) {
            const GridLength definition = ColumnAt(index);
            if (definition.unit == GridUnitType::Pixel) {
                nonStarWidth += definition.value;
            } else if (definition.unit == GridUnitType::Auto) {
                nonStarWidth += desiredColumns[index];
            }
        }
        nonStarHeight = 0.0f;
        for (std::uint32_t index = 0U; index < rows; ++index) {
            const GridLength definition = RowAt(index);
            if (definition.unit == GridUnitType::Pixel) {
                nonStarHeight += definition.value;
            } else if (definition.unit == GridUnitType::Auto) {
                nonStarHeight += desiredRows[index];
            }
        }
    }
    for (const SpanningChild& item : spanningChildren) {
        UIElement* child = item.child;
        const std::uint32_t column = item.column;
        const std::uint32_t row = item.row;
        const std::uint32_t columnSpan = item.columnSpan;
        const std::uint32_t rowSpan = item.rowSpan;

        float fixedWidth = 0.0f;
        float fixedHeight = 0.0f;
        float spanColumnStarWeight = 0.0f;
        float spanRowStarWeight = 0.0f;
        bool autoWidth = false;
        bool autoHeight = false;

        for (std::uint32_t offset = 0U; offset < columnSpan; ++offset) {
            const GridLength definition = ColumnAt(column + offset);
            if (definition.unit == GridUnitType::Pixel) fixedWidth += definition.value;
            else if (definition.unit == GridUnitType::Auto) autoWidth = true;
            else if (definition.unit == GridUnitType::Star) spanColumnStarWeight += definition.value;
        }
        for (std::uint32_t offset = 0U; offset < rowSpan; ++offset) {
            const GridLength definition = RowAt(row + offset);
            if (definition.unit == GridUnitType::Pixel) fixedHeight += definition.value;
            else if (definition.unit == GridUnitType::Auto) autoHeight = true;
            else if (definition.unit == GridUnitType::Star) spanRowStarWeight += definition.value;
        }

        const bool crossesStar =
            spanColumnStarWeight > 0.0f || spanRowStarWeight > 0.0f;
        if (crossesStar != (starPhase == 1)) continue;

        float childWidth = fixedWidth;
        if (spanColumnStarWeight > 0.0f) {
            childWidth = (availableSize.width < FiniteConstraintLimit && columnStarWeight > 0.0f)
                ? fixedWidth + std::max(0.0f, availableSize.width - nonStarWidth) * spanColumnStarWeight / columnStarWeight
                : Unconstrained;
        } else if (autoWidth) {
            childWidth = Unconstrained;
        }

        float childHeight = fixedHeight;
        if (spanRowStarWeight > 0.0f) {
            childHeight = (availableSize.height < FiniteConstraintLimit && rowStarWeight > 0.0f)
                ? fixedHeight + std::max(0.0f, availableSize.height - nonStarHeight) * spanRowStarWeight / rowStarWeight
                : Unconstrained;
        } else if (autoHeight) {
            childHeight = Unconstrained;
        }

        MeasureChild(*child, {childWidth, childHeight});
        const Size childDesired = child->GetDesiredSize();

        // Distribute extra width among spanned columns
        float currentWidth = 0.0f;
        std::uint32_t autoColCount = 0U;
        for (std::uint32_t offset = 0U; offset < columnSpan; ++offset) {
            const std::uint32_t c = column + offset;
            const GridLength def = ColumnAt(c);
            if (def.unit == GridUnitType::Pixel) {
                currentWidth += def.value;
            } else if (def.unit == GridUnitType::Auto) {
                currentWidth += desiredColumns[c];
                ++autoColCount;
            } else if (def.unit == GridUnitType::Star) {
                currentWidth += desiredColumns[c];
            }
        }
        const float extraWidth = std::max(0.0f, childDesired.width - currentWidth);
        if (extraWidth > 0.0f) {
            if (spanColumnStarWeight > 0.0f) {
                for (std::uint32_t offset = 0U; offset < columnSpan; ++offset) {
                    const std::uint32_t c = column + offset;
                    if (ColumnAt(c).unit == GridUnitType::Star) {
                        desiredColumns[c] += extraWidth * (ColumnAt(c).value / spanColumnStarWeight);
                    }
                }
            } else if (autoColCount > 0U) {
                const float share = extraWidth / static_cast<float>(autoColCount);
                for (std::uint32_t offset = 0U; offset < columnSpan; ++offset) {
                    const std::uint32_t c = column + offset;
                    if (ColumnAt(c).unit == GridUnitType::Auto) {
                        desiredColumns[c] += share;
                    }
                }
            }
        }

        // Distribute extra height among spanned rows
        float currentHeight = 0.0f;
        std::uint32_t autoRowCount = 0U;
        for (std::uint32_t offset = 0U; offset < rowSpan; ++offset) {
            const std::uint32_t r = row + offset;
            const GridLength def = RowAt(r);
            if (def.unit == GridUnitType::Pixel) {
                currentHeight += def.value;
            } else if (def.unit == GridUnitType::Auto) {
                currentHeight += desiredRows[r];
                ++autoRowCount;
            } else if (def.unit == GridUnitType::Star) {
                currentHeight += desiredRows[r];
            }
        }
        const float extraHeight = std::max(0.0f, childDesired.height - currentHeight);
        if (extraHeight > 0.0f) {
            if (spanRowStarWeight > 0.0f) {
                for (std::uint32_t offset = 0U; offset < rowSpan; ++offset) {
                    const std::uint32_t r = row + offset;
                    if (RowAt(r).unit == GridUnitType::Star) {
                        desiredRows[r] += extraHeight * (RowAt(r).value / spanRowStarWeight);
                    }
                }
            } else if (autoRowCount > 0U) {
                const float share = extraHeight / static_cast<float>(autoRowCount);
                for (std::uint32_t offset = 0U; offset < rowSpan; ++offset) {
                    const std::uint32_t r = row + offset;
                    if (RowAt(r).unit == GridUnitType::Auto) {
                        desiredRows[r] += share;
                    }
                }
            }
        }
    }
    }

    // Compute total desired size of the Grid for MeasureOverride
    float maxStarWidthUnit = 0.0f;
    for (std::uint32_t index = 0U; index < columns; ++index) {
        const GridLength def = ColumnAt(index);
        if (def.unit == GridUnitType::Star && def.value > 0.0f) {
            maxStarWidthUnit = std::max(maxStarWidthUnit, desiredColumns[index] / def.value);
        }
    }
    float totalDesiredWidth = 0.0f;
    for (std::uint32_t index = 0U; index < columns; ++index) {
        const GridLength def = ColumnAt(index);
        if (def.unit == GridUnitType::Pixel) {
            totalDesiredWidth += def.value;
        } else if (def.unit == GridUnitType::Auto) {
            totalDesiredWidth += desiredColumns[index];
        } else if (def.unit == GridUnitType::Star) {
            totalDesiredWidth += def.value * maxStarWidthUnit;
        }
    }

    float maxStarHeightUnit = 0.0f;
    for (std::uint32_t index = 0U; index < rows; ++index) {
        const GridLength def = RowAt(index);
        if (def.unit == GridUnitType::Star && def.value > 0.0f) {
            maxStarHeightUnit = std::max(maxStarHeightUnit, desiredRows[index] / def.value);
        }
    }
    float totalDesiredHeight = 0.0f;
    for (std::uint32_t index = 0U; index < rows; ++index) {
        const GridLength def = RowAt(index);
        if (def.unit == GridUnitType::Pixel) {
            totalDesiredHeight += def.value;
        } else if (def.unit == GridUnitType::Auto) {
            totalDesiredHeight += desiredRows[index];
        } else if (def.unit == GridUnitType::Star) {
            totalDesiredHeight += def.value * maxStarHeightUnit;
        }
    }

    for (std::uint32_t index = 0U; index < columns; ++index) {
        desiredColumns[index] = std::min(desiredColumns[index], MaxWidthAt(index));
    }
    for (std::uint32_t index = 0U; index < rows; ++index) {
        desiredRows[index] = std::min(desiredRows[index], MaxHeightAt(index));
    }
    desiredColumns_ = std::move(desiredColumns);
    desiredRows_ = std::move(desiredRows);
    return Size{totalDesiredWidth, totalDesiredHeight};
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

    Base::Vector<float> maxCols;
    maxCols.Reserve(GetColumnCount());
    for (std::uint32_t i = 0U; i < GetColumnCount(); ++i) maxCols.PushBack(MaxWidthAt(i));
    Base::Vector<float> maxRows;
    maxRows.Reserve(GetRowCount());
    for (std::uint32_t i = 0U; i < GetRowCount(); ++i) maxRows.PushBack(MaxHeightAt(i));

    Base::Vector<float> columns;
    Base::Vector<float> rows;
    if (!ResolveTracks(
            {columns_.Data(), columns_.Size()},
            {desiredColumns_.Data(), desiredColumns_.Size()},
            finalSize.width, columns, maxCols.AsSpan()) ||
        !ResolveTracks(
            {rows_.Data(), rows_.Size()},
            {desiredRows_.Data(), desiredRows_.Size()},
            finalSize.height, rows, maxRows.AsSpan())) {
        return finalSize;
    }

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
        float x = 0.0f;
        float y = 0.0f;
        for (std::uint32_t index = 0U; index < column; ++index) x += columns[index];
        for (std::uint32_t index = 0U; index < row; ++index) y += rows[index];
        float width = 0.0f;
        float height = 0.0f;
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
        ArrangeChild(*child,
            {x, y, width, height});
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
float Grid::MaxWidthAt(std::uint32_t index) const noexcept {
    if (index < columnDefinitionObjects_.Size() && columnDefinitionObjects_[index]) {
        return static_cast<float>(columnDefinitionObjects_[index]->GetMaxWidth());
    }
    return 1.0e12f;
}
float Grid::MaxHeightAt(std::uint32_t index) const noexcept {
    if (index < rowDefinitionObjects_.Size() && rowDefinitionObjects_[index]) {
        return static_cast<float>(rowDefinitionObjects_[index]->GetMaxHeight());
    }
    return 1.0e12f;
}
bool Grid::ValidateDefinitions(
    Base::Span<const GridLength> definitions) const noexcept {
    for (const GridLength& definition : definitions) {
        if (!std::isfinite(definition.value) || definition.value < 0.0f ||
            (definition.unit == GridUnitType::Star && definition.value <= 0.0f)) {
            return false;
        }
    }
    return true;
}
std::uint32_t Grid::GetChildRow(const UIElement& child) const noexcept {
    return child.GetValue(RowProperty);
}
std::uint32_t Grid::GetChildColumn(const UIElement& child) const noexcept {
    return child.GetValue(ColumnProperty);
}
std::uint32_t Grid::GetChildRowSpan(
    const UIElement& child) const noexcept {
    return std::max(
        1U,
        child.GetValue(RowSpanProperty));
}
std::uint32_t Grid::GetChildColumnSpan(
    const UIElement& child) const noexcept {
    return std::max(
        1U,
        child.GetValue(ColumnSpanProperty));
}
bool Grid::ResolveTracks(
    Base::Span<const GridLength> definitions,
    Base::Span<const float> desired,
    float available,
    Base::Vector<float>& resolved,
    Base::Span<const float> maxLimits) const noexcept {
    const std::uint32_t count = definitions.Empty() ? 1U : definitions.Size();
    if (!std::isfinite(available) || available < 0.0f ||
        (!desired.Empty() && desired.Size() != count)) {
        return false;
    }
    resolved.Resize(count, 0.0f);
    float occupied = 0.0f;
    float totalStarWeight = 0.0f;
    const auto getMaxLimit = [&](std::uint32_t idx) noexcept -> float {
        return (!maxLimits.Empty() && idx < maxLimits.Size()) ? maxLimits[idx] : 1.0e12f;
    };
    for (std::uint32_t index = 0U; index < count; ++index) {
        const GridLength definition = definitions.Empty()
            ? GridLength::Star() : definitions[index];
        const float limit = getMaxLimit(index);
        if (definition.unit == GridUnitType::Pixel) {
            resolved[index] = std::min(definition.value, limit);
            occupied += resolved[index];
        } else if (definition.unit == GridUnitType::Auto) {
            resolved[index] = std::min(desired.Empty() ? 0.0f : desired[index], limit);
            occupied += resolved[index];
        } else {
            totalStarWeight += definition.value;
        }
    }
    const bool unconstrained =
        available >= 1.0e12f * 0.5f;
    const float remaining = std::max(0.0f, available - occupied);
    if (totalStarWeight > 0.0f) {
        float maxStarUnit = 0.0f;
        if (unconstrained && !desired.Empty()) {
            for (std::uint32_t index = 0U; index < count; ++index) {
                const GridLength definition = definitions.Empty()
                    ? GridLength::Star() : definitions[index];
                if (definition.unit == GridUnitType::Star && definition.value > 0.0f) {
                    maxStarUnit = std::max(maxStarUnit, desired[index] / definition.value);
                }
            }
        }
        for (std::uint32_t index = 0U; index < count; ++index) {
            const GridLength definition = definitions.Empty()
                ? GridLength::Star() : definitions[index];
            if (definition.unit != GridUnitType::Star) continue;
            const float limit = getMaxLimit(index);
            // WPF: star tracks behave like Auto when the constraint is
            // infinite (StackPanel → ColorSelector Grid with Height="*").
            if (unconstrained) {
                resolved[index] = std::min(definition.value * maxStarUnit, limit);
            } else {
                resolved[index] = std::min(remaining *
                    (definition.value / totalStarWeight), limit);
            }
        }
    }
    return true;
}

namespace {

Base::Result<GridLength> ConvertGridLength(Base::StringView text) noexcept {
    return Controls::Grid::ConvertLength(text);
}

bool EqualGridLength(const void* left, const void* right, void*) noexcept {
    const auto& a = *static_cast<const GridLength*>(left);
    const auto& b = *static_cast<const GridLength*>(right);
    return a.unit == b.unit && a.value == b.value;
}



} // namespace

AERO_DESCRIBE(Grid) {
    using namespace Aero::Meta;
    Register<GridLength>(context)
        .ValueSemantics({sizeof(GridLength), alignof(GridLength), nullptr, nullptr, &EqualGridLength, nullptr, true})
        .TextConverter<&ConvertGridLength>();

    Register<ColumnDefinition>(context)
        .Property(ColumnDefinition::WidthProperty, GridLength::Star(), AffectsParentMeasure)
        .Property(ColumnDefinition::MaxWidthProperty, 1.0e12, AffectsParentMeasure)
        .Property(ColumnDefinition::SharedSizeGroupProperty, Base::String{}, AffectsParentMeasure)
        .Factory();

    Register<RowDefinition>(context)
        .Property(RowDefinition::HeightProperty, GridLength::Star(), AffectsParentMeasure)
        .Property(RowDefinition::MaxHeightProperty, 1.0e12, AffectsParentMeasure)
        .Property(RowDefinition::SharedSizeGroupProperty, Base::String{}, AffectsParentMeasure)
        .Factory();

    Register<Grid>(context)
        .Property(Grid::IsSharedSizeScopeProperty, false)
        .Property(Grid::ColumnDefinitionsTextProperty, Base::String{}, AffectsMeasure)
        .Property(Grid::RowDefinitionsTextProperty, Base::String{}, AffectsMeasure)
        .Collection<ColumnDefinition, &Grid::AddColumnDefinition, &Grid::ClearColumnDefinitionObjects>("ColumnDefinitions")
        .Collection<RowDefinition, &Grid::AddRowDefinition, &Grid::ClearRowDefinitionObjects>("RowDefinitions")
        .Property(Grid::RowProperty, std::uint32_t{0}, AffectsParentMeasure)
        .Property(Grid::ColumnProperty, std::uint32_t{0}, AffectsParentMeasure)
        .Property(Grid::RowSpanProperty, std::uint32_t{1}, AffectsParentMeasure, &Base::Validate::Positive<std::uint32_t>)
        .Property(Grid::ColumnSpanProperty, std::uint32_t{1}, AffectsParentMeasure, &Base::Validate::Positive<std::uint32_t>)
        .Factory();
}

} // namespace Aero::Controls

namespace Aero {

Base::Result<GridLength> Controls::Grid::ConvertLength(
    Base::StringView text) noexcept {
    return ::Aero::Base::ValueConversion::ConvertGridLength(text);
}

bool Controls::Grid::ParseDefinitions(
    Base::StringView text,
    Base::Vector<GridLength>& output) noexcept {
    output.Clear();
    const Base::StringView value =
        ::Aero::Base::ValueConversion::Trim(text);
    if (value.Empty()) return true;
    std::uint32_t start = 0U;
    while (start <= value.SizeBytes()) {
        std::uint32_t end = start;
        while (end < value.SizeBytes() &&
            value[end] != ',') {
            ++end;
        }
        const Base::StringView token =
            ::Aero::Base::ValueConversion::Trim(
                value.Substr(start, end - start));
        if (token.Empty()) {
            return false;
        }
        Base::Result<GridLength> parsed =
            ConvertGridLength(token);
        if (!parsed) return false;
        output.PushBack(parsed.Value());
        if (end == value.SizeBytes()) break;
        start = end + 1U;
    }
    return true;
}

} // namespace Aero
