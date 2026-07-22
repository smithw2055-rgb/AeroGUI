#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/Core/Rendering.hpp>

namespace Aero::Core {

enum class Orientation : std::uint8_t { Horizontal = 0U, Vertical };

// First production Panel: lays out visual/layout children sequentially in a
// single axis. Manual use keeps ownership outside the panel; XAML collection
// content uses AddOwnedChild() and must be released after its tree edges have
// been detached by XamlVisualTreeHost.
class AERO_API StackPanel final : public RenderElement {
public:
    StackPanel(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId runtimeType, Orientation orientation = Orientation::Vertical,
        Base::IAllocator* allocator = nullptr) noexcept;

    AERO_NODISCARD Orientation GetOrientation() const noexcept { return orientation_; }
    AERO_NODISCARD Base::Result<void> SetOrientation(Orientation value) noexcept;
    AERO_NODISCARD std::uint32_t OwnedChildCount() const noexcept {
        return ownedChildren_.Size();
    }
    AERO_NODISCARD Base::Result<void> AddOwnedChild(
        const Base::Ref<Base::Object>& childObject,
        LayoutElement& child) noexcept;
    AERO_NODISCARD Base::Result<void> ClearOwnedChildren() noexcept;

protected:
    AERO_NODISCARD Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    AERO_NODISCARD Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;

private:
    Orientation orientation_ = Orientation::Vertical;
    Base::Vector<Base::Ref<Base::Object>> ownedChildren_;

    AERO_NODISCARD bool IsOwnedChild(const LayoutElement& child) const noexcept;
};

// Canvas has no visual of its own, but remains a RenderElement so a child
// loaded through XAML stays connected to the render tree.
class AERO_API Canvas final : public RenderElement {
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
// Grid likewise acts as a transparent render-tree container for its children.
class AERO_API Grid final : public RenderElement {
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
    AERO_NODISCARD Color Background() const noexcept { return background_; }
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

// M2 text control seam. Text shaping and atlas allocation remain provider
// responsibilities; once a provider has registered an immutable glyph run
// with the active render backend, TextBlock keeps the logical UTF-8 content,
// participates in layout, and emits that run with its foreground tint.
class AERO_API TextBlock final : public RenderElement {
public:
    TextBlock(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId runtimeType, Base::IAllocator* allocator = nullptr) noexcept;

    AERO_NODISCARD Base::StringView Text() const noexcept { return text_.View(); }
    AERO_NODISCARD Color Foreground() const noexcept { return foreground_; }
    AERO_NODISCARD RenderGlyphRunId GlyphRun() const noexcept { return glyphRun_; }
    AERO_NODISCARD Size GlyphRunSize() const noexcept { return glyphRunSize_; }
    AERO_NODISCARD Base::Result<void> SetText(Base::StringView value) noexcept;
    AERO_NODISCARD Base::Result<void> SetForeground(Color value) noexcept;
    // A run id of InvalidRenderGlyphRunId clears the shaped presentation and
    // requires a zero size. The id must be registered with the render backend
    // before a frame containing this TextBlock is submitted.
    AERO_NODISCARD Base::Result<void> SetGlyphRun(
        RenderGlyphRunId glyphRun, Size size) noexcept;

protected:
    AERO_NODISCARD Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    AERO_NODISCARD Base::Result<void> BuildDisplayList(
        DisplayListBuilder& builder) noexcept override;

private:
    Base::String text_;
    Color foreground_{0.0F, 0.0F, 0.0F, 1.0F};
    RenderGlyphRunId glyphRun_ = InvalidRenderGlyphRunId;
    Size glyphRunSize_;
};

// Single-child visual presenter. It does not attach or detach content itself:
// those mutations remain transactional through ObjectTree, LayoutManager, and
// RenderManager. XAML loaders may retain the content object with
// SetOwnedContent(), but the host must still Unmount() before releasing the
// root object.
class AERO_API ContentPresenter final : public RenderElement {
public:
    ContentPresenter(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId runtimeType, Base::IAllocator* allocator = nullptr) noexcept;

    AERO_NODISCARD LayoutElement* Content() const noexcept { return content_; }
    AERO_NODISCARD const Base::Ref<Base::Object>& OwnedContent() const noexcept {
        return ownedContent_;
    }
    AERO_NODISCARD Base::Result<void> SetContent(
        LayoutElement* content) noexcept;
    AERO_NODISCARD Base::Result<void> SetOwnedContent(
        const Base::Ref<Base::Object>& contentObject,
        LayoutElement& content) noexcept;

protected:
    AERO_NODISCARD Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    AERO_NODISCARD Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;

private:
    LayoutElement* content_ = nullptr;
    Base::Ref<Base::Object> ownedContent_;

    AERO_NODISCARD bool IsOnlyAttachedContent(
        const LayoutElement& content) const noexcept;
    AERO_NODISCARD Base::Result<void> ValidateContent(
        LayoutElement* content) const noexcept;
};

} // namespace Aero::Core
