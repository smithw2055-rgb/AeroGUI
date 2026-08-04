#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/Input.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Media/Transforms.hpp>
#include <cstddef>
#include <Aero/Controls/Core.hpp>

namespace Aero::Documents {
class InlineCollection;
class InlineCollectionView;
class TextPointer;
}
namespace Aero::Controls {

using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::TypeId;
using ::Aero::Media::Brush;
using ::Aero::Media::FrameworkElementForegroundProperty;
using ::Aero::Media::MatrixTransform;
using ::Aero::Media::Transform;

enum class Orientation : std::uint8_t { Horizontal = 0U, Vertical };
enum class Dock : std::uint8_t { Left = 0U, Top, Right, Bottom };

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::TextWrapping)

AERO_DECLARE_TYPE_ENUM(Aero::TextTrimming)

AERO_DECLARE_TYPE_ENUM(Aero::TextAlignment)

AERO_DECLARE_TYPE_ENUM(Aero::FontStyle)

AERO_DECLARE_TYPE_ENUM(Aero::Controls::Orientation)

AERO_DECLARE_TYPE_ENUM(Aero::Controls::Dock)

namespace Aero::Controls {


class AERO_API StackPanel : public Panel {
    AERO_DECLARE_TYPE(StackPanel, Panel)
public:
    StackPanel() noexcept;
    explicit StackPanel(Orientation orientation) noexcept;
    Orientation GetOrientation() const noexcept;
    void SetOrientation(Orientation value) noexcept;
    inline static constexpr Members::Property<Orientation> OrientationProperty{"Orientation"};
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

class AERO_API DockPanel : public Panel {
    AERO_DECLARE_TYPE(DockPanel, Panel)
public:
    DockPanel() noexcept : Panel(StaticTypeId()) {}
    bool GetLastChildFill() const noexcept;
    void SetLastChildFill(bool value) noexcept;
    void SetChildDock(
        UIElement& child, Dock value) noexcept;
    Dock GetChildDock(const UIElement& child) const noexcept;
    inline static constexpr Members::Property<bool> LastChildFillProperty{"LastChildFill"};
    inline static constexpr Members::AttachedProperty<Dock> DockProperty{"Dock"};
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

class AERO_API WrapPanel : public Panel {
    AERO_DECLARE_TYPE(WrapPanel, Panel)
public:
    WrapPanel() noexcept : Panel(StaticTypeId()) {}
    Orientation GetOrientation() const noexcept;
    void SetOrientation(Orientation value) noexcept;
    double GetItemWidth() const noexcept;
    double GetItemHeight() const noexcept;
    void SetItemWidth(double value) noexcept;
    void SetItemHeight(double value) noexcept;
    inline static constexpr Members::Property<Orientation> OrientationProperty{"Orientation"};
    // Zero selects the child's desired dimension.
    inline static constexpr Members::Property<double> ItemWidthProperty{"ItemWidth"};
    inline static constexpr Members::Property<double> ItemHeightProperty{"ItemHeight"};
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

class AERO_API UniformGrid : public Panel {
    AERO_DECLARE_TYPE(UniformGrid, Panel)
public:
    UniformGrid() noexcept : Panel(StaticTypeId()) {}
    std::uint32_t GetRows() const noexcept;
    std::uint32_t GetColumns() const noexcept;
    std::uint32_t GetFirstColumn() const noexcept;
    void SetRows(std::uint32_t value) noexcept;
    void SetColumns(std::uint32_t value) noexcept;
    void SetFirstColumn(
        std::uint32_t value) noexcept;
    inline static constexpr Members::Property<std::uint32_t> RowsProperty{"Rows"};
    inline static constexpr Members::Property<std::uint32_t> ColumnsProperty{"Columns"};
    inline static constexpr Members::Property<std::uint32_t> FirstColumnProperty{"FirstColumn"};
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
private:
    void ResolveDimensions(
        std::uint32_t childCount,
        std::uint32_t& rows,
        std::uint32_t& columns) const noexcept;
};

class AERO_API Canvas : public Panel {
    AERO_DECLARE_TYPE(Canvas, Panel)
public:
    Canvas() noexcept;
    void SetChildPosition(UIElement& child, Point position) noexcept;
    Point GetChildPosition(const UIElement& child) const noexcept;
    inline static constexpr Members::AttachedProperty<double> LeftProperty{"Left"};
    inline static constexpr Members::AttachedProperty<double> TopProperty{"Top"};
    inline static constexpr Members::AttachedProperty<double> RightProperty{"Right"};
    inline static constexpr Members::AttachedProperty<double> BottomProperty{"Bottom"};
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

enum class GridUnitType : std::uint8_t { Auto = 0U, Pixel, Star };

struct GridLength {
    double value = 1.0;
    GridUnitType unit = GridUnitType::Star;
    static constexpr GridLength Auto() noexcept { return {0.0, GridUnitType::Auto}; }
    static constexpr GridLength Pixel(double value) noexcept { return {value, GridUnitType::Pixel}; }
    static constexpr GridLength Star(double weight = 1.0) noexcept { return {weight, GridUnitType::Star}; }
};

class AERO_API ColumnDefinition : public Base::Object {
    AERO_DECLARE_TYPE(ColumnDefinition, Base::Object)
public:
    ColumnDefinition() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    GridLength GetWidth() const noexcept { return width_; }
    double GetMaxWidth() const noexcept { return maxWidth_; }
    Base::StringView GetSharedSizeGroup() const noexcept {
        return sharedSizeGroup_.View();
    }
    void SetWidth(GridLength value) noexcept;
    void SetMaxWidth(double value) noexcept;
    void SetSharedSizeGroup(
        Base::StringView value) noexcept;
private:
    GridLength width_ = GridLength::Star();
    double maxWidth_ = 1.0e12;
    Base::String sharedSizeGroup_;
};

class AERO_API RowDefinition : public Base::Object {
    AERO_DECLARE_TYPE(RowDefinition, Base::Object)
public:
    RowDefinition() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    GridLength GetHeight() const noexcept { return height_; }
    double GetMaxHeight() const noexcept { return maxHeight_; }
    Base::StringView GetSharedSizeGroup() const noexcept {
        return sharedSizeGroup_.View();
    }
    void SetHeight(GridLength value) noexcept;
    void SetMaxHeight(double value) noexcept;
    void SetSharedSizeGroup(
        Base::StringView value) noexcept;
private:
    GridLength height_ = GridLength::Star();
    double maxHeight_ = 1.0e12;
    Base::String sharedSizeGroup_;
};

class AERO_API Grid : public Panel {
    AERO_DECLARE_TYPE(Grid, Panel)
public:
    Grid() noexcept;
    void SetColumnDefinitions(Base::Span<const GridLength> definitions) noexcept;
    void SetRowDefinitions(Base::Span<const GridLength> definitions) noexcept;
    void SetChildCell(UIElement& child, std::uint32_t row, std::uint32_t column) noexcept;
    void SetChildCell(
        UIElement& child,
        std::uint32_t row,
        std::uint32_t column,
        std::uint32_t rowSpan,
        std::uint32_t columnSpan) noexcept;
    Base::Result<void> AddColumnDefinition(
        Base::Ref<ColumnDefinition> definition) noexcept;
    Base::Result<void> AddRowDefinition(
        Base::Ref<RowDefinition> definition) noexcept;
    void ClearColumnDefinitionObjects() noexcept;
    void ClearRowDefinitionObjects() noexcept;
    Base::Result<void> AddInputBinding(
        Base::Ref<Aero::Input::KeyBinding> binding) noexcept;
    void ClearInputBindings() noexcept { inputBindings_.Clear(); }
    Base::Span<const Base::Ref<Aero::Input::KeyBinding>>
    GetInputBindings() const noexcept {
        return {inputBindings_.Data(), inputBindings_.Size()};
    }
    Base::StringView GetColumnDefinitionsText() const noexcept;
    Base::StringView GetRowDefinitionsText() const noexcept;
    void SetColumnDefinitionsText(
        Base::StringView value) noexcept;
    void SetRowDefinitionsText(
        Base::StringView value) noexcept;
    Base::Span<const GridLength> GetColumnDefinitions() const noexcept { return {columns_.Data(), columns_.Size()}; }
    Base::Span<const GridLength> GetRowDefinitions() const noexcept { return {rows_.Data(), rows_.Size()}; }
    inline static constexpr Members::AttachedProperty<std::uint32_t> RowProperty{"Row"};
    inline static constexpr Members::AttachedProperty<std::uint32_t> ColumnProperty{"Column"};
    inline static constexpr Members::AttachedProperty<std::uint32_t> RowSpanProperty{"RowSpan"};
    inline static constexpr Members::AttachedProperty<std::uint32_t> ColumnSpanProperty{"ColumnSpan"};
    inline static constexpr Members::AttachedProperty<bool> IsSharedSizeScopeProperty{"IsSharedSizeScope"};
    // Programmatic compact form; WPF XAML uses the structural
    // ColumnDefinitions and RowDefinitions collections.
    inline static constexpr Members::Property<Base::String> ColumnDefinitionsTextProperty{"ColumnDefinitionsText"};
    inline static constexpr Members::Property<Base::String> RowDefinitionsTextProperty{"RowDefinitionsText"};
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
private:
    Base::Vector<GridLength> columns_;
    Base::Vector<GridLength> rows_;
    Base::Vector<Base::Ref<ColumnDefinition>>
        columnDefinitionObjects_;
    Base::Vector<Base::Ref<RowDefinition>>
        rowDefinitionObjects_;
    Base::Vector<Base::Ref<Aero::Input::KeyBinding>>
        inputBindings_;
    Base::Vector<double> desiredColumns_;
    Base::Vector<double> desiredRows_;
    std::uint32_t GetColumnCount() const noexcept;
    std::uint32_t GetRowCount() const noexcept;
    GridLength ColumnAt(std::uint32_t index) const noexcept;
    GridLength RowAt(std::uint32_t index) const noexcept;
    Base::Result<void> ValidateDefinitions(Base::Span<const GridLength> definitions) const noexcept;
    std::uint32_t GetChildRow(const UIElement& child) const noexcept;
    std::uint32_t GetChildColumn(const UIElement& child) const noexcept;
    std::uint32_t GetChildRowSpan(const UIElement& child) const noexcept;
    std::uint32_t GetChildColumnSpan(const UIElement& child) const noexcept;
    Base::Result<void> ResolveTracks(Base::Span<const GridLength> definitions,
        Base::Span<const double> desired, double available,
        Base::Vector<double>& resolved) const noexcept;
};

// WPF-shaped single-child scaling decorator. The child participates in
// layout at its natural size and is then fitted into the Viewbox slot.
class AERO_API Viewbox : public Decorator {
    AERO_DECLARE_TYPE(Viewbox, Decorator)
public:
    Viewbox() noexcept : Decorator(StaticTypeId()) {}

