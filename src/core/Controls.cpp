#include <Aero/Core/Controls.hpp>
#include <Aero/Core/Presentation.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Aero::Core {
namespace {

bool IsValidColor(Color value) noexcept {
    return IsFinite(value) && value.red >= 0.0F && value.red <= 1.0F &&
        value.green >= 0.0F && value.green <= 1.0F &&
        value.blue >= 0.0F && value.blue <= 1.0F &&
        value.alpha >= 0.0F && value.alpha <= 1.0F;
}

bool IsValidPadding(Thickness value) noexcept {
    return IsFinite(value) && value.left >= 0.0 && value.top >= 0.0 &&
        value.right >= 0.0 && value.bottom >= 0.0 &&
        std::isfinite(value.left + value.right) &&
        std::isfinite(value.top + value.bottom);
}

bool IsValidTextSize(Size value) noexcept {
    return IsFinite(value) && value.width >= 0.0 && value.height >= 0.0;
}

TypeId PresentationType(const char* name) noexcept {
    return MakeTypeId(Base::StringView(
        name, static_cast<std::uint32_t>(std::strlen(name))));
}

} // namespace

StackPanel::StackPanel() noexcept : StackPanel(Orientation::Vertical) {}

StackPanel::StackPanel(Orientation orientation) noexcept
    : Panel(StaticTypeId()) {
    if (orientation != Orientation::Vertical) {
        static_cast<void>(SetOrientation(orientation));
    }
}

Orientation StackPanel::GetOrientation() const noexcept {
    Base::Result<Value> value = GetValue(OrientationProperty);
    return value ? static_cast<Orientation>(value.Value().AsUnsignedInteger())
                 : Orientation::Vertical;
}

Base::Result<void> StackPanel::SetOrientation(Orientation value) noexcept {
    if (value > Orientation::Vertical) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "StackPanel orientation is invalid");
    }
    return SetValue(OrientationProperty, Value::FromUnsignedInteger(
        PresentationType("Orientation"), static_cast<std::uint64_t>(value)));
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
    Base::Result<void> left = child.SetValue(LeftProperty,
        Value::FromDouble(PresentationType("Double"), position.x));
    return left ? child.SetValue(TopProperty,
        Value::FromDouble(PresentationType("Double"), position.y)) : left;
}

Point Canvas::ChildPosition(const UIElement& child) const noexcept {
    Base::Result<Value> left = child.GetValue(LeftProperty);
    Base::Result<Value> top = child.GetValue(TopProperty);
    return {left ? left.Value().AsDouble() : 0.0,
        top ? top.Value().AsDouble() : 0.0};
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
        const Point position = ChildPosition(*child);
        const Size desired = child->DesiredSize();
        Base::Result<void> arranged = ArrangeChild(*child,
            {position.x, position.y, desired.width, desired.height});
        if (!arranged) return arranged.GetStatus();
    }
    return finalSize;
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
    const std::uint32_t count = definitions.Empty() ? 1U : definitions.Size();
    for (UIElement* child : LayoutChildren()) {
        if (child != nullptr && ChildColumn(*child) >= count) {
            return Base::Status::Failure(Base::ErrorCode::OutOfRange,
                "Grid column definition removal would orphan a child cell");
        }
    }
    Base::Vector<GridLength> next;
    Base::Result<void> copied = next.TryAssign(definitions);
    if (!copied) return copied.GetStatus();
    columns_ = std::move(next);
    return InvalidateMeasure();
}

Base::Result<void> Grid::SetRowDefinitions(
    Base::Span<const GridLength> definitions) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    Base::Result<void> valid = ValidateDefinitions(definitions);
    if (!valid) return valid;
    const std::uint32_t count = definitions.Empty() ? 1U : definitions.Size();
    for (UIElement* child : LayoutChildren()) {
        if (child != nullptr && ChildRow(*child) >= count) {
            return Base::Status::Failure(Base::ErrorCode::OutOfRange,
                "Grid row definition removal would orphan a child cell");
        }
    }
    Base::Vector<GridLength> next;
    Base::Result<void> copied = next.TryAssign(definitions);
    if (!copied) return copied.GetStatus();
    rows_ = std::move(next);
    return InvalidateMeasure();
}

