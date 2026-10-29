#pragma once

// Brush family: Brush plus stops, shaders, and concrete brushes.
#include <Aero/Animatable.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Collections.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/ElementEnums.hpp>
#include <Aero/Media/FreezableCollection.hpp>
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

    std::uint64_t GetRevision() const noexcept;

    AERO_DEPENDENCY_PROPERTY2(double, Opacity);
    AERO_DEPENDENCY_PROPERTY2(Ref<Base::Object>, Shader);
    AERO_DEPENDENCY_PROPERTY2(Ref<Transform>, RelativeTransform);

protected:
    explicit Brush(TypeId runtimeType) noexcept : Animatable(runtimeType) {}
    ~Brush() override = default;
};

class AERO_GUI_API GradientStop : public Animatable {
    AERO_DECLARE_TYPE(GradientStop, Animatable)

public:
    GradientStop() noexcept : Animatable(StaticTypeId()) {}
    ~GradientStop() override = default;

    std::uint64_t GetRevision() const noexcept { return Revision(); }

    AERO_DEPENDENCY_PROPERTY2(double, Offset);
    AERO_DEPENDENCY_PROPERTY2(Color, Color);

};

class AERO_GUI_API GradientStopCollection : public FreezableCollection<GradientStop> {
    AERO_DECLARE_TYPE(GradientStopCollection, FreezableCollection<GradientStop>)

public:
    GradientStopCollection() noexcept
        : FreezableCollection<GradientStop>(StaticTypeId()) {}
    ~GradientStopCollection() override = default;

protected:
    Base::Ref<Freezable> CreateInstanceCore() const override {
        return Base::MakeRef<GradientStopCollection>();
    }
};

// Shader parameters are a revision counter plus optional dependency
// properties declared by a derived shader. XAML bindings require the owner
// to be a DependencyObject.
class AERO_GUI_API BrushShader : public Animatable {
    AERO_DECLARE_TYPE(BrushShader, Animatable)

public:
    std::uint64_t GetRevision() const noexcept { return revision_; }
    BrushShader() noexcept : Animatable(StaticTypeId()) {}
    ~BrushShader() override = default;

protected:
    explicit BrushShader(TypeId runtimeType) noexcept
        : Animatable(runtimeType) {}
    void BumpRevision() noexcept { ++revision_; }
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override {
        Animatable::OnPropertyChanged(args);
        BumpRevision();
    }

private:
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

protected:
    Base::Ref<Freezable> CreateInstanceCore() const override {
        return Base::MakeRef<MonochromeShader>();
    }

private:
    Color color_{};
};

class AERO_GUI_API ConicGradientShader : public BrushShader {
    AERO_DECLARE_TYPE(ConicGradientShader, BrushShader)

public:
    ConicGradientShader() noexcept : BrushShader(StaticTypeId()),
          stops_(&Base::GetDefaultAllocator()) {}
    void AddGradientStop(Ref<GradientStop> value) noexcept {
        if (!value) { AERO_ASSERT(false); return; }
        stops_.PushBack(std::move(value));
    }
    void ClearGradientStops() noexcept { stops_.Clear(); }
    Span<const Ref<GradientStop>> GetGradientStops() const noexcept { return stops_.AsSpan(); }

protected:
    Base::Ref<Freezable> CreateInstanceCore() const override {
        return Base::MakeRef<ConicGradientShader>();
    }

private:
    Base::Vector<Ref<GradientStop>> stops_;
};

class AERO_GUI_API WavesShader : public BrushShader {
    AERO_DECLARE_TYPE(WavesShader, BrushShader)

public:
    WavesShader() noexcept : BrushShader(StaticTypeId()) {}

    AERO_DEPENDENCY_PROPERTY2(double, Time);

protected:
    Base::Ref<Freezable> CreateInstanceCore() const override {
        return Base::MakeRef<WavesShader>();
    }
};

class AERO_GUI_API SolidColorBrush : public Brush {
    AERO_DECLARE_TYPE(SolidColorBrush, Brush)

public:
    SolidColorBrush() noexcept : Brush(StaticTypeId()) {}
    explicit SolidColorBrush(Color color) noexcept : Brush(StaticTypeId()), initialColor_(color) {}
    ~SolidColorBrush() override = default;

    AERO_DEPENDENCY_PROPERTY2(Color, Color);

private:
    Color initialColor_{};
};

