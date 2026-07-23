#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/Core/ControlPrimitives.hpp>

namespace Aero::Core {

struct MetaRegistrationContext;

enum class Orientation : std::uint8_t { Horizontal = 0U, Vertical };

class AERO_API StackPanel final : public Panel {
    AERO_DECLARE_METADATA(StackPanel, Panel)
public:
    StackPanel() noexcept;
    explicit StackPanel(Orientation orientation) noexcept;
    Orientation GetOrientation() const noexcept;
    Base::Result<void> SetOrientation(Orientation value) noexcept;
    AERO_DECLARE_DEPENDENCY_PROPERTY(Orientation);
protected:
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;
};

class AERO_API Canvas final : public Panel {
    AERO_DECLARE_METADATA(Canvas, Panel)
public:
    Canvas() noexcept;
    Base::Result<void> SetChildPosition(UIElement& child, Point position) noexcept;
    Point ChildPosition(const UIElement& child) const noexcept;
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
    static constexpr GridLength Auto() noexcept { return {0.0, GridUnitType::Auto}; }
    static constexpr GridLength Pixel(double value) noexcept { return {value, GridUnitType::Pixel}; }
    static constexpr GridLength Star(double weight = 1.0) noexcept { return {weight, GridUnitType::Star}; }
};

class AERO_API Grid final : public Panel {
    AERO_DECLARE_METADATA(Grid, Panel)
public:
    Grid() noexcept;
    Base::Result<void> SetColumnDefinitions(Base::Span<const GridLength> definitions) noexcept;
    Base::Result<void> SetRowDefinitions(Base::Span<const GridLength> definitions) noexcept;
    Base::Result<void> SetChildCell(UIElement& child, std::uint32_t row, std::uint32_t column) noexcept;
    Base::Span<const GridLength> ColumnDefinitions() const noexcept { return {columns_.Data(), columns_.Size()}; }
    Base::Span<const GridLength> RowDefinitions() const noexcept { return {rows_.Data(), rows_.Size()}; }
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
    Base::Result<void> ValidateDefinitions(Base::Span<const GridLength> definitions) const noexcept;
    std::uint32_t ChildRow(const UIElement& child) const noexcept;
    std::uint32_t ChildColumn(const UIElement& child) const noexcept;
    Base::Result<void> ResolveTracks(Base::Span<const GridLength> definitions,
        Base::Span<const double> desired, double available,
        Base::Vector<double>& resolved) const noexcept;
};

class AERO_API Border : public Decorator {
    AERO_DECLARE_METADATA(Border, Decorator)
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
    AERO_DECLARE_DEPENDENCY_PROPERTY(Background);
    AERO_DECLARE_DEPENDENCY_PROPERTY(BorderBrush);
    AERO_DECLARE_DEPENDENCY_PROPERTY(BorderThickness);
    AERO_DECLARE_DEPENDENCY_PROPERTY(Padding);
protected:
    explicit Border(TypeId runtimeType) noexcept : Decorator(runtimeType) {}
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;
    Base::Result<void> BuildDisplayList(DisplayListBuilder& builder) noexcept override;
};

class AERO_API TextBlock final : public FrameworkElement {
    AERO_DECLARE_METADATA(TextBlock, FrameworkElement)
public:
    TextBlock() noexcept;
    Base::StringView Text() const noexcept;
    Color Foreground() const noexcept;
    RenderGlyphRunId GlyphRun() const noexcept { return glyphRun_; }
    Size GlyphRunSize() const noexcept { return glyphRunSize_; }
    Base::Result<void> SetText(Base::StringView value) noexcept;
    Base::Result<void> SetForeground(Color value) noexcept;
    AERO_DECLARE_DEPENDENCY_PROPERTY(Text);
    AERO_DECLARE_DEPENDENCY_PROPERTY(Foreground);
    Base::Result<void> SetGlyphRun(RenderGlyphRunId glyphRun, Size size) noexcept;
protected:
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<void> BuildDisplayList(DisplayListBuilder& builder) noexcept override;
private:
    RenderGlyphRunId glyphRun_ = InvalidRenderGlyphRunId;
    Size glyphRunSize_;
};

class AERO_API ContentPresenter final : public FrameworkElement {
    AERO_DECLARE_METADATA(ContentPresenter, FrameworkElement)
public:
    ContentPresenter() noexcept;
    UIElement* Content() const noexcept { return content_; }
    const Base::Ref<Base::Object>& OwnedContent() const noexcept { return ownedContent_; }
    Base::Result<void> SetContent(UIElement* content) noexcept;
    Base::Result<void> SetOwnedContent(const Base::Ref<Base::Object>& contentObject,
        UIElement& content) noexcept;
protected:
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;
private:
    UIElement* content_ = nullptr;
    Base::Ref<Base::Object> ownedContent_;
    bool IsOnlyAttachedContent(const UIElement& content) const noexcept;
    Base::Result<void> ValidateContent(UIElement* content) const noexcept;
};

} // namespace Aero::Core
