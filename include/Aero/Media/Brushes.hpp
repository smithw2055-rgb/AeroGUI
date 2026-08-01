#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Collections.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Transforms.hpp>

namespace Aero::Detail {
class ImageBrushAccess;
}

namespace Aero::Media {

using namespace Aero::Core;
using Color = Base::Color;
class GradientBrush;

enum class TileMode : std::uint8_t {
    None = 0U,
    Tile,
    FlipX,
    FlipY,
    FlipXY
};

// WPF BrushMappingMode. Gradient coordinates are relative to the painted
// bounds by default and become device-independent units in Absolute mode.
enum class BrushMappingMode : std::uint8_t {
    RelativeToBoundingBox = 0U,
    Absolute
};

class AERO_API Brush : public DependencyObject {
    AERO_DECLARE_TYPE(Brush, DependencyObject)
public:
    double Opacity() const noexcept;
    FrameworkElement* Owner() const noexcept {
        return owner_;
    }
    void SetOwner(FrameworkElement* owner) noexcept {
        owner_ = owner;
    }
    Base::Result<void> SetOpacity(double value) noexcept;
    Base::Ref<Base::Object> Shader() const noexcept {
        return GetValueOr(
            ShaderProperty, Base::Ref<Base::Object>{});
    }
    Base::Result<void> SetShader(
        Base::Ref<Base::Object> value) noexcept {
        return SetValue(ShaderProperty, std::move(value));
    }
    Base::Ref<Transform> RelativeTransform() const noexcept {
        return GetValueOr(
            RelativeTransformProperty,
            Base::Ref<Transform>{});
    }
    Base::Result<void> SetRelativeTransform(
        Base::Ref<Transform> value) noexcept {
        return SetValue(
            RelativeTransformProperty,
            std::move(value));
    }

    inline static constexpr Members::Property<double> OpacityProperty{"Opacity"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ShaderProperty{"Shader"};
    inline static constexpr Members::Property<Base::Ref<Transform>> RelativeTransformProperty{"RelativeTransform"};

protected:
    explicit Brush(TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}
    ~Brush() override = default;
    Base::Result<void> OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;

private:
    FrameworkElement* owner_ = nullptr;
};

// Common owner for the WPF-style inheritable Foreground property.
inline constexpr DependencyPropertyRef<
    FrameworkElement,
    Base::Ref<Brush>>
    FrameworkElementForegroundProperty{"Foreground"};

class AERO_API SolidColorBrush final : public Brush {
    AERO_DECLARE_TYPE(SolidColorBrush, Brush)
public:
    SolidColorBrush() noexcept
        : Brush(StaticTypeId()) {}
    explicit SolidColorBrush(Color color) noexcept
        : Brush(StaticTypeId()), initialColor_(color) {}
    ~SolidColorBrush() override = default;

    Color GetColor() const noexcept;
    Base::Result<void> SetColor(Color value) noexcept;

    inline static constexpr Members::Property<Color> ColorProperty{"Color"};

private:
    Color initialColor_{};
};

class AERO_API GradientStop final : public DependencyObject {
    AERO_DECLARE_TYPE(GradientStop, DependencyObject)
public:
    GradientStop() noexcept
        : DependencyObject(StaticTypeId()) {}
    ~GradientStop() override = default;

    double Offset() const noexcept;
    Color GetColor() const noexcept;
    Base::Result<void> SetOffset(double value) noexcept;
    Base::Result<void> SetColor(Color value) noexcept;

    inline static constexpr Members::Property<double> OffsetProperty{"Offset"};
    inline static constexpr Members::Property<Color> ColorProperty{"Color"};

