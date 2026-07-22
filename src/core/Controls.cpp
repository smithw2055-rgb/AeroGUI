#include <Aero/Core/Controls.hpp>

#include <algorithm>
#include <cmath>

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

bool SameThickness(Thickness left, Thickness right) noexcept {
    return left.left == right.left && left.top == right.top &&
        left.right == right.right && left.bottom == right.bottom;
}
} // namespace

StackPanel::StackPanel(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
    TypeId runtimeType, Orientation orientation, Base::IAllocator* allocator) noexcept
    : LayoutElement(dispatcher, registry, runtimeType, allocator), orientation_(orientation) {}

Base::Result<void> StackPanel::SetOrientation(Orientation value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (orientation_ == value) return {};
    orientation_ = value;
    return InvalidateMeasure();
}

Base::Result<Size> StackPanel::MeasureOverride(Size availableSize) noexcept {
    Size desired;
    for (LayoutElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Size childAvailable = availableSize;
        if (orientation_ == Orientation::Vertical) childAvailable.height = 1.0e12;
        else childAvailable.width = 1.0e12;
        Base::Result<void> measured = MeasureChild(*child, childAvailable);
        if (!measured) return measured.GetStatus();
        const Size childDesired = child->DesiredSize();
        if (orientation_ == Orientation::Vertical) {
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
    for (LayoutElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Size desired = child->DesiredSize();
        const Rect slot = orientation_ == Orientation::Vertical
            ? Rect{0.0, offset, finalSize.width, desired.height}
            : Rect{offset, 0.0, desired.width, finalSize.height};
        Base::Result<void> arranged = ArrangeChild(*child, slot);
        if (!arranged) return arranged.GetStatus();
        offset += orientation_ == Orientation::Vertical ? desired.height : desired.width;
    }
    return finalSize;
}

Canvas::Canvas(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
    TypeId runtimeType, Base::IAllocator* allocator) noexcept
    : LayoutElement(dispatcher, registry, runtimeType, allocator),
      positions_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()) {}

Base::Result<void> Canvas::SetChildPosition(
    LayoutElement& child, Point position) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (!IsFinite(position)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Canvas child position must be finite");
    }
    bool attached = false;
    for (LayoutElement* current : LayoutChildren()) attached = attached || current == &child;
    if (!attached) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Canvas child must be attached before positioning");
    }
    for (Position& entry : positions_) {
        if (entry.child == &child) {
            entry.point = position;
            return InvalidateArrange();
        }
    }
    Base::Result<void> added = positions_.TryPushBack({&child, position});
    if (!added) return added.GetStatus();
    return InvalidateArrange();
}

Point Canvas::ChildPosition(const LayoutElement& child) const noexcept {
    for (const Position& entry : positions_) {
        if (entry.child == &child) return entry.point;
    }
    return {};
}

Base::Result<Size> Canvas::MeasureOverride(Size) noexcept {
    Size desired;
    for (LayoutElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> measured = MeasureChild(*child, {1.0e12, 1.0e12});
        if (!measured) return measured.GetStatus();
        const Point position = ChildPosition(*child);
        desired.width = std::max(desired.width, position.x + child->DesiredSize().width);
        desired.height = std::max(desired.height, position.y + child->DesiredSize().height);
    }
    return desired;
}

Base::Result<Size> Canvas::ArrangeOverride(Size finalSize) noexcept {
    for (LayoutElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Point position = ChildPosition(*child);
        const Size desired = child->DesiredSize();
        Base::Result<void> arranged = ArrangeChild(*child,
            {position.x, position.y, desired.width, desired.height});
        if (!arranged) return arranged.GetStatus();
    }
    return finalSize;
}

Grid::Grid(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
    TypeId runtimeType, Base::IAllocator* allocator) noexcept
    : LayoutElement(dispatcher, registry, runtimeType, allocator),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      columns_(allocator_),
      rows_(allocator_),
      cells_(allocator_),
      desiredColumns_(allocator_),
      desiredRows_(allocator_) {}