Base::Result<void> Grid::SetChildCell(
    UIElement& child, std::uint32_t row, std::uint32_t column) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (row >= RowCount() || column >= ColumnCount()) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange,
            "Grid child cell is outside the declared track range");
    }
    bool attached = false;
    for (UIElement* current : LayoutChildren()) attached = attached || current == &child;
    if (!attached) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Grid child must be attached before assigning a cell");
    }
    Base::Result<void> rowResult = child.SetValue(RowProperty,
        Value::FromUnsignedInteger(PresentationType("UInt32"), row));
    return rowResult ? child.SetValue(ColumnProperty,
        Value::FromUnsignedInteger(PresentationType("UInt32"), column))
        : rowResult;
}

Base::Result<Size> Grid::MeasureOverride(Size) noexcept {
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
    for (UIElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const std::uint32_t row = ChildRow(*child);
        const std::uint32_t column = ChildColumn(*child);
        if (row >= rows || column >= columns) {
            return Base::Status::Failure(Base::ErrorCode::OutOfRange,
                "Grid child cell is outside the declared track range");
        }
        const GridLength columnDefinition = ColumnAt(column);
        const GridLength rowDefinition = RowAt(row);
        const Size childAvailable{
            columnDefinition.unit == GridUnitType::Pixel
                ? columnDefinition.value : Unconstrained,
            rowDefinition.unit == GridUnitType::Pixel
                ? rowDefinition.value : Unconstrained};
        Base::Result<void> measured = MeasureChild(*child, childAvailable);
        if (!measured) return measured.GetStatus();
        const Size childDesired = child->DesiredSize();
        if (columnDefinition.unit != GridUnitType::Pixel)
            desiredColumns[column] = std::max(
                desiredColumns[column], childDesired.width);
        if (rowDefinition.unit != GridUnitType::Pixel)
            desiredRows[row] = std::max(
                desiredRows[row], childDesired.height);
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
        const std::uint32_t row = ChildRow(*child);
        const std::uint32_t column = ChildColumn(*child);
        if (row >= rows.Size() || column >= columns.Size()) {
            return Base::Status::Failure(Base::ErrorCode::OutOfRange,
                "Grid child cell is outside the declared track range");
        }
        double x = 0.0;
        double y = 0.0;
        for (std::uint32_t index = 0U; index < column; ++index) x += columns[index];
        for (std::uint32_t index = 0U; index < row; ++index) y += rows[index];
        Base::Result<void> arranged = ArrangeChild(*child,
            {x, y, columns[column], rows[row]});
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
    Base::Result<Value> value = child.GetValue(RowProperty);
    return value ? static_cast<std::uint32_t>(
        value.Value().AsUnsignedInteger()) : 0U;
}

std::uint32_t Grid::ChildColumn(const UIElement& child) const noexcept {
    Base::Result<Value> value = child.GetValue(ColumnProperty);
    return value ? static_cast<std::uint32_t>(
        value.Value().AsUnsignedInteger()) : 0U;
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

Border::Border() noexcept : Decorator(StaticTypeId()) {}

Color Border::Background() const noexcept {
    Base::Result<Value> value = GetValue(BackgroundProperty);
    return value ? *static_cast<const Color*>(value.Value().AsCustom()) : Color{};
}

Color Border::BorderBrush() const noexcept {
    Base::Result<Value> value = GetValue(BorderBrushProperty);
    return value ? *static_cast<const Color*>(value.Value().AsCustom()) : Color{};
}

double Border::BorderThickness() const noexcept {
    Base::Result<Value> value = GetValue(BorderThicknessProperty);
    return value ? value.Value().AsDouble() : 0.0;
}

Thickness Border::Padding() const noexcept {
    Base::Result<Value> value = GetValue(PaddingProperty);
    return value ? *static_cast<const Thickness*>(value.Value().AsCustom())
                 : Thickness{};
}

Base::Result<void> Border::SetBackground(Color value) noexcept {
    if (!IsValidColor(value)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Border color is invalid");
    }
    Base::Result<Value> stored = PropertyRegistry().Types().TryCreateValue(
        PresentationType("Color"), &value);
    return stored ? SetValue(BackgroundProperty, stored.Value())
                  : stored.GetStatus();
}

Base::Result<void> Border::SetStroke(Color value, double thickness) noexcept {
    if (!IsValidColor(value) || !std::isfinite(thickness) || thickness < 0.0) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Border stroke is invalid");
    }
    Base::Result<void> brush = SetBorderBrush(value);
    return brush ? SetBorderThickness(thickness) : brush;
}

Base::Result<void> Border::SetBorderBrush(Color value) noexcept {
    if (!IsValidColor(value)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Border brush is invalid");
    }
    Base::Result<Value> stored = PropertyRegistry().Types().TryCreateValue(
        PresentationType("Color"), &value);
    return stored ? SetValue(BorderBrushProperty, stored.Value())
                  : stored.GetStatus();
}

Base::Result<void> Border::SetBorderThickness(double value) noexcept {
    if (!std::isfinite(value) || value < 0.0) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Border thickness is invalid");
    }
    return SetValue(BorderThicknessProperty,
        Value::FromDouble(PresentationType("Double"), value));
}