AERO_GUI_API Ref<Brush> MakeSolidColorBrush(Color color) noexcept;

class AERO_GUI_API GradientBrush : public Brush {
    AERO_DECLARE_TYPE(GradientBrush, Brush)

public:
    Span<const Ref<GradientStop>> GetGradientStops() const noexcept { return stops_.AsSpan(); }
    void AddGradientStop(Ref<GradientStop> stop) noexcept;
    void ClearGradientStops() noexcept;

    AERO_DEPENDENCY_PROPERTY2(BrushMappingMode, MappingMode);
    AERO_DEPENDENCY_PROPERTY2(GradientSpreadMethod, SpreadMethod);

protected:
    explicit GradientBrush(TypeId runtimeType) noexcept : Brush(runtimeType),
          stops_(&Base::GetDefaultAllocator()) {}
    ~GradientBrush() override;
    bool FreezeCore(bool isChecking) noexcept override;
    void CloneCore(const Freezable& source) override;
    void CloneCurrentValueCore(const Freezable& source) override;

private:
    void OnGradientStopChanged(Freezable&) noexcept;
    Base::Vector<Ref<GradientStop>> stops_;
    FreezableChangedHandler stopChangedHandler_;
};

class AERO_GUI_API LinearGradientBrush : public GradientBrush {
    AERO_DECLARE_TYPE(LinearGradientBrush, GradientBrush)

public:
    LinearGradientBrush() noexcept : GradientBrush(StaticTypeId()) {}
    ~LinearGradientBrush() override = default;

    AERO_DEPENDENCY_PROPERTY2(Point, StartPoint);
    AERO_DEPENDENCY_PROPERTY2(Point, EndPoint);
};

class AERO_GUI_API RadialGradientBrush : public GradientBrush {
    AERO_DECLARE_TYPE(RadialGradientBrush, GradientBrush)

public:
    RadialGradientBrush() noexcept : GradientBrush(StaticTypeId()) {}
    ~RadialGradientBrush() override = default;

    AERO_DEPENDENCY_PROPERTY2(Point, Center);
    AERO_DEPENDENCY_PROPERTY2(Point, GradientOrigin);
    AERO_DEPENDENCY_PROPERTY2(double, RadiusX);
    AERO_DEPENDENCY_PROPERTY2(double, RadiusY);
};

class AERO_GUI_API TileBrush : public Brush {
    AERO_DECLARE_TYPE(TileBrush, Brush)

public:
    AERO_DEPENDENCY_PROPERTY2(Stretch, Stretch);
    AERO_DEPENDENCY_PROPERTY2(Rect, Viewbox);
    AERO_DEPENDENCY_PROPERTY2(Rect, Viewport);
    AERO_DEPENDENCY_PROPERTY2(BrushMappingMode, ViewboxUnits);
    AERO_DEPENDENCY_PROPERTY2(BrushMappingMode, ViewportUnits);
    AERO_DEPENDENCY_PROPERTY2(TileMode, TileMode);
    AERO_DEPENDENCY_PROPERTY2(HorizontalAlignment, AlignmentX);
    AERO_DEPENDENCY_PROPERTY2(VerticalAlignment, AlignmentY);

protected:
    explicit TileBrush(TypeId runtimeType) noexcept : Brush(runtimeType) {}
};

class AERO_GUI_API ImageBrush : public TileBrush {
    AERO_DECLARE_TYPE(ImageBrush, TileBrush)

public:
    ImageBrush() noexcept : TileBrush(StaticTypeId()) {}
    ~ImageBrush() override = default;

    AERO_DEPENDENCY_PROPERTY2(Ref<ImageSource>, ImageSource);

    Ref<ImageSource> GetSource() const noexcept { return GetImageSource(); }
    void SetSource(Ref<ImageSource> value) noexcept { SetImageSource(std::move(value)); }

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
    VisualBrush() noexcept : TileBrush(StaticTypeId()) {}
    ~VisualBrush() override = default;

    AERO_DEPENDENCY_PROPERTY2(Ref<Base::Object>, Visual);
};

} // namespace Aero::Media
AERO_DECLARE_TYPE_ENUM(Aero::Media::TileMode)
AERO_DECLARE_TYPE_ENUM(Aero::Media::BrushMappingMode)
AERO_DECLARE_TYPE_ENUM(Aero::Media::GradientSpreadMethod)