    Stretch GetStretch() const noexcept;
    StretchDirection GetStretchDirection() const noexcept;
    void SetStretch(Stretch value) noexcept;
    void SetStretchDirection(
        StretchDirection value) noexcept;

    inline static constexpr Members::Property<Stretch> StretchProperty{"Stretch"};
    inline static constexpr Members::Property<StretchDirection> StretchDirectionProperty{"StretchDirection"};

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    Base::Ref<MatrixTransform> viewTransform_;
    Base::Result<void> ApplyViewTransform(
        double scaleX,
        double scaleY,
        double offsetX,
        double offsetY) noexcept;
};

class AERO_API Border : public Decorator {
    AERO_DECLARE_TYPE(Border, Decorator)
public:
    Border() noexcept;
    void SetBackground(
        Base::Ref<Brush> value) noexcept;
    void SetBorderBrush(
        Base::Ref<Brush> value) noexcept;
    void SetBorderThickness(
        Thickness value) noexcept;
    void SetBorderThickness(
        double value) noexcept;
    void SetCornerRadius(
        CornerRadius value) noexcept;
    void SetCornerRadius(
        double value) noexcept;
    void SetPadding(Thickness value) noexcept;
    Base::Ref<Brush> GetBackground() const noexcept;
    Base::Ref<Brush> GetBorderBrush() const noexcept;
    Thickness GetBorderThickness() const noexcept;
    CornerRadius GetCornerRadius() const noexcept;
    Thickness GetPadding() const noexcept;
    inline static constexpr Members::Property<Base::Ref<Aero::Media::Brush>> BackgroundProperty{"Background"};
    inline static constexpr Members::Property<Base::Ref<Aero::Media::Brush>> BorderBrushProperty{"BorderBrush"};
    inline static constexpr Members::Property<Aero::Base::Thickness> BorderThicknessProperty{"BorderThickness"};
    inline static constexpr Members::Property<Aero::Base::CornerRadius> CornerRadiusProperty{"CornerRadius"};
    inline static constexpr Members::Property<Aero::Base::Thickness> PaddingProperty{"Padding"};
protected:
    explicit Border(TypeId runtimeType) noexcept : Decorator(runtimeType) {}
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    void OnRender(DrawingContext& context) noexcept override;
};

class AERO_API TextBlock : public FrameworkElement {
    AERO_DECLARE_TYPE(TextBlock, FrameworkElement)
public:
    struct Impl;

