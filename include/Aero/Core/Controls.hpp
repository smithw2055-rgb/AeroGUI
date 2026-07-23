#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/Core/Rendering.hpp>

namespace Aero::Core {

struct MetaRegistrationContext;

enum class Orientation : std::uint8_t { Horizontal = 0U, Vertical };

// First production Panel: lays out visual/layout children sequentially in a
// single axis. Manual use keeps ownership outside the panel; XAML collection
// content uses AddOwnedChild() and must be released after its tree edges have
// been detached by XamlVisualTreeHost.
class AERO_API StackPanel final : public RenderElement {
    AERO_DECLARE_METADATA(StackPanel, RenderElement)
public:
    StackPanel() noexcept;
    explicit StackPanel(Orientation orientation) noexcept;

    Orientation GetOrientation() const noexcept;
    Base::Result<void> SetOrientation(Orientation value) noexcept;
    std::uint32_t OwnedChildCount() const noexcept {
        return ownedChildren_.Size();
    }
    Base::Result<void> AddOwnedChild(
        const Base::Ref<Base::Object>& childObject,
        LayoutElement& child) noexcept;
    Base::Result<void> ClearOwnedChildren() noexcept;

    // Dependency properties
    AERO_DECLARE_DEPENDENCY_PROPERTY(Orientation);

protected:
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;

private:
    Base::Vector<Base::Ref<Base::Object>> ownedChildren_;

    bool IsOwnedChild(const LayoutElement& child) const noexcept;
};

// Canvas has no visual of its own, but remains a RenderElement so a child
// loaded through XAML stays connected to the render tree.
class AERO_API Canvas final : public RenderElement {
    AERO_DECLARE_METADATA(Canvas, RenderElement)
public:
    Canvas() noexcept;

    Base::Result<void> SetChildPosition(
        LayoutElement& child, Point position) noexcept;
    Point ChildPosition(const LayoutElement& child) const noexcept;

    // Dependency properties
    AERO_DECLARE_DEPENDENCY_PROPERTY(Left);
    AERO_DECLARE_DEPENDENCY_PROPERTY(Top);

protected:
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;

};

enum class GridUnitType : std::uint8_t { Auto = 0U, Pixel, Star };

struct GridLength final {
    double value = 1.0;
    GridUnitType unit = GridUnitType::Star;

    static constexpr GridLength Auto() noexcept {
        return {0.0, GridUnitType::Auto};
    }
    static constexpr GridLength Pixel(double value) noexcept {
        return {value, GridUnitType::Pixel};
    }
    static constexpr GridLength Star(double weight = 1.0) noexcept {
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
    AERO_DECLARE_METADATA(Grid, RenderElement)
public:
    Grid() noexcept;

    Base::Result<void> SetColumnDefinitions(
        Base::Span<const GridLength> definitions) noexcept;
    Base::Result<void> SetRowDefinitions(
        Base::Span<const GridLength> definitions) noexcept;
    Base::Result<void> SetChildCell(
        LayoutElement& child, std::uint32_t row, std::uint32_t column) noexcept;

    Base::Span<const GridLength> ColumnDefinitions() const noexcept {
        return {columns_.Data(), columns_.Size()};
    }
    Base::Span<const GridLength> RowDefinitions() const noexcept {
        return {rows_.Data(), rows_.Size()};
    }

    // Dependency properties
    AERO_DECLARE_DEPENDENCY_PROPERTY(Row);
    AERO_DECLARE_DEPENDENCY_PROPERTY(Column);

protected:
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;

private:
    Base::Vector<GridLength> columns_;
    Base::Vector<GridLength> rows_;
    Base::Vector<double> desiredColumns_;
    Base::Vector<double> desiredRows_;

    std::uint32_t ColumnCount() const noexcept;
    std::uint32_t RowCount() const noexcept;
    GridLength ColumnAt(std::uint32_t index) const noexcept;
    GridLength RowAt(std::uint32_t index) const noexcept;
    Base::Result<void> ValidateDefinitions(
        Base::Span<const GridLength> definitions) const noexcept;
    std::uint32_t ChildRow(const LayoutElement& child) const noexcept;
    std::uint32_t ChildColumn(const LayoutElement& child) const noexcept;
    Base::Result<void> ResolveTracks(
        Base::Span<const GridLength> definitions,
        Base::Span<const double> desired,
        double available,
        Base::Vector<double>& resolved) const noexcept;
};

class AERO_API Border : public RenderElement {
    AERO_DECLARE_METADATA(Border, RenderElement)
public:
    Border() noexcept;
    Base::Result<void> SetBackground(Color value) noexcept;
    Base::Result<void> SetStroke(Color value, double thickness) noexcept;
    Base::Result<void> SetBorderBrush(Color value) noexcept;
    Base::Result<void> SetBorderThickness(double value) noexcept;
    Base::Result<void> SetPadding(Thickness value) noexcept;
    Color Background() const noexcept;
    Color BorderBrush() const noexcept;
    double BorderThickness() const noexcept;
    Thickness Padding() const noexcept;

