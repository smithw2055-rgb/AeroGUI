#pragma once

// Brush family: Brush plus stops, shaders, and concrete brushes.
#include <Aero/Animatable.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Collections.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/HorizontalAlignment.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/Transform2D.hpp>

#include <cstdint>

namespace Aero::Media {

using ::Aero::Meta::DependencyPropertyHandle;
using ::Aero::Meta::DependencyPropertyRef;
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
using ::Aero::Meta::PropertyInvalidationFlags;
using ::Aero::Meta::TypeId;
using Color = Base::Color;
using Point = Base::Point;
using Rect = Base::Rect;
using ::Aero::HorizontalAlignment;
using ::Aero::VerticalAlignment;

enum class TileMode : std::uint8_t {
    None = 0U,
    Tile,
    FlipX,
    FlipY,
    FlipXY
};

enum class BrushMappingMode : std::uint8_t {
    RelativeToBoundingBox = 0U,
    Absolute
};

enum class GradientSpreadMethod : std::uint8_t {
    Pad = 0U,
    Reflect,
    Repeat
};

class AERO_GUI_API Brush : public Animatable {
    AERO_DECLARE_TYPE(Brush, Animatable)
public:

    double GetOpacity() const noexcept;
    void SetOpacity(double value) noexcept;
    Ref<Base::Object> GetShader() const noexcept {
        return GetValue(ShaderProperty);
    }
    void SetShader(Ref<Base::Object> value) noexcept {
        SetValue(ShaderProperty, std::move(value));
    }
    Ref<Transform> GetRelativeTransform() const noexcept {
        return GetValue(RelativeTransformProperty);
    }
    void SetRelativeTransform(Ref<Transform> value) noexcept {
        SetValue(RelativeTransformProperty, std::move(value));
    }

    std::uint64_t GetRevision() const noexcept;

    AERO_DEPENDENCY_PROPERTY(double, Opacity);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, Shader);
    AERO_DEPENDENCY_PROPERTY(Ref<Transform>, RelativeTransform);

protected:
    explicit Brush(TypeId runtimeType) noexcept
        : Animatable(runtimeType) {}
    ~Brush() override = default;
};

class AERO_GUI_API GradientStop : public Animatable {
    AERO_DECLARE_TYPE(GradientStop, Animatable)
public:
    GradientStop() noexcept
        : Animatable(StaticTypeId()) {}
    ~GradientStop() override = default;

    double GetOffset() const noexcept;
    Color GetColor() const noexcept;
    void SetOffset(double value) noexcept;
    void SetColor(Color value) noexcept;

    AERO_DEPENDENCY_PROPERTY(double, Offset);
    AERO_DEPENDENCY_PROPERTY(Color, Color);

};

class AERO_GUI_API GradientStopCollection :
    public Freezable,
    public Collections::IItemsSource {
    AERO_DECLARE_TYPE(GradientStopCollection, Freezable)
public:
    GradientStopCollection() noexcept
        : Freezable(StaticTypeId()),
          stops_(&Base::GetDefaultAllocator()) {}
    ~GradientStopCollection() override;
    Span<const Ref<GradientStop>>
    GetItems() const noexcept {
        return stops_.AsSpan();
    }
    std::uint32_t GetCount() const noexcept override {
        return stops_.Size();
    }
    Ref<Base::Object> GetItem(
        std::uint32_t index) const noexcept override {
        return index < stops_.Size()
            ? Ref<Base::Object>(stops_[index])
            : Ref<Base::Object>{};
    }
    void AddItemsChanged(
        const Collections::ItemsChangedHandler& handler) noexcept override {
        if (IsFrozen()) return;
        changed_.Add(handler);
    }
    bool RemoveItemsChanged(
        const Collections::ItemsChangedHandler& handler) noexcept override {
        return changed_.Remove(handler);
    }
    void Add(
        Ref<GradientStop> stop) noexcept;
    void Clear() noexcept;
protected:
    bool FreezeCore(bool isChecking) noexcept override;
private:
    void OnStopChanged(Freezable&) noexcept;
    Base::Vector<Ref<GradientStop>> stops_;
    Collections::ItemsChangedHandler changed_;
    FreezableChangedHandler stopChangedHandler_;
};

