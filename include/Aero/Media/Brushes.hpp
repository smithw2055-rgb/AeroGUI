#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Collections.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Transforms.hpp>

namespace Aero::Internal {
class BrushPrivate;
}

namespace Aero::Media {

using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::DependencyPropertyRef;
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::PropertyInvalidationFlags;
using ::Aero::Meta::TypeId;

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
    double GetOpacity() const noexcept;
    void SetOpacity(double value) noexcept;
    Base::Ref<Base::Object> GetShader() const noexcept {
        return GetValueOr(
            ShaderProperty, Base::Ref<Base::Object>{});
    }
    void SetShader(
        Base::Ref<Base::Object> value) noexcept {
        SetValue(ShaderProperty, std::move(value));
    }
    Base::Ref<Transform> GetRelativeTransform() const noexcept {
        return GetValueOr(
            RelativeTransformProperty,
            Base::Ref<Transform>{});
    }
    void SetRelativeTransform(
        Base::Ref<Transform> value) noexcept {
        SetValue(RelativeTransformProperty, std::move(value));
    }

    inline static constexpr Members::Property<double> OpacityProperty{"Opacity"};
    inline static constexpr Members::Property<Base::Ref<Base::Object>> ShaderProperty{"Shader"};
    inline static constexpr Members::Property<Base::Ref<Transform>> RelativeTransformProperty{"RelativeTransform"};

protected:
    explicit Brush(TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}
    ~Brush() override = default;
    void OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;

private:
    friend class ::Aero::Internal::BrushPrivate;
    FrameworkElement* owner_ = nullptr;

    FrameworkElement* GetOwner() const noexcept {
        return owner_;
    }
    void SetOwner(FrameworkElement* owner) noexcept {
        owner_ = owner;
    }
};

// Common owner for the WPF-style inheritable Foreground property.
inline constexpr DependencyPropertyRef<
    FrameworkElement,
    Base::Ref<Brush>>
    FrameworkElementForegroundProperty{"Foreground"};

class AERO_API SolidColorBrush : public Brush {
    AERO_DECLARE_TYPE(SolidColorBrush, Brush)
public:
    SolidColorBrush() noexcept
        : Brush(StaticTypeId()) {}
    explicit SolidColorBrush(Color color) noexcept
        : Brush(StaticTypeId()), initialColor_(color) {}
    ~SolidColorBrush() override = default;

    Color GetColor() const noexcept;
    void SetColor(Color value) noexcept;

    inline static constexpr Members::Property<Color> ColorProperty{"Color"};

private:
    Color initialColor_{};
};

class AERO_API GradientStop : public DependencyObject {
    AERO_DECLARE_TYPE(GradientStop, DependencyObject)
public:
    GradientStop() noexcept
        : DependencyObject(StaticTypeId()) {}
    ~GradientStop() override = default;

    double GetOffset() const noexcept;
    Color GetColor() const noexcept;
    void SetOffset(double value) noexcept;
    void SetColor(Color value) noexcept;

    inline static constexpr Members::Property<double> OffsetProperty{"Offset"};
    inline static constexpr Members::Property<Color> ColorProperty{"Color"};

protected:
    void OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept override;

private:
    friend class GradientBrush;
    void SetOwner(GradientBrush* owner) noexcept {
        owner_ = owner;
    }
    GradientBrush* owner_ = nullptr;
};