    void SetOwner(GradientBrush* owner) noexcept {
        owner_ = owner;
    }

protected:
    Base::Result<void> OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;

private:
    GradientBrush* owner_ = nullptr;
};

// Standalone WPF collection resource. GradientBrush keeps its own optimized
// stops, while this collection is also consumable as an authored resource
// (for example, as an ItemsSource in the Gallery samples).
class AERO_API GradientStopCollection final :
    public Base::Object,
    public Collections::IItemsSource {
    AERO_DECLARE_TYPE(GradientStopCollection, Base::Object)
public:
    GradientStopCollection() noexcept
        : stops_(&Base::GetDefaultAllocator()) {}
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Span<const Base::Ref<GradientStop>>
    Items() const noexcept {
        return stops_.AsSpan();
    }
    std::uint32_t Count() const noexcept override {
        return stops_.Size();
    }
    Base::Ref<Base::Object> ItemAt(
        std::uint32_t index) const noexcept override {
        return index < stops_.Size()
            ? Base::Ref<Base::Object>(stops_[index])
            : Base::Ref<Base::Object>{};
    }
    Base::Result<void> TryAddItemsChanged(
        const Collections::ItemsChangedHandler& handler) noexcept override {
        return changed_.TryAdd(handler);
    }
    bool RemoveItemsChanged(
        const Collections::ItemsChangedHandler& handler) noexcept override {
        return changed_.Remove(handler);
    }
    Base::Result<void> Add(
        Base::Ref<GradientStop> stop) noexcept;
    void Clear() noexcept {
        const std::uint32_t count = stops_.Size();
        stops_.Clear();
        if (!changed_.Empty()) {
            changed_.Invoke({
                Collections::ItemsChangeAction::Reset,
                0U, 0U, count, 0U});
        }
    }
private:
    Base::Vector<Base::Ref<GradientStop>> stops_;
    Collections::ItemsChangedHandler changed_;
};

class AERO_API MonochromeBrush final : public Base::Object {
    AERO_DECLARE_TYPE(MonochromeBrush, Base::Object)
public:
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Color ColorValue() const noexcept { return color_; }
    Base::Result<void> SetColor(Color value) noexcept {
        color_ = value; return {};
    }
private:
    Color color_{};
};

class AERO_API ConicGradientBrush final : public Base::Object {
    AERO_DECLARE_TYPE(ConicGradientBrush, Base::Object)
public:
    ConicGradientBrush() noexcept
        : stops_(&Base::GetDefaultAllocator()) {}
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Result<void> AddGradientStop(Base::Ref<GradientStop> value) noexcept {
        return value ? stops_.TryPushBack(std::move(value))
            : Base::Result<void>(Base::Status::Failure(
                Base::ErrorCode::InvalidArgument, "ConicGradientBrush stop is null"));
    }
    void ClearGradientStops() noexcept { stops_.Clear(); }
private:
    Base::Vector<Base::Ref<GradientStop>> stops_;
};

class AERO_API WavesBrush final : public Base::Object {
    AERO_DECLARE_TYPE(WavesBrush, Base::Object)
public:
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    double Time() const noexcept { return time_; }
    Base::Result<void> SetTime(double value) noexcept {
        time_ = value; return {};
    }
private:
    double time_ = 0.0;
};

class AERO_API GradientBrush : public Brush {
    AERO_DECLARE_TYPE(GradientBrush, Brush)
public:
    Base::Span<const Base::Ref<GradientStop>>
        GradientStops() const noexcept {
        return stops_.AsSpan();
    }
    Base::Result<void> AddGradientStop(
        Base::Ref<GradientStop> stop) noexcept;
    void ClearGradientStops() noexcept;
    Color Sample(double position) const noexcept;
    BrushMappingMode MappingMode() const noexcept;
    Base::Result<void> SetMappingMode(
        BrushMappingMode value) noexcept;

    inline static constexpr Members::Property<BrushMappingMode> MappingModeProperty{"MappingMode"};

protected:
    explicit GradientBrush(TypeId runtimeType) noexcept
        : Brush(runtimeType),
          stops_(&Base::GetDefaultAllocator()) {}
    ~GradientBrush() override;

private:
    Base::Vector<Base::Ref<GradientStop>> stops_;
};

class AERO_API LinearGradientBrush final
    : public GradientBrush {
    AERO_DECLARE_TYPE(LinearGradientBrush, GradientBrush)
public:
    LinearGradientBrush() noexcept
        : GradientBrush(StaticTypeId()) {}
    ~LinearGradientBrush() override = default;

    Point StartPoint() const noexcept;
    Point EndPoint() const noexcept;
    Base::Result<void> SetStartPoint(Point value) noexcept;
    Base::Result<void> SetEndPoint(Point value) noexcept;

    inline static constexpr Members::Property<Point> StartPointProperty{"StartPoint"};
    inline static constexpr Members::Property<Point> EndPointProperty{"EndPoint"};
};

class AERO_API RadialGradientBrush final
    : public GradientBrush {
    AERO_DECLARE_TYPE(RadialGradientBrush, GradientBrush)
public:
    RadialGradientBrush() noexcept
        : GradientBrush(StaticTypeId()) {}
    ~RadialGradientBrush() override = default;

    Point Center() const noexcept;
    Point GradientOrigin() const noexcept;
    double GetRadiusX() const noexcept;
    double GetRadiusY() const noexcept;
    Base::Result<void> SetCenter(Point value) noexcept;
    Base::Result<void> SetGradientOrigin(
        Point value) noexcept;
    Base::Result<void> SetRadiusX(double value) noexcept;
    Base::Result<void> SetRadiusY(double value) noexcept;

    inline static constexpr Members::Property<Point> CenterProperty{"Center"};
    inline static constexpr Members::Property<Point> GradientOriginProperty{"GradientOrigin"};
    inline static constexpr Members::Property<double> RadiusXProperty{"RadiusX"};
    inline static constexpr Members::Property<double> RadiusYProperty{"RadiusY"};
};

class AERO_API ImageBrush final : public Brush {
    AERO_DECLARE_TYPE(ImageBrush, Brush)
public:
    ImageBrush() noexcept
        : Brush(StaticTypeId()) {}
    ~ImageBrush() override = default;