// Shader parameters are plain fields plus a generation counter. XAML still
// sets Color and Time by name, without a dependency-property store.
class AERO_GUI_API BrushShader : public Base::Object {
    AERO_DECLARE_TYPE(BrushShader, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return runtimeType_; }
    std::uint64_t GetRevision() const noexcept { return revision_; }
    BrushShader() noexcept : BrushShader(StaticTypeId()) {}
    ~BrushShader() override = default;
protected:
    explicit BrushShader(TypeId runtimeType) noexcept
        : runtimeType_(runtimeType) {}
    void BumpRevision() noexcept { ++revision_; }
private:
    TypeId runtimeType_;
    std::uint64_t revision_ = 0U;
};

class AERO_GUI_API MonochromeShader : public BrushShader {
    AERO_DECLARE_TYPE(MonochromeShader, BrushShader)
public:
    MonochromeShader() noexcept : BrushShader(StaticTypeId()) {}
    Color GetColor() const noexcept { return color_; }
    void SetColor(Color value) noexcept {
        color_ = value;
        BumpRevision();
    }
private:
    Color color_{};
};

class AERO_GUI_API ConicGradientShader : public BrushShader {
    AERO_DECLARE_TYPE(ConicGradientShader, BrushShader)
public:
    ConicGradientShader() noexcept
        : BrushShader(StaticTypeId()),
          stops_(&Base::GetDefaultAllocator()) {}
    void AddGradientStop(Ref<GradientStop> value) noexcept {
        if (!value) { AERO_ASSERT(false); return; }
        stops_.PushBack(std::move(value));
    }
    void ClearGradientStops() noexcept { stops_.Clear(); }
    Span<const Ref<GradientStop>> GetGradientStops() const noexcept {
        return stops_.AsSpan();
    }
private:
    Base::Vector<Ref<GradientStop>> stops_;
};

class AERO_GUI_API WavesShader : public BrushShader {
    AERO_DECLARE_TYPE(WavesShader, BrushShader)
public:
    WavesShader() noexcept : BrushShader(StaticTypeId()) {}
    double GetTime() const noexcept { return time_; }
    void SetTime(double value) noexcept {
        time_ = value;
        BumpRevision();
    }
private:
    double time_ = 0.0;
};

class AERO_GUI_API SolidColorBrush : public Brush {
    AERO_DECLARE_TYPE(SolidColorBrush, Brush)
public:
    SolidColorBrush() noexcept
        : Brush(StaticTypeId()) {}
    explicit SolidColorBrush(Color color) noexcept
        : Brush(StaticTypeId()), initialColor_(color) {}
    ~SolidColorBrush() override = default;

    Color GetColor() const noexcept;
    void SetColor(Color value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Color, Color);

private:
    Color initialColor_{};
};

AERO_GUI_API Result<Ref<Brush>>
MakeSolidColorBrush(Color color) noexcept;

class AERO_GUI_API GradientBrush : public Brush {
    AERO_DECLARE_TYPE(GradientBrush, Brush)
public:
    Span<const Ref<GradientStop>>
        GetGradientStops() const noexcept {
        return stops_.AsSpan();
    }
    void AddGradientStop(
        Ref<GradientStop> stop) noexcept;
    void ClearGradientStops() noexcept;
    BrushMappingMode GetMappingMode() const noexcept;
    void SetMappingMode(BrushMappingMode value) noexcept;
    GradientSpreadMethod GetSpreadMethod() const noexcept;
    void SetSpreadMethod(GradientSpreadMethod value) noexcept;

    AERO_DEPENDENCY_PROPERTY(BrushMappingMode, MappingMode);
    AERO_DEPENDENCY_PROPERTY(GradientSpreadMethod, SpreadMethod);

protected:
    explicit GradientBrush(TypeId runtimeType) noexcept
        : Brush(runtimeType),
          stops_(&Base::GetDefaultAllocator()) {}
    ~GradientBrush() override;
    bool FreezeCore(bool isChecking) noexcept override;

private:
    void OnGradientStopChanged(Freezable&) noexcept;
    Base::Vector<Ref<GradientStop>> stops_;
    FreezableChangedHandler stopChangedHandler_;
};

