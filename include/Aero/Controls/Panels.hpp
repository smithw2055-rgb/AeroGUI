#pragma once

#include <Aero/Base/String.hpp>
#include <Aero/Input.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Text/TextTypes.hpp>
#include <cstddef>
#include <Aero/Controls/Base.hpp>

namespace Aero::Documents {
class InlineCollection;
class InlineCollectionView;
class TextPointer;
}
namespace Aero::Detail { class DocumentTextAccess; }

namespace Aero::Controls {

namespace Detail {
class TextLayoutService;
class TextServicesAccess;
class PathServicesAccess;
}

using namespace Aero::Core;
using namespace Aero;
using namespace Aero::Data;
using namespace Aero::Input;
using namespace Aero::Media;
using namespace Aero::Render;

enum class Orientation : std::uint8_t { Horizontal = 0U, Vertical };
enum class Dock : std::uint8_t { Left = 0U, Top, Right, Bottom };
enum class PenLineJoin : std::uint8_t { Miter = 0U, Bevel, Round };
enum class PenLineCap : std::uint8_t { Flat = 0U, Square, Round, Triangle };

} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Text::TextWrapping> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("TextWrapping"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "TextWrapping"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<Text::TextTrimming> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("TextTrimming"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "TextTrimming"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<Text::TextAlignment> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("TextAlignment"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "TextAlignment"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<Text::FontStyle> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("FontStyle"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "FontStyle"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<Controls::Orientation> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("Orientation"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "Orientation"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<Controls::Dock> {
    static constexpr TypeId Id() noexcept { return MakeTypeId("Dock"); }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept { return "Dock"; }
    static constexpr TypeId BaseType() noexcept { return InvalidTypeId; }
};

template<>
struct MetaTypeTraits<Controls::PenLineJoin> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("PenLineJoin");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "PenLineJoin";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Controls::PenLineCap> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("PenLineCap");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "PenLineCap";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core

namespace Aero::Controls {

using namespace Aero::Core;
using namespace Aero;
using namespace Aero::Data;
using namespace Aero::Input;
using namespace Aero::Media;
using namespace Aero::Render;

class AERO_API StackPanel final : public Panel {
    AERO_DECLARE_TYPE(StackPanel, Panel)
public:
    StackPanel() noexcept;
    explicit StackPanel(Orientation orientation) noexcept;
    Orientation GetOrientation() const noexcept;
    Base::Result<void> SetOrientation(Orientation value) noexcept;
    inline static constexpr Members::Property<Orientation> OrientationProperty{"Orientation"};
protected:
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;
};

class AERO_API DockPanel final : public Panel {
    AERO_DECLARE_TYPE(DockPanel, Panel)
public:
    DockPanel() noexcept : Panel(StaticTypeId()) {}
    bool LastChildFill() const noexcept;
    Base::Result<void> SetLastChildFill(bool value) noexcept;
    Base::Result<void> SetChildDock(
        UIElement& child, Dock value) noexcept;
    Dock ChildDock(const UIElement& child) const noexcept;
    inline static constexpr Members::Property<bool> LastChildFillProperty{"LastChildFill"};
    inline static constexpr Members::AttachedProperty<Dock> DockProperty{"Dock"};
protected:
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;
};

class AERO_API WrapPanel final : public Panel {
    AERO_DECLARE_TYPE(WrapPanel, Panel)
public:
    WrapPanel() noexcept : Panel(StaticTypeId()) {}
    Orientation GetOrientation() const noexcept;
    Base::Result<void> SetOrientation(Orientation value) noexcept;
    double ItemWidth() const noexcept;
    double ItemHeight() const noexcept;
    Base::Result<void> SetItemWidth(double value) noexcept;
    Base::Result<void> SetItemHeight(double value) noexcept;
    inline static constexpr Members::Property<Orientation> OrientationProperty{"Orientation"};
    // Zero selects the child's desired dimension.
    inline static constexpr Members::Property<double> ItemWidthProperty{"ItemWidth"};
    inline static constexpr Members::Property<double> ItemHeightProperty{"ItemHeight"};
protected:
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;
};

class AERO_API UniformGrid final : public Panel {
    AERO_DECLARE_TYPE(UniformGrid, Panel)
public:
    UniformGrid() noexcept : Panel(StaticTypeId()) {}
    std::uint32_t Rows() const noexcept;
    std::uint32_t Columns() const noexcept;
    std::uint32_t FirstColumn() const noexcept;
    Base::Result<void> SetRows(std::uint32_t value) noexcept;
    Base::Result<void> SetColumns(std::uint32_t value) noexcept;
    Base::Result<void> SetFirstColumn(
        std::uint32_t value) noexcept;
    inline static constexpr Members::Property<std::uint32_t> RowsProperty{"Rows"};
    inline static constexpr Members::Property<std::uint32_t> ColumnsProperty{"Columns"};
    inline static constexpr Members::Property<std::uint32_t> FirstColumnProperty{"FirstColumn"};
protected:
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;
private:
    void ResolveDimensions(
        std::uint32_t childCount,
        std::uint32_t& rows,
        std::uint32_t& columns) const noexcept;
};

class AERO_API Canvas final : public Panel {
    AERO_DECLARE_TYPE(Canvas, Panel)
public:
    Canvas() noexcept;
    Base::Result<void> SetChildPosition(UIElement& child, Point position) noexcept;
    Point ChildPosition(const UIElement& child) const noexcept;
    inline static constexpr Members::AttachedProperty<double> LeftProperty{"Left"};
    inline static constexpr Members::AttachedProperty<double> TopProperty{"Top"};
    inline static constexpr Members::AttachedProperty<double> RightProperty{"Right"};
    inline static constexpr Members::AttachedProperty<double> BottomProperty{"Bottom"};
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

class AERO_API ColumnDefinition final : public Base::Object {
    AERO_DECLARE_TYPE(ColumnDefinition, Base::Object)
public:
    ColumnDefinition() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    GridLength Width() const noexcept { return width_; }
    double MaxWidth() const noexcept { return maxWidth_; }
    Base::StringView SharedSizeGroup() const noexcept {
        return sharedSizeGroup_.View();
    }
    Base::Result<void> SetWidth(GridLength value) noexcept;
    Base::Result<void> SetMaxWidth(double value) noexcept;
    Base::Result<void> SetSharedSizeGroup(
        Base::StringView value) noexcept;
private:
    GridLength width_ = GridLength::Star();
    double maxWidth_ = 1.0e12;
    Base::String sharedSizeGroup_;
};

class AERO_API RowDefinition final : public Base::Object {
    AERO_DECLARE_TYPE(RowDefinition, Base::Object)
public:
    RowDefinition() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    GridLength Height() const noexcept { return height_; }
    double MaxHeight() const noexcept { return maxHeight_; }
    Base::StringView SharedSizeGroup() const noexcept {
        return sharedSizeGroup_.View();
    }
    Base::Result<void> SetHeight(GridLength value) noexcept;
    Base::Result<void> SetMaxHeight(double value) noexcept;
    Base::Result<void> SetSharedSizeGroup(
        Base::StringView value) noexcept;
private:
    GridLength height_ = GridLength::Star();
    double maxHeight_ = 1.0e12;
    Base::String sharedSizeGroup_;
};

class AERO_API Grid final : public Panel {
    AERO_DECLARE_TYPE(Grid, Panel)
public:
    Grid() noexcept;
    Base::Result<void> SetColumnDefinitions(Base::Span<const GridLength> definitions) noexcept;
    Base::Result<void> SetRowDefinitions(Base::Span<const GridLength> definitions) noexcept;
    Base::Result<void> SetChildCell(UIElement& child, std::uint32_t row, std::uint32_t column) noexcept;
    Base::Result<void> SetChildCell(
        UIElement& child,
        std::uint32_t row,
        std::uint32_t column,
        std::uint32_t rowSpan,
        std::uint32_t columnSpan) noexcept;
    Base::Result<void> AddColumnDefinition(
        Base::Ref<ColumnDefinition> definition) noexcept;
    Base::Result<void> AddRowDefinition(
        Base::Ref<RowDefinition> definition) noexcept;
    Base::Result<void> ClearColumnDefinitionObjects() noexcept;
    Base::Result<void> ClearRowDefinitionObjects() noexcept;
    Base::Result<void> AddInputBinding(
        Base::Ref<Aero::Input::KeyBinding> binding) noexcept;
    void ClearInputBindings() noexcept { inputBindings_.Clear(); }
    Base::Span<const Base::Ref<Aero::Input::KeyBinding>>
    InputBindings() const noexcept {
        return {inputBindings_.Data(), inputBindings_.Size()};
    }
    Base::StringView ColumnDefinitionsText() const noexcept;
    Base::StringView RowDefinitionsText() const noexcept;
    Base::Result<void> SetColumnDefinitionsText(
        Base::StringView value) noexcept;
    Base::Result<void> SetRowDefinitionsText(
        Base::StringView value) noexcept;
    Base::Span<const GridLength> ColumnDefinitions() const noexcept { return {columns_.Data(), columns_.Size()}; }
    Base::Span<const GridLength> RowDefinitions() const noexcept { return {rows_.Data(), rows_.Size()}; }
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
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;
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
    std::uint32_t ColumnCount() const noexcept;
    std::uint32_t RowCount() const noexcept;
    GridLength ColumnAt(std::uint32_t index) const noexcept;
    GridLength RowAt(std::uint32_t index) const noexcept;
    Base::Result<void> ValidateDefinitions(Base::Span<const GridLength> definitions) const noexcept;
    std::uint32_t ChildRow(const UIElement& child) const noexcept;
    std::uint32_t ChildColumn(const UIElement& child) const noexcept;
    std::uint32_t ChildRowSpan(const UIElement& child) const noexcept;
    std::uint32_t ChildColumnSpan(const UIElement& child) const noexcept;
    Base::Result<void> ResolveTracks(Base::Span<const GridLength> definitions,
        Base::Span<const double> desired, double available,
        Base::Vector<double>& resolved) const noexcept;
};

// WPF-shaped single-child scaling decorator. The child participates in
// layout at its natural size and is then fitted into the Viewbox slot.
class AERO_API Viewbox final : public Decorator {
    AERO_DECLARE_TYPE(Viewbox, Decorator)
public:
    Viewbox() noexcept : Decorator(StaticTypeId()) {}

    Stretch GetStretch() const noexcept;
    StretchDirection GetStretchDirection() const noexcept;
    Base::Result<void> SetStretch(Stretch value) noexcept;
    Base::Result<void> SetStretchDirection(
        StretchDirection value) noexcept;

    inline static constexpr Members::Property<Stretch> StretchProperty{"Stretch"};
    inline static constexpr Members::Property<StretchDirection> StretchDirectionProperty{"StretchDirection"};

protected:
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
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
    Base::Result<void> SetBackground(Color value) noexcept;
    Base::Result<void> SetBackgroundBrush(
        Base::Ref<Brush> value) noexcept;
    Base::Result<void> SetStroke(Color value, double thickness) noexcept;
    Base::Result<void> SetBorderBrush(Color value) noexcept;
    Base::Result<void> SetBorderBrushObject(
        Base::Ref<Brush> value) noexcept;
    Base::Result<void> SetBorderThickness(
        Thickness value) noexcept;
    Base::Result<void> SetBorderThickness(
        double value) noexcept;
    Base::Result<void> SetCornerRadius(
        CornerRadius value) noexcept;
    Base::Result<void> SetCornerRadius(
        double value) noexcept;
    Base::Result<void> SetPadding(Thickness value) noexcept;
    Color Background() const noexcept;
    Base::Ref<Brush> BackgroundBrush() const noexcept;
    Color BorderBrush() const noexcept;
    Base::Ref<Brush> BorderBrushObject() const noexcept;
    Thickness BorderThickness() const noexcept;
    CornerRadius GetCornerRadius() const noexcept;
    Thickness Padding() const noexcept;
    inline static constexpr Members::Property<Base::Ref<Aero::Media::Brush>> BackgroundProperty{"Background"};
    inline static constexpr Members::Property<Base::Ref<Aero::Media::Brush>> BorderBrushProperty{"BorderBrush"};
    inline static constexpr Members::Property<Aero::Base::Thickness> BorderThicknessProperty{"BorderThickness"};
    inline static constexpr Members::Property<Aero::Base::CornerRadius> CornerRadiusProperty{"CornerRadius"};
    inline static constexpr Members::Property<Aero::Base::Thickness> PaddingProperty{"Padding"};
protected:
    explicit Border(TypeId runtimeType) noexcept : Decorator(runtimeType) {}
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;
    Base::Result<void> OnRender(DrawingContext& context) noexcept override;
};

class AERO_API TextBlock : public FrameworkElement {
    AERO_DECLARE_TYPE(TextBlock, FrameworkElement)
public:
    TextBlock() noexcept;
    ~TextBlock() override;
    Base::StringView Text() const noexcept;
    Color Foreground() const noexcept;
    Base::Ref<Brush> ForegroundBrush() const noexcept;
    Color Background() const noexcept;
    Base::Ref<Brush> BackgroundBrush() const noexcept;
    double FontSize() const noexcept;
    Base::StringView FontFamily() const noexcept;
    FontWeight GetFontWeight() const noexcept;
    Text::FontStyle GetFontStyle() const noexcept;
    TextDecorations GetTextDecorations() const noexcept;
    Text::TextWrapping TextWrapping() const noexcept;
    Text::TextTrimming TextTrimming() const noexcept;
    Text::TextAlignment TextAlignment() const noexcept;
    std::uint32_t InlineCount() const noexcept {
        return ownedInlines_.Size();
    }
    Documents::InlineCollection Inlines() noexcept;
    Documents::InlineCollectionView Inlines() const noexcept;
    Documents::InlineCollection GetInlines() noexcept;
    Documents::InlineCollectionView GetInlines() const noexcept;
    Documents::TextPointer ContentStart() noexcept;
    Documents::TextPointer ContentEnd() noexcept;
    Core::Value MetadataInlines() const noexcept;
    Base::Result<void> SetText(Base::StringView value) noexcept;
    Base::Result<void> SetForeground(Color value) noexcept;
    Base::Result<void> SetForegroundBrush(
        Base::Ref<Brush> value) noexcept;
    Base::Result<void> SetBackground(Color value) noexcept;
    Base::Result<void> SetBackgroundBrush(
        Base::Ref<Brush> value) noexcept;
    Base::Result<void> SetFontSize(double value) noexcept;
    Base::Result<void> SetFontFamily(
        Base::StringView value) noexcept;
    Base::Result<void> SetFontWeight(
        FontWeight value) noexcept;
    Base::Result<void> SetFontStyle(
        Text::FontStyle value) noexcept;
    Base::Result<void> SetTextDecorations(
        TextDecorations value) noexcept;
    Base::Result<void> SetTextWrapping(
        Text::TextWrapping value) noexcept;
    Base::Result<void> SetTextTrimming(
        Text::TextTrimming value) noexcept;
    Base::Result<void> SetTextAlignment(
        Text::TextAlignment value) noexcept;
    Base::Result<void> SetInlineValue(
        Core::Value value) noexcept;
    Base::Result<void> AddOwnedInline(
        const Base::Ref<Base::Object>& inlineObject) noexcept;
    Base::Result<void> ClearOwnedInlines() noexcept;
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
    inline static constexpr Members::Property<Text::FontStyle> FontStyleProperty{"FontStyle"};
    inline static constexpr Members::Property<TextDecorations> TextDecorationsProperty{"TextDecorations"};
    inline static constexpr Members::Property<double> StrokeThicknessProperty{"StrokeThickness"};
    inline static constexpr Members::Property<Text::TextWrapping> TextWrappingProperty{"TextWrapping"};
    inline static constexpr Members::Property<Text::TextTrimming> TextTrimmingProperty{"TextTrimming"};
    inline static constexpr Members::Property<Text::TextAlignment> TextAlignmentProperty{"TextAlignment"};
    inline static constexpr Members::Property<Thickness> PaddingProperty{"Padding"};
protected:
    explicit TextBlock(TypeId runtimeType) noexcept;
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;
    Base::Result<void> OnRender(DrawingContext& context) noexcept override;
private:
    friend class Detail::TextServicesAccess;
    friend class Aero::Detail::DocumentTextAccess;

    Base::StringView EffectiveFontFamily() const noexcept;
    void ReleaseServiceGlyphRun() noexcept;
    Base::Result<void> SetGlyphRun(
        std::uint64_t glyphRun, Size size) noexcept;

    Detail::TextLayoutService* layoutService_ = nullptr;
    Base::Vector<std::uint64_t> glyphRuns_;
    Base::Vector<Text::TextHitRegion> textHitRegions_;
    Base::Vector<Base::Ref<Base::Object>> ownedInlines_;
    Base::Ref<Base::Object> pendingInline_;
    Size glyphRunSize_;
    bool serviceOwnsGlyphRun_ = false;
};

// WPF-shaped vector path. The first implementation intentionally accepts the
// deterministic SVG/WPF subset used by the Gallery vector brand (M/m, L/l,
// H/h, V/v, C/c, and Z/z). Unsupported commands fail validation instead of
// silently producing different geometry. Multiple contours use even-odd fill.
class AERO_API Path final : public FrameworkElement {
    AERO_DECLARE_TYPE(Path, FrameworkElement)
public:
    Path() noexcept;
    ~Path() override;

    Base::StringView Data() const noexcept;
    Base::Ref<Geometry> DataGeometry() const noexcept;
    Color Fill() const noexcept;
    Base::Ref<Brush> FillBrush() const noexcept;
    Color Stroke() const noexcept;
    Base::Ref<Brush> StrokeBrush() const noexcept;
    double StrokeThickness() const noexcept;
    PenLineJoin StrokeLineJoin() const noexcept;
    PenLineCap StrokeStartLineCap() const noexcept;
    PenLineCap StrokeEndLineCap() const noexcept;
    double TrimStart() const noexcept;
    double TrimEnd() const noexcept;
    Stretch GetStretch() const noexcept;
    Rect GeometryBounds() const noexcept { return geometryBounds_; }

    Base::Result<void> SetData(Base::StringView value) noexcept;
    Base::Result<void> SetData(
        Base::Ref<Geometry> value) noexcept;
    Base::Result<void> SetFill(Color value) noexcept;
    Base::Result<void> SetFillBrush(
        Base::Ref<Brush> value) noexcept;
    Base::Result<void> SetStroke(Color value) noexcept;
    Base::Result<void> SetStrokeBrush(
        Base::Ref<Brush> value) noexcept;
    Base::Result<void> SetStrokeThickness(double value) noexcept;
    Base::Result<void> SetStrokeLineJoin(
        PenLineJoin value) noexcept;
    Base::Result<void> SetStrokeStartLineCap(
        PenLineCap value) noexcept;
    Base::Result<void> SetStrokeEndLineCap(
        PenLineCap value) noexcept;
    Base::Result<void> SetTrimStart(double value) noexcept;
    Base::Result<void> SetTrimEnd(double value) noexcept;
    Base::Result<void> SetStretch(Stretch value) noexcept;

    inline static constexpr Members::Property<Base::Ref<Geometry>> DataProperty{"Data"};
    inline static constexpr Members::Property<Base::Ref<Brush>> FillProperty{"Fill"};
    inline static constexpr Members::Property<Base::Ref<Brush>> StrokeProperty{"Stroke"};
    inline static constexpr Members::Property<double> StrokeThicknessProperty{"StrokeThickness"};
    inline static constexpr Members::Property<PenLineJoin> StrokeLineJoinProperty{"StrokeLineJoin"};
    inline static constexpr Members::Property<PenLineCap> StrokeStartLineCapProperty{"StrokeStartLineCap"};
    inline static constexpr Members::Property<PenLineCap> StrokeEndLineCapProperty{"StrokeEndLineCap"};
    inline static constexpr Members::AttachedProperty<double> TrimStartProperty{"TrimStart"};
    inline static constexpr Members::AttachedProperty<double> TrimEndProperty{"TrimEnd"};
    inline static constexpr Members::Property<Stretch> StretchProperty{"Stretch"};

protected:
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<void> OnRender(
        DrawingContext& context) noexcept override;

private:
    friend class Detail::PathServicesAccess;

    Base::Result<void> EnsureGeometry() noexcept;
    Base::Result<void> EnsureMesh() noexcept;
    void ResetGeometry() noexcept;
    void AttachMeshServices(
        void* services,
        bool force = false) noexcept;
    void ReleaseMesh() noexcept;

    Base::Vector<Point> geometryVertices_;
    Base::Vector<std::uint32_t> geometryIndices_;
    Base::Vector<Point> pathPoints_;
    Base::Vector<std::uint32_t> pathContourStarts_;
    Base::Vector<std::uint32_t> pathContourCounts_;
    Base::Vector<std::uint8_t> pathContourClosed_;
    Base::Vector<Point> strokeVertices_;
    Base::Vector<std::uint32_t> strokeIndices_;
    Rect geometryBounds_;
    void* meshServices_ = nullptr;
    std::uint64_t meshServiceGeneration_ = 0U;
    std::uint64_t mesh_ = 0U;
    std::uint64_t strokeMesh_ = 0U;
    bool geometryDirty_ = true;
};

class AERO_API ContentPresenter final : public FrameworkElement {
    AERO_DECLARE_TYPE(ContentPresenter, FrameworkElement)
public:
    ContentPresenter() noexcept;
    UIElement* Content() const noexcept { return content_; }
    const Base::Ref<Base::Object>& OwnedContent() const noexcept { return ownedContent_; }
    const Core::Value& ContentValue() const noexcept {
        return contentValue_;
    }
    Base::StringView ContentSource() const noexcept {
        return GetValueOr(
            ContentSourceProperty,
            Base::StringView{});
    }
    Base::Result<void> SetContentSource(
        Base::StringView value) noexcept;
    Base::Result<void> SetContentValue(
        Core::Value value) noexcept {
        return SetValue(
            ContentProperty, std::move(value));
    }
    Base::Result<void> SetContent(UIElement* content) noexcept;

    // Template teardown is a two-step transaction: clear the presenter-owned
    // reference first, then detach the visual/layout/render edge through the GUI context. A nullptr literal selects this overload without weakening
    // the ordinary UIElement* validation path.
    Base::Result<void> SetContent(std::nullptr_t) noexcept {
        Base::Result<void> access = VerifyAccess();
        if (!access) return access.GetStatus();
        if (content_ == nullptr) return {};
        if (!LayoutChildren().Empty() && !IsOnlyAttachedContent(*content_)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "ContentPresenter content must be its only attached layout child");
        }
        content_ = nullptr;
        ownedContent_.Reset();
        return InvalidateMeasure();
    }

    Base::Result<void> SetOwnedContent(const Base::Ref<Base::Object>& contentObject,
        UIElement& content) noexcept;

    inline static constexpr Members::Property<Base::String> ContentSourceProperty{"ContentSource"};
    inline static constexpr Members::Property<Core::Value> ContentProperty{"Content"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ContentTemplateProperty{"ContentTemplate"};
    static void OnContentPropertyChanged(
        Core::DependencyObject& object,
        const Core::DependencyPropertyChangedEventArgs&
            change) noexcept;
protected:
    Base::Result<Size> MeasureOverride(Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(Size finalSize) noexcept override;
private:
    UIElement* content_ = nullptr;
    Base::Ref<Base::Object> ownedContent_;
    Core::Value contentValue_ =
        Core::Value::NullObject(
            Core::TypeOf<Base::Object>());
    bool IsOnlyAttachedContent(const UIElement& content) const noexcept;
    Base::Result<void> ValidateContent(UIElement* content) const noexcept;
    Base::Result<void> UpdatePresentedText() noexcept;
};

} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Controls::GridLength> {
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

} // namespace Aero::Core