    TextBlock() noexcept;
    ~TextBlock() override;
    Base::StringView GetText() const noexcept;
    Base::Ref<Brush> GetForeground() const noexcept;
    Base::Ref<Brush> GetBackground() const noexcept;
    double GetFontSize() const noexcept;
    Base::Ref<Media::FontFamily> GetFontFamily() const noexcept;
    FontWeight GetFontWeight() const noexcept;
    FontStyle GetFontStyle() const noexcept;
    TextDecorations GetTextDecorations() const noexcept;
    TextWrapping GetTextWrapping() const noexcept;
    TextTrimming GetTextTrimming() const noexcept;
    TextAlignment GetTextAlignment() const noexcept;
    double GetLineHeight() const noexcept;
    std::uint32_t GetInlineCount() const noexcept {
        return ownedInlines_.Size();
    }
    Documents::InlineCollection GetInlines() noexcept;
    Documents::InlineCollectionView GetInlines() const noexcept;
    Documents::TextPointer GetContentStart() noexcept;
    Documents::TextPointer GetContentEnd() noexcept;
    Value GetMetadataInlines() const noexcept;
    void SetText(Base::StringView value) noexcept;
    void SetForeground(
        Base::Ref<Brush> value) noexcept;
    void SetBackground(
        Base::Ref<Brush> value) noexcept;
    void SetFontSize(double value) noexcept;
    void SetFontFamily(Base::Ref<Media::FontFamily> value) noexcept;
    Base::Result<void> SetFontFamily(Base::StringView value) noexcept;
    void SetFontWeight(
        FontWeight value) noexcept;
    void SetFontStyle(
        FontStyle value) noexcept;
    void SetTextDecorations(
        TextDecorations value) noexcept;
    void SetTextWrapping(
        TextWrapping value) noexcept;
    void SetTextTrimming(
        TextTrimming value) noexcept;
    void SetTextAlignment(
        TextAlignment value) noexcept;
    void SetLineHeight(double value) noexcept;
    void SetInlineValue(
        Value value) noexcept;
    Base::Result<void> AddOwnedInline(
        const Base::Ref<Base::Object>& inlineObject) noexcept;
    void ClearOwnedInlines() noexcept;
    inline static constexpr Members::Property<Base::String> TextProperty{"Text"};
    inline static constexpr auto ForegroundProperty = FrameworkElementForegroundProperty;
    inline static constexpr Members::Property<Base::Ref<Aero::Media::Brush>> BackgroundProperty{"Background"};
    inline static constexpr Members::Property<Base::Ref<Aero::Media::Brush>> StrokeProperty{"Stroke"};
    // WPF exposes the same inheritable text formatting property through
    // Control and TextBlock owners. Sharing the handle here gives generated
    // text content the ComboBoxItem/Control FontSize instead of falling back
    // to an unrelated TextBlock default.
    inline static constexpr auto FontSizeProperty = Control::FontSizeProperty;
    inline static constexpr auto FontFamilyProperty = FrameworkElement::FontFamilyProperty;
    inline static constexpr Members::Property<FontWeight> FontWeightProperty{"FontWeight"};
    inline static constexpr Members::Property<FontStyle> FontStyleProperty{"FontStyle"};
    inline static constexpr Members::Property<TextDecorations> TextDecorationsProperty{"TextDecorations"};
    inline static constexpr Members::Property<double> StrokeThicknessProperty{"StrokeThickness"};
    inline static constexpr Members::Property<TextWrapping> TextWrappingProperty{"TextWrapping"};
    inline static constexpr Members::Property<TextTrimming> TextTrimmingProperty{"TextTrimming"};
    inline static constexpr Members::Property<TextAlignment> TextAlignmentProperty{"TextAlignment"};
    inline static constexpr Members::Property<double> LineHeightProperty{"LineHeight"};
    inline static constexpr Members::Property<Thickness> PaddingProperty{"Padding"};
protected:
    explicit TextBlock(TypeId runtimeType) noexcept;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    void OnRender(DrawingContext& context) noexcept override;
private:
    friend struct ::Aero::Controls::Control::Impl;
    friend struct Impl;