// Standalone WPF collection resource. GradientBrush keeps its own optimized
// stops, while this collection is also consumable as an authored resource
// (for example, as an ItemsSource in the Gallery samples).
class AERO_API GradientStopCollection :
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
    GetItems() const noexcept {
        return stops_.AsSpan();
    }
    std::uint32_t GetCount() const noexcept override {
        return stops_.Size();
    }
    Base::Ref<Base::Object> GetItem(
        std::uint32_t index) const noexcept override {
        return index < stops_.Size()
            ? Base::Ref<Base::Object>(stops_[index])
            : Base::Ref<Base::Object>{};
    }
    void AddItemsChanged(
        const Collections::ItemsChangedHandler& handler) noexcept override {
        changed_.Add(handler);
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

class AERO_API BrushShader : public Base::Object {
    AERO_DECLARE_TYPE(BrushShader, Base::Object)
public:
    BrushShader() noexcept = default;
    ~BrushShader() override = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
};

class AERO_API MonochromeShader : public BrushShader {
    AERO_DECLARE_TYPE(MonochromeShader, BrushShader)
public:
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Color GetColor() const noexcept { return color_; }
    void SetColor(Color value) noexcept {
        color_ = value; return;
    }
private:
    Color color_{};
};

class AERO_API ConicGradientShader : public BrushShader {
    AERO_DECLARE_TYPE(ConicGradientShader, BrushShader)
public:
    ConicGradientShader() noexcept
        : stops_(&Base::GetDefaultAllocator()) {}
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Base::Result<void> AddGradientStop(Base::Ref<GradientStop> value) noexcept {
        return value ? stops_.PushBack(std::move(value))
            : Base::Result<void>(Base::Status::Failure(
                Base::ErrorCode::InvalidArgument, "ConicGradientShader stop is null"));
    }
    void ClearGradientStops() noexcept { stops_.Clear(); }
private:
    Base::Vector<Base::Ref<GradientStop>> stops_;
};

class AERO_API WavesShader : public BrushShader {
    AERO_DECLARE_TYPE(WavesShader, BrushShader)
public:
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    double GetTime() const noexcept { return time_; }
    void SetTime(double value) noexcept {
        time_ = value; return;
    }
private:
    double time_ = 0.0;
};

class AERO_API GradientBrush : public Brush {
    AERO_DECLARE_TYPE(GradientBrush, Brush)
public:
    Base::Span<const Base::Ref<GradientStop>>
        GetGradientStops() const noexcept {
        return stops_.AsSpan();
    }
    Base::Result<void> AddGradientStop(
        Base::Ref<GradientStop> stop) noexcept;
    void ClearGradientStops() noexcept;
    BrushMappingMode GetMappingMode() const noexcept;
    void SetMappingMode(
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

class AERO_API LinearGradientBrush
    : public GradientBrush {
    AERO_DECLARE_TYPE(LinearGradientBrush, GradientBrush)
public:
    LinearGradientBrush() noexcept
        : GradientBrush(StaticTypeId()) {}
    ~LinearGradientBrush() override = default;

    Point GetStartPoint() const noexcept;
    Point GetEndPoint() const noexcept;
    void SetStartPoint(Point value) noexcept;
    void SetEndPoint(Point value) noexcept;

    inline static constexpr Members::Property<Point> StartPointProperty{"StartPoint"};
    inline static constexpr Members::Property<Point> EndPointProperty{"EndPoint"};
};

class AERO_API RadialGradientBrush
    : public GradientBrush {
    AERO_DECLARE_TYPE(RadialGradientBrush, GradientBrush)
public:
    RadialGradientBrush() noexcept
        : GradientBrush(StaticTypeId()) {}
    ~RadialGradientBrush() override = default;

    Point GetCenter() const noexcept;
    Point GetGradientOrigin() const noexcept;
    double GetRadiusX() const noexcept;
    double GetRadiusY() const noexcept;
    void SetCenter(Point value) noexcept;
    void SetGradientOrigin(
        Point value) noexcept;
    void SetRadiusX(double value) noexcept;
    void SetRadiusY(double value) noexcept;

    inline static constexpr Members::Property<Point> CenterProperty{"Center"};
    inline static constexpr Members::Property<Point> GradientOriginProperty{"GradientOrigin"};
    inline static constexpr Members::Property<double> RadiusXProperty{"RadiusX"};
    inline static constexpr Members::Property<double> RadiusYProperty{"RadiusY"};
};

class AERO_API ImageBrush : public Brush {
    AERO_DECLARE_TYPE(ImageBrush, Brush)
public:
    ImageBrush() noexcept
        : Brush(StaticTypeId()) {}
    ~ImageBrush() override = default;

    Base::Ref<ImageSource> GetSource() const noexcept;
    Stretch GetStretch() const noexcept;
    Rect GetViewbox() const noexcept;
    Rect GetViewport() const noexcept;
    TileMode GetTileMode() const noexcept;
    void SetSource(
        Base::Ref<ImageSource> value) noexcept;
    void SetStretch(
        Stretch value) noexcept;
    void SetViewbox(
        Rect value) noexcept;
    void SetViewport(
        Rect value) noexcept;
    void SetTileMode(
        TileMode value) noexcept;

    inline static constexpr Members::Property<Base::Ref<ImageSource>> ImageSourceProperty{"ImageSource"};
    inline static constexpr Members::Property<Stretch> StretchProperty{"Stretch"};
    inline static constexpr Members::Property<Rect> ViewboxProperty{"Viewbox"};
    inline static constexpr Members::Property<Rect> ViewportProperty{"Viewport"};
    inline static constexpr Members::Property<TileMode> TileModeProperty{"TileMode"};

private:
    friend class Aero::Internal::BrushPrivate;
    std::uint64_t renderImage_ = 0U;
    std::uint32_t pixelWidth_ = 0U;
    std::uint32_t pixelHeight_ = 0U;
};

// A visual-backed brush keeps the referenced visual as a dependency property.
// Rendering it through an offscreen visual target is handled by the renderer;
// the UI object owns the WPF resource and binding semantics.
class AERO_API VisualBrush : public Brush {
    AERO_DECLARE_TYPE(VisualBrush, Brush)
public:
    VisualBrush() noexcept
        : Brush(StaticTypeId()) {}
    ~VisualBrush() override = default;

    Base::Ref<Base::Object> GetVisual() const noexcept {
        return GetValueOr(
            VisualProperty, Base::Ref<Base::Object>{});
    }
    void SetVisual(
        Base::Ref<Base::Object> value) noexcept {
        SetValue(VisualProperty, std::move(value));
    }
    Stretch GetStretch() const noexcept {
        return GetValueOr(StretchProperty, Stretch::Fill);
    }
    void SetStretch(Stretch value) noexcept {
        SetValue(StretchProperty, value);
    }
    Rect GetViewbox() const noexcept {
        return GetValueOr(
            ViewboxProperty, Rect{0.0, 0.0, 1.0, 1.0});
    }
    void SetViewbox(Rect value) noexcept {
        SetValue(ViewboxProperty, value);
    }
    VerticalAlignment GetAlignmentY() const noexcept {
        return GetValueOr(
            AlignmentYProperty, VerticalAlignment::Center);
    }
    void SetAlignmentY(
        VerticalAlignment value) noexcept {
        SetValue(AlignmentYProperty, value);
    }

    inline static constexpr Members::Property<Base::Ref<Base::Object>> VisualProperty{"Visual"};
    inline static constexpr Members::Property<Stretch> StretchProperty{"Stretch"};
    inline static constexpr Members::Property<Rect> ViewboxProperty{"Viewbox"};
    inline static constexpr Members::Property<VerticalAlignment> AlignmentYProperty{"AlignmentY"};
};

AERO_API Base::Result<Base::Ref<Brush>>
MakeSolidColorBrush(Color color) noexcept;

} // namespace Aero::Media

AERO_DECLARE_TYPE_ENUM(Aero::Media::TileMode)

AERO_DECLARE_TYPE_ENUM(Aero::Media::BrushMappingMode)

namespace Aero::Meta {

template<>
struct TypeTraits<Base::Rect> {
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

} // namespace Aero::Meta