    // Dependency properties
    AERO_DECLARE_DEPENDENCY_PROPERTY(Background);
    AERO_DECLARE_DEPENDENCY_PROPERTY(BorderBrush);
    AERO_DECLARE_DEPENDENCY_PROPERTY(BorderThickness);
    AERO_DECLARE_DEPENDENCY_PROPERTY(Padding);

protected:
    explicit Border(TypeId runtimeType) noexcept;
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;
    Base::Result<void> BuildDisplayList(DisplayListBuilder& builder) noexcept override;

};

// M2 text control seam. Text shaping and atlas allocation remain provider
// responsibilities; once a provider has registered an immutable glyph run
// with the active render backend, TextBlock keeps the logical UTF-8 content,
// participates in layout, and emits that run with its foreground tint.
class AERO_API TextBlock final : public RenderElement {
    AERO_DECLARE_METADATA(TextBlock, RenderElement)
public:
    TextBlock() noexcept;

    Base::StringView Text() const noexcept;
    Color Foreground() const noexcept;
    RenderGlyphRunId GlyphRun() const noexcept { return glyphRun_; }
    Size GlyphRunSize() const noexcept { return glyphRunSize_; }
    Base::Result<void> SetText(Base::StringView value) noexcept;
    Base::Result<void> SetForeground(Color value) noexcept;

    // Dependency properties
    AERO_DECLARE_DEPENDENCY_PROPERTY(Text);
    AERO_DECLARE_DEPENDENCY_PROPERTY(Foreground);

    // A run id of InvalidRenderGlyphRunId clears the shaped presentation and
    // requires a zero size. The id must be registered with the render backend
    // before a frame containing this TextBlock is submitted.
    Base::Result<void> SetGlyphRun(
        RenderGlyphRunId glyphRun, Size size) noexcept;

protected:
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<void> BuildDisplayList(
        DisplayListBuilder& builder) noexcept override;

private:
    RenderGlyphRunId glyphRun_ = InvalidRenderGlyphRunId;
    Size glyphRunSize_;
};

// Single-child visual presenter. It does not attach or detach content itself:
// those mutations remain transactional through ObjectTree, LayoutManager, and
// RenderManager. XAML loaders may retain the content object with
// SetOwnedContent(), but the host must still Unmount() before releasing the
// root object.
class AERO_API ContentPresenter final : public RenderElement {
    AERO_DECLARE_METADATA(ContentPresenter, RenderElement)
public:
    ContentPresenter() noexcept;

    LayoutElement* Content() const noexcept { return content_; }
    const Base::Ref<Base::Object>& OwnedContent() const noexcept {
        return ownedContent_;
    }
    Base::Result<void> SetContent(
        LayoutElement* content) noexcept;
    Base::Result<void> SetOwnedContent(
        const Base::Ref<Base::Object>& contentObject,
        LayoutElement& content) noexcept;

protected:
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;

private:
    LayoutElement* content_ = nullptr;
    Base::Ref<Base::Object> ownedContent_;

    bool IsOnlyAttachedContent(
        const LayoutElement& content) const noexcept;
    Base::Result<void> ValidateContent(
        LayoutElement* content) const noexcept;
};

} // namespace Aero::Core