Base::Result<void> Border::SetPadding(Thickness value) noexcept {
    if (!IsValidPadding(value)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Border padding must be finite, nonnegative, and non-overflowing");
    }
    Base::Result<Value> stored = PropertyRegistry().Types().TryCreateValue(
        PresentationType("Thickness"), &value);
    return stored ? SetValue(PaddingProperty, stored.Value())
                  : stored.GetStatus();
}

Base::Result<Size> Border::MeasureOverride(Size availableSize) noexcept {
    UIElement* child = Child();
    if (child == nullptr) return Inflate({}, Padding());
    const Thickness padding = Padding();
    Base::Result<void> measured = MeasureChild(
        *child, Deflate(availableSize, padding));
    if (!measured) return measured.GetStatus();
    return Inflate(child->DesiredSize(), padding);
}

Base::Result<Size> Border::ArrangeOverride(Size finalSize) noexcept {
    UIElement* child = Child();
    if (child == nullptr) return finalSize;
    const Thickness padding = Padding();
    const Size childSize = Deflate(finalSize, padding);
    Base::Result<void> arranged = ArrangeChild(*child,
        {padding.left, padding.top, childSize.width, childSize.height});
    if (!arranged) return arranged.GetStatus();
    return finalSize;
}

Base::Result<void> Border::BuildDisplayList(
    DisplayListBuilder& builder) noexcept {
    const Rect bounds{0.0, 0.0, RenderSize().width, RenderSize().height};
    Base::Result<void> fill = builder.FillRect(bounds, Background());
    if (!fill) return fill.GetStatus();
    const Color brush = BorderBrush();
    const double thickness = BorderThickness();
    if (thickness > 0.0 && brush.alpha > 0.0F) {
        return builder.StrokeRect(bounds, brush, thickness);
    }
    return {};
}

TextBlock::TextBlock() noexcept : FrameworkElement(StaticTypeId()) {}

Base::StringView TextBlock::Text() const noexcept {
    Base::Result<Value> value = GetValue(TextProperty);
    return value ? value.Value().AsString() : Base::StringView();
}

Color TextBlock::Foreground() const noexcept {
    Base::Result<Value> value = GetValue(ForegroundProperty);
    return value ? *static_cast<const Color*>(value.Value().AsCustom())
                 : Color{0.0F, 0.0F, 0.0F, 1.0F};
}

Base::Result<void> TextBlock::SetText(Base::StringView value) noexcept {
    Base::Result<Value> stored = Value::TryFromString(
        PresentationType("String"), value);
    return stored ? SetValue(TextProperty, stored.Value())
                  : stored.GetStatus();
}

Base::Result<void> TextBlock::SetForeground(Color value) noexcept {
    if (!IsValidColor(value)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "TextBlock foreground color is invalid");
    }
    Base::Result<Value> stored = PropertyRegistry().Types().TryCreateValue(
        PresentationType("Color"), &value);
    return stored ? SetValue(ForegroundProperty, stored.Value())
                  : stored.GetStatus();
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
    if (glyphRun_ == glyphRun && glyphRunSize_.width == size.width &&
        glyphRunSize_.height == size.height) return {};
    glyphRun_ = glyphRun;
    glyphRunSize_ = size;
    Base::Result<void> measure = InvalidateMeasure();
    if (!measure) return measure.GetStatus();
    return InvalidateRender();
}

Base::Result<Size> TextBlock::MeasureOverride(Size availableSize) noexcept {
    return Size{std::min(glyphRunSize_.width, availableSize.width),
        std::min(glyphRunSize_.height, availableSize.height)};
}

Base::Result<void> TextBlock::BuildDisplayList(
    DisplayListBuilder& builder) noexcept {
    return glyphRun_ == InvalidRenderGlyphRunId
        ? Base::Result<void>()
        : builder.DrawGlyphRun(glyphRun_, Foreground());
}

ContentPresenter::ContentPresenter() noexcept
    : FrameworkElement(StaticTypeId()) {}

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
    return InvalidateMeasure();
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

} // namespace Aero::Core