class AERO_GUI_API LinearGradientBrush
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

    AERO_DEPENDENCY_PROPERTY(Point, StartPoint);
    AERO_DEPENDENCY_PROPERTY(Point, EndPoint);
};

class AERO_GUI_API RadialGradientBrush
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
    void SetGradientOrigin(Point value) noexcept;
    void SetRadiusX(double value) noexcept;
    void SetRadiusY(double value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Point, Center);
    AERO_DEPENDENCY_PROPERTY(Point, GradientOrigin);
    AERO_DEPENDENCY_PROPERTY(double, RadiusX);
    AERO_DEPENDENCY_PROPERTY(double, RadiusY);
};

class AERO_GUI_API TileBrush : public Brush {
    AERO_DECLARE_TYPE(TileBrush, Brush)
public:
    Stretch GetStretch() const noexcept;
    Rect GetViewbox() const noexcept;
    Rect GetViewport() const noexcept;
    BrushMappingMode GetViewboxUnits() const noexcept;
    BrushMappingMode GetViewportUnits() const noexcept;
    TileMode GetTileMode() const noexcept;
    HorizontalAlignment GetAlignmentX() const noexcept;
    VerticalAlignment GetAlignmentY() const noexcept;

    void SetStretch(Stretch value) noexcept;
    void SetViewbox(Rect value) noexcept;
    void SetViewport(Rect value) noexcept;
    void SetViewboxUnits(BrushMappingMode value) noexcept;
    void SetViewportUnits(BrushMappingMode value) noexcept;
    void SetTileMode(TileMode value) noexcept;
    void SetAlignmentX(HorizontalAlignment value) noexcept;
    void SetAlignmentY(VerticalAlignment value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Stretch, Stretch);
    AERO_DEPENDENCY_PROPERTY(Rect, Viewbox);
    AERO_DEPENDENCY_PROPERTY(Rect, Viewport);
    AERO_DEPENDENCY_PROPERTY(BrushMappingMode, ViewboxUnits);
    AERO_DEPENDENCY_PROPERTY(BrushMappingMode, ViewportUnits);
    AERO_DEPENDENCY_PROPERTY(TileMode, TileMode);
    AERO_DEPENDENCY_PROPERTY(HorizontalAlignment, AlignmentX);
    AERO_DEPENDENCY_PROPERTY(VerticalAlignment, AlignmentY);

protected:
    explicit TileBrush(TypeId runtimeType) noexcept
        : Brush(runtimeType) {}
};

class AERO_GUI_API ImageBrush : public TileBrush {
    AERO_DECLARE_TYPE(ImageBrush, TileBrush)
public:
    ImageBrush() noexcept
        : TileBrush(StaticTypeId()) {}
    ~ImageBrush() override = default;

    Ref<ImageSource> GetSource() const noexcept;
    void SetSource(Ref<ImageSource> value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Ref<ImageSource>, ImageSource);

    std::uint64_t GetRenderImageId() const noexcept { return renderImage_; }
    std::uint32_t GetPixelWidth() const noexcept { return pixelWidth_; }
    std::uint32_t GetPixelHeight() const noexcept { return pixelHeight_; }
    void SetRuntimeImage(std::uint64_t image, std::uint32_t width, std::uint32_t height) noexcept;

private:
    std::uint64_t renderImage_ = 0U;
    std::uint32_t pixelWidth_ = 0U;
    std::uint32_t pixelHeight_ = 0U;
};

class AERO_GUI_API VisualBrush : public TileBrush {
    AERO_DECLARE_TYPE(VisualBrush, TileBrush)
public:
    VisualBrush() noexcept
        : TileBrush(StaticTypeId()) {}
    ~VisualBrush() override = default;

    Ref<Base::Object> GetVisual() const noexcept {
        return GetValue(VisualProperty);
    }
    void SetVisual(Ref<Base::Object> value) noexcept {
        SetValue(VisualProperty, std::move(value));
    }

    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, Visual);
};

} // namespace Aero::Media
AERO_DECLARE_TYPE_ENUM(Aero::Media::TileMode)
AERO_DECLARE_TYPE_ENUM(Aero::Media::BrushMappingMode)
AERO_DECLARE_TYPE_ENUM(Aero::Media::GradientSpreadMethod)