Base::Result<void> Grid::SetColumnDefinitions(
    Base::Span<const GridLength> definitions) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    Base::Result<void> valid = ValidateDefinitions(definitions);
    if (!valid) return valid;
    const std::uint32_t count = definitions.Empty() ? 1U : definitions.Size();
    for (const Cell& cell : cells_) {
        if (cell.column >= count) {
            return Base::Status::Failure(Base::ErrorCode::OutOfRange,
                "Grid column definition removal would orphan a child cell");
        }
    }
    Base::Vector<GridLength> next(allocator_);
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
    for (const Cell& cell : cells_) {
        if (cell.row >= count) {
            return Base::Status::Failure(Base::ErrorCode::OutOfRange,
                "Grid row definition removal would orphan a child cell");
        }
    }
    Base::Vector<GridLength> next(allocator_);
    Base::Result<void> copied = next.TryAssign(definitions);
    if (!copied) return copied.GetStatus();
    rows_ = std::move(next);
    return InvalidateMeasure();
}

Base::Result<void> Grid::SetChildCell(
    LayoutElement& child, std::uint32_t row, std::uint32_t column) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (row >= RowCount() || column >= ColumnCount()) {
        return Base::Status::Failure(Base::ErrorCode::OutOfRange,
            "Grid child cell is outside the declared track range");
    }
    bool attached = false;
    for (LayoutElement* current : LayoutChildren()) {
        attached = attached || current == &child;
    }
    if (!attached) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Grid child must be attached before assigning a cell");
    }
    for (Cell& cell : cells_) {
        if (cell.child == &child) {
            cell.row = row;
            cell.column = column;
            return InvalidateMeasure();
        }
    }
    Base::Result<void> added = cells_.TryPushBack({&child, row, column});
    if (!added) return added.GetStatus();
    return InvalidateMeasure();
}

Base::Result<Size> Grid::MeasureOverride(Size) noexcept {
    const std::uint32_t columns = ColumnCount();
    const std::uint32_t rows = RowCount();
    Base::Vector<double> desiredColumns(allocator_);
    Base::Vector<double> desiredRows(allocator_);
    Base::Result<void> resized = desiredColumns.TryResize(columns, 0.0);
    if (!resized) return resized.GetStatus();
    resized = desiredRows.TryResize(rows, 0.0);
    if (!resized) return resized.GetStatus();

    for (std::uint32_t index = 0U; index < columns; ++index) {
        const GridLength definition = ColumnAt(index);
        if (definition.unit == GridUnitType::Pixel) desiredColumns[index] = definition.value;
    }
    for (std::uint32_t index = 0U; index < rows; ++index) {
        const GridLength definition = RowAt(index);
        if (definition.unit == GridUnitType::Pixel) desiredRows[index] = definition.value;
    }

    constexpr double Unconstrained = 1.0e12;
    for (LayoutElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Cell* cell = FindCell(*child);
        const std::uint32_t row = cell != nullptr ? cell->row : 0U;
        const std::uint32_t column = cell != nullptr ? cell->column : 0U;
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
        if (columnDefinition.unit != GridUnitType::Pixel) {
            desiredColumns[column] = std::max(desiredColumns[column], childDesired.width);
        }
        if (rowDefinition.unit != GridUnitType::Pixel) {
            desiredRows[row] = std::max(desiredRows[row], childDesired.height);
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
    Base::Vector<double> columns(allocator_);
    Base::Vector<double> rows(allocator_);
    Base::Result<void> resolved = ResolveTracks(
        {columns_.Data(), columns_.Size()},
        {desiredColumns_.Data(), desiredColumns_.Size()}, finalSize.width, columns);
    if (!resolved) return resolved.GetStatus();
    resolved = ResolveTracks(
        {rows_.Data(), rows_.Size()},
        {desiredRows_.Data(), desiredRows_.Size()}, finalSize.height, rows);
    if (!resolved) return resolved.GetStatus();

    for (LayoutElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        const Cell* cell = FindCell(*child);
        const std::uint32_t row = cell != nullptr ? cell->row : 0U;
        const std::uint32_t column = cell != nullptr ? cell->column : 0U;
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

const Grid::Cell* Grid::FindCell(const LayoutElement& child) const noexcept {
    for (const Cell& cell : cells_) {
        if (cell.child == &child) return &cell;
    }
    return nullptr;
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
                resolved[index] = remaining * (definition.value / totalStarWeight);
            }
        }
    }
    return {};
}

Border::Border(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
    TypeId runtimeType, Base::IAllocator* allocator) noexcept
    : RenderElement(dispatcher, registry, runtimeType, allocator) {}

Base::Result<void> Border::SetBackground(Color value) noexcept {
    if (!IsValidColor(value)) return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Border color is invalid");
    background_ = value;
    return InvalidateRender();
}

Base::Result<void> Border::SetStroke(Color value, double thickness) noexcept {
    if (!IsValidColor(value) || !std::isfinite(thickness) || thickness < 0.0) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Border stroke is invalid");
    }
    stroke_ = value;
    strokeThickness_ = thickness;
    return InvalidateRender();
}

