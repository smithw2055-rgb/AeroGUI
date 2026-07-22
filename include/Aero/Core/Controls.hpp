#pragma once

#include <Aero/Core/Rendering.hpp>

namespace Aero::Core {

enum class Orientation : std::uint8_t { Horizontal = 0U, Vertical };

// First production Panel: lays out visual/layout children sequentially in a
// single axis. Child ownership and attach/detach remain in ObjectTree and
// LayoutManager; StackPanel owns no child objects.
class AERO_API StackPanel final : public LayoutElement {
public:
    StackPanel(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId runtimeType, Orientation orientation = Orientation::Vertical,
        Base::IAllocator* allocator = nullptr) noexcept;

    AERO_NODISCARD Orientation GetOrientation() const noexcept { return orientation_; }
    AERO_NODISCARD Base::Result<void> SetOrientation(Orientation value) noexcept;

protected:
    AERO_NODISCARD Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    AERO_NODISCARD Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;

private:
    Orientation orientation_ = Orientation::Vertical;
};

class AERO_API Canvas final : public LayoutElement {
public:
    Canvas(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId runtimeType, Base::IAllocator* allocator = nullptr) noexcept;

    AERO_NODISCARD Base::Result<void> SetChildPosition(
        LayoutElement& child, Point position) noexcept;
    AERO_NODISCARD Point ChildPosition(const LayoutElement& child) const noexcept;

protected:
    AERO_NODISCARD Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    AERO_NODISCARD Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;

private:
    struct Position final { LayoutElement* child = nullptr; Point point; };
    Base::Vector<Position> positions_;
};

enum class GridUnitType : std::uint8_t { Auto = 0U, Pixel, Star };

struct GridLength final {
    double value = 1.0;
    GridUnitType unit = GridUnitType::Star;

    AERO_NODISCARD static constexpr GridLength Auto() noexcept {
        return {0.0, GridUnitType::Auto};
    }
    AERO_NODISCARD static constexpr GridLength Pixel(double value) noexcept {
        return {value, GridUnitType::Pixel};
    }
    AERO_NODISCARD static constexpr GridLength Star(double weight = 1.0) noexcept {
        return {weight, GridUnitType::Star};
    }
};

// M2 Grid core. A missing row/column definition represents a single implicit
// star track. Auto tracks take their largest child desired size; pixel tracks
// are fixed; remaining arrange space is apportioned between star tracks by
// weight. Row/column spans are deliberately deferred until the base track
// contract has conformance coverage.
class AERO_API Grid final : public LayoutElement {
public:
    Grid(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId runtimeType, Base::IAllocator* allocator = nullptr) noexcept;

    AERO_NODISCARD Base::Result<void> SetColumnDefinitions(
        Base::Span<const GridLength> definitions) noexcept;
    AERO_NODISCARD Base::Result<void> SetRowDefinitions(
        Base::Span<const GridLength> definitions) noexcept;
    AERO_NODISCARD Base::Result<void> SetChildCell(
        LayoutElement& child, std::uint32_t row, std::uint32_t column) noexcept;

    AERO_NODISCARD Base::Span<const GridLength> ColumnDefinitions() const noexcept {
        return {columns_.Data(), columns_.Size()};
    }
    AERO_NODISCARD Base::Span<const GridLength> RowDefinitions() const noexcept {
        return {rows_.Data(), rows_.Size()};
    }

protected:
    AERO_NODISCARD Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    AERO_NODISCARD Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;

private:
    struct Cell final {
        LayoutElement* child = nullptr;
        std::uint32_t row = 0U;
        std::uint32_t column = 0U;
    };

    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<GridLength> columns_;
    Base::Vector<GridLength> rows_;
    Base::Vector<Cell> cells_;
    Base::Vector<double> desiredColumns_;
    Base::Vector<double> desiredRows_;

    AERO_NODISCARD std::uint32_t ColumnCount() const noexcept;
    AERO_NODISCARD std::uint32_t RowCount() const noexcept;
    AERO_NODISCARD GridLength ColumnAt(std::uint32_t index) const noexcept;
    AERO_NODISCARD GridLength RowAt(std::uint32_t index) const noexcept;
    AERO_NODISCARD Base::Result<void> ValidateDefinitions(
        Base::Span<const GridLength> definitions) const noexcept;
    AERO_NODISCARD const Cell* FindCell(const LayoutElement& child) const noexcept;
    AERO_NODISCARD Base::Result<void> ResolveTracks(
        Base::Span<const GridLength> definitions,
        Base::Span<const double> desired,
        double available,
        Base::Vector<double>& resolved) const noexcept;
};

class AERO_API Border : public RenderElement {
public:
    Border(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId runtimeType, Base::IAllocator* allocator = nullptr) noexcept;
    AERO_NODISCARD Base::Result<void> SetBackground(Color value) noexcept;
    AERO_NODISCARD Base::Result<void> SetStroke(Color value, double thickness) noexcept;
    AERO_NODISCARD Base::Result<void> SetPadding(Thickness value) noexcept;
    AERO_NODISCARD Thickness Padding() const noexcept { return padding_; }

protected:
    AERO_NODISCARD Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    AERO_NODISCARD Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;
    AERO_NODISCARD Base::Result<void> BuildDisplayList(DisplayListBuilder& builder) noexcept override;

private:
    Color background_;
    Color stroke_{0.0F, 0.0F, 0.0F, 0.0F};
    double strokeThickness_ = 0.0;
    Thickness padding_;
};

} // namespace Aero::Core