    Base::StringView EffectiveFontFamily() const noexcept;
    void ReleaseServiceGlyphRun() noexcept;
    void SetGlyphRun(
        std::uint64_t glyphRun, Size size) noexcept;

    Base::Vector<std::uint64_t> glyphRuns_;
    Base::Vector<TextHitRegion> textHitRegions_;
    Base::Vector<Base::Ref<Base::Object>> ownedInlines_;
    Base::Ref<Base::Object> pendingInline_;
    Size glyphRunSize_;
    bool serviceOwnsGlyphRun_ = false;
};

class AERO_API ContentPresenter : public FrameworkElement {
    AERO_DECLARE_TYPE(ContentPresenter, FrameworkElement)
public:
    ContentPresenter() noexcept;
    UIElement* GetContent() const noexcept { return content_; }
    const Base::Ref<Base::Object>& GetOwnedContent() const noexcept { return ownedContent_; }
    const Value& GetContentValue() const noexcept {
        return contentValue_;
    }
    Base::StringView GetContentSource() const noexcept {
        return GetValueOr(
            ContentSourceProperty,
            Base::StringView{});
    }
    void SetContentSource(
        Base::StringView value) noexcept;
    void SetContentValue(
        Value value) noexcept {
        SetValue(ContentProperty, std::move(value));
    }
    void SetContent(UIElement* content) noexcept;