    Base::Ref<ImageSource> Source() const noexcept;
    Stretch GetStretch() const noexcept;
    Rect Viewbox() const noexcept;
    Rect Viewport() const noexcept;
    TileMode GetTileMode() const noexcept;
    Base::Result<void> SetSource(
        Base::Ref<ImageSource> value) noexcept;
    Base::Result<void> SetStretch(
        Stretch value) noexcept;
    Base::Result<void> SetViewbox(
        Rect value) noexcept;
    Base::Result<void> SetViewport(
        Rect value) noexcept;
    Base::Result<void> SetTileMode(
        TileMode value) noexcept;

    inline static constexpr Members::Property<Base::Ref<ImageSource>> ImageSourceProperty{"ImageSource"};
    inline static constexpr Members::Property<Stretch> StretchProperty{"Stretch"};
    inline static constexpr Members::Property<Rect> ViewboxProperty{"Viewbox"};
    inline static constexpr Members::Property<Rect> ViewportProperty{"Viewport"};
    inline static constexpr Members::Property<TileMode> TileModeProperty{"TileMode"};

private:
    friend class Aero::Detail::ImageBrushAccess;
    std::uint64_t renderImage_ = 0U;
    std::uint32_t pixelWidth_ = 0U;
    std::uint32_t pixelHeight_ = 0U;
};

// A visual-backed brush keeps the referenced visual as a dependency property.
// Rendering it through an offscreen visual target is handled by the renderer;
// the UI object owns the WPF resource and binding semantics.
class AERO_API VisualBrush final : public Brush {
    AERO_DECLARE_TYPE(VisualBrush, Brush)
public:
    VisualBrush() noexcept
        : Brush(StaticTypeId()) {}
    ~VisualBrush() override = default;

    Base::Ref<Base::Object> Visual() const noexcept {
        return GetValueOr(
            VisualProperty, Base::Ref<Base::Object>{});
    }
    Base::Result<void> SetVisual(
        Base::Ref<Base::Object> value) noexcept {
        return SetValue(VisualProperty, std::move(value));
    }
    Stretch GetStretch() const noexcept {
        return GetValueOr(StretchProperty, Stretch::Fill);
    }
    Base::Result<void> SetStretch(Stretch value) noexcept {
        return SetValue(StretchProperty, value);
    }
    Rect Viewbox() const noexcept {
        return GetValueOr(
            ViewboxProperty, Rect{0.0, 0.0, 1.0, 1.0});
    }
    Base::Result<void> SetViewbox(Rect value) noexcept {
        return SetValue(ViewboxProperty, value);
    }
    VerticalAlignment AlignmentY() const noexcept {
        return GetValueOr(
            AlignmentYProperty, VerticalAlignment::Center);
    }
    Base::Result<void> SetAlignmentY(
        VerticalAlignment value) noexcept {
        return SetValue(AlignmentYProperty, value);
    }

    inline static constexpr Members::Property<Base::Ref<Base::Object>> VisualProperty{"Visual"};
    inline static constexpr Members::Property<Stretch> StretchProperty{"Stretch"};
    inline static constexpr Members::Property<Rect> ViewboxProperty{"Viewbox"};
    inline static constexpr Members::Property<VerticalAlignment> AlignmentYProperty{"AlignmentY"};
};

AERO_API Color SampleBrush(
    const Base::Ref<Brush>& brush,
    double position = 0.5,
    Color fallback = {0.0F, 0.0F, 0.0F, 0.0F}) noexcept;

AERO_API Base::Result<Base::Ref<Brush>>
MakeSolidColorBrush(Color color) noexcept;

} // namespace Aero::Media

namespace Aero::Core {

template<>
struct MetaTypeTraits<Aero::Media::TileMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("TileMode");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "TileMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Aero::Media::BrushMappingMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("BrushMappingMode");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "BrushMappingMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Base::Rect> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("Rect");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "Rect";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core