Base::Result<void> Border::SetPadding(Thickness value) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return access;
    if (!IsValidPadding(value)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Border padding must be finite, nonnegative, and non-overflowing");
    }
    if (SameThickness(padding_, value)) return {};
    padding_ = value;
    return InvalidateMeasure();
}

Base::Result<Size> Border::MeasureOverride(Size availableSize) noexcept {
    const Size childAvailable = Deflate(availableSize, padding_);
    Size desired;
    for (LayoutElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> measured = MeasureChild(*child, childAvailable);
        if (!measured) return measured.GetStatus();
        desired.width = std::max(desired.width, child->DesiredSize().width);
        desired.height = std::max(desired.height, child->DesiredSize().height);
    }
    return Inflate(desired, padding_);
}

Base::Result<Size> Border::ArrangeOverride(Size finalSize) noexcept {
    const Size childSize = Deflate(finalSize, padding_);
    for (LayoutElement* child : LayoutChildren()) {
        if (child == nullptr) continue;
        Base::Result<void> arranged = ArrangeChild(*child,
            {padding_.left, padding_.top, childSize.width, childSize.height});
        if (!arranged) return arranged.GetStatus();
    }
    return finalSize;
}

Base::Result<void> Border::BuildDisplayList(DisplayListBuilder& builder) noexcept {
    const Rect bounds{0.0, 0.0, RenderSize().width, RenderSize().height};
    Base::Result<void> fill = builder.FillRect(bounds, background_);
    if (!fill) return fill.GetStatus();
    if (strokeThickness_ > 0.0 && stroke_.alpha > 0.0F) {
        return builder.StrokeRect(bounds, stroke_, strokeThickness_);
    }
    return {};
}

ContentPresenter::ContentPresenter(Dispatcher& dispatcher,
    DependencyPropertyRegistry& registry, TypeId runtimeType,
    Base::IAllocator* allocator) noexcept
    : RenderElement(dispatcher, registry, runtimeType, allocator) {}

bool ContentPresenter::IsOnlyAttachedContent(
    const LayoutElement& content) const noexcept {
    const Base::Span<LayoutElement* const> children = LayoutChildren();
    return children.Size() == 1U && children[0] == &content;
}

Base::Result<void> ContentPresenter::SetContent(
    LayoutElement* content) noexcept {
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
    LayoutElement& content) noexcept {
    if (!contentObject || contentObject.Get() != &content) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "ContentPresenter owned content does not match its layout object");
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
    LayoutElement* content) const noexcept {
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

Base::Result<Size> ContentPresenter::ArrangeOverride(
    Size finalSize) noexcept {
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