    // Template teardown is a two-step transaction: clear the presenter-owned
    // reference first, then detach the visual/layout/render edge through the Gui context. A nullptr literal selects this overload without weakening
    // the ordinary UIElement* validation path.
    void SetContent(std::nullptr_t) noexcept {
        Base::Result<void> access = VerifyAccess();
        if (!access) return;
        if (content_ == nullptr) return;
        if (!LayoutChildren().Empty() && !IsOnlyAttachedContent(*content_)) {
            return;
        }
        content_ = nullptr;
        ownedContent_.Reset();
        return;
    }

    void SetOwnedContent(const Base::Ref<Base::Object>& contentObject,
        UIElement& content) noexcept;

    inline static constexpr Members::Property<Base::String> ContentSourceProperty{"ContentSource"};
    inline static constexpr Members::Property<Value> ContentProperty{"Content"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ContentTemplateProperty{"ContentTemplate"};
    static void OnContentPropertyChanged(
        ::Aero::DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs&
            change) noexcept;
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
private:
    UIElement* content_ = nullptr;
    Base::Ref<Base::Object> ownedContent_;
    Value contentValue_ =
        Value::NullObject(
            Meta::TypeOf<Base::Object>());
    bool IsOnlyAttachedContent(const UIElement& content) const noexcept;
    Base::Result<void> ValidateContent(UIElement* content) const noexcept;
    Base::Result<void> UpdatePresentedText() noexcept;
};

} // namespace Aero::Controls

namespace Aero::Meta {

template<>
struct TypeTraits<Controls::GridLength> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("GridLength");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "GridLength";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Meta
