#pragma once

// Brush paint helpers + gradient/shader sampling (absorbed from BrushRendering/AnimationModel.hpp).

#include "render/DisplayList.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/meta/TypeRegistryDetail.hpp"

#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Base/Span.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Aero::Media {

inline bool IsSpatialGradientBrush(const Brush* brush) noexcept {
    return TryCast<LinearGradientBrush>(brush) != nullptr ||
        TryCast<RadialGradientBrush>(brush) != nullptr;
}

Base::Result<void> PaintBrushRect(
    Render::DisplayListBuilder& builder,
    const Base::Ref<Brush>& brush,
    Rect bounds,
    double cornerRadius = 0.0,
    bool isRtl = false) noexcept;

// Ellipse/rounded-rect pen as an annulus (same construction as StrokeRect).
// LinearGradientBrush maps through `bounds`; do not Flatten+tessellate.
Base::Result<void> PaintBrushRoundedStroke(
    Render::DisplayListBuilder& builder,
    const Base::Ref<Brush>& brush,
    Rect bounds,
    double thickness,
    double cornerRadius,
    bool isRtl = false) noexcept;

Base::Result<void> PaintBrushGeometry(
    Render::DisplayListBuilder& builder,
    const Base::Ref<Brush>& brush,
    Base::Span<const Point> vertices,
    Base::Span<const std::uint32_t> indices,
    Rect bounds,
    bool isRtl = false,
    Render::RenderMeshId mesh = Render::InvalidRenderMeshId) noexcept;


inline double SpreadPosition(
    double value,
    Media::GradientSpreadMethod method) noexcept {
    if (method == Media::GradientSpreadMethod::Repeat) {
        value -= std::floor(value);
        return value < 0.0 ? value + 1.0 : value;
    }
    if (method == Media::GradientSpreadMethod::Reflect) {
        value = std::fmod(std::fabs(value), 2.0);
        return value <= 1.0 ? value : 2.0 - value;
    }
    return std::clamp(value, 0.0, 1.0);
}

inline Base::Color SampleGradient(
    const Media::GradientBrush& brush,
    double position) noexcept {
    const auto stops = brush.GetGradientStops();
    if (stops.Empty()) return {};
    position = SpreadPosition(position, brush.GetSpreadMethod());
    const Media::GradientStop* lower = nullptr;
    const Media::GradientStop* upper = nullptr;
    for (const Base::Ref<Media::GradientStop>& stop : stops) {
        if (!stop) continue;
        if (stop->GetOffset() <= position &&
            (lower == nullptr || stop->GetOffset() >= lower->GetOffset())) {
            lower = stop.Get();
        }
        if (stop->GetOffset() >= position &&
            (upper == nullptr || stop->GetOffset() < upper->GetOffset())) {
            upper = stop.Get();
        }
    }
    if (lower == nullptr) lower = upper;
    if (upper == nullptr) upper = lower;
    if (lower == nullptr || upper == nullptr) return {};
    const double span = upper->GetOffset() - lower->GetOffset();
    const float amount = span > 0.0
        ? static_cast<float>(std::clamp(
            (position - lower->GetOffset()) / span, 0.0, 1.0))
        : 0.0F;
    const Base::Color a = lower->GetColor();
    const Base::Color b = upper->GetColor();
    Base::Color result{
        a.red + (b.red - a.red) * amount,
        a.green + (b.green - a.green) * amount,
        a.blue + (b.blue - a.blue) * amount,
        a.alpha + (b.alpha - a.alpha) * amount};
    result.alpha *= static_cast<float>(brush.GetOpacity());
    return result;
}

inline double ShaderDouble(
    const Media::BrushShader& shader,
    Base::StringView name,
    double fallback) noexcept {
    const Meta::DependencyProperty* property =
        AeroGuiInternal::PropertyRegistry(shader).Find(shader.RuntimeType(), name);
    if (property == nullptr) return fallback;
    Base::Result<Meta::Value> value =
        shader.GetValue(property->Handle());
    if (!value) return fallback;
    Base::Result<double> decoded =
        Meta::ValueCodec<double>::Decode(value.Value());
    return decoded ? decoded.Value() : fallback;
}

inline Base::Color ShaderColor(
    const Media::BrushShader& shader,
    Base::StringView name,
    Base::Color fallback) noexcept {
    const Meta::DependencyProperty* property =
        AeroGuiInternal::PropertyRegistry(shader).Find(shader.RuntimeType(), name);
    if (property == nullptr) return fallback;
    Base::Result<Meta::Value> value =
        shader.GetValue(property->Handle());
    if (!value) return fallback;
    Base::Result<Base::Color> decoded =
        Meta::ValueCodec<Base::Color>::Decode(value.Value());
    return decoded ? decoded.Value() : fallback;
}

inline Base::Color SampleStops(
    Base::Span<const Base::Ref<Media::GradientStop>> stops,
    double position,
    Base::Color fallback) noexcept {
    if (stops.Empty()) return fallback;
    position = SpreadPosition(
        position, Media::GradientSpreadMethod::Repeat);
    const Media::GradientStop* lower = nullptr;
    const Media::GradientStop* upper = nullptr;
    for (const Base::Ref<Media::GradientStop>& stop : stops) {
        if (!stop) continue;
        if (stop->GetOffset() <= position &&
            (lower == nullptr || stop->GetOffset() > lower->GetOffset())) {
            lower = stop.Get();
        }
        if (stop->GetOffset() >= position &&
            (upper == nullptr || stop->GetOffset() < upper->GetOffset())) {
            upper = stop.Get();
        }
    }
    if (lower == nullptr) lower = upper;
    if (upper == nullptr) upper = lower;
    if (lower == nullptr || upper == nullptr) return fallback;
    const double span = upper->GetOffset() - lower->GetOffset();
    const float amount = span > 0.0
        ? static_cast<float>((position - lower->GetOffset()) / span)
        : 0.0F;
    const Base::Color a = lower->GetColor();
    const Base::Color b = upper->GetColor();
    return {
        a.red + (b.red - a.red) * amount,
        a.green + (b.green - a.green) * amount,
        a.blue + (b.blue - a.blue) * amount,
        a.alpha + (b.alpha - a.alpha) * amount};
}

inline Base::Color ApplyShader(
    const Media::BrushShader& shader,
    Base::Color source,
    Base::Point uv,
    Base::Size) noexcept {
    if (shader.RuntimeType() ==
        Media::MonochromeShader::StaticTypeId()) {
        const Base::Color color =
            static_cast<const Media::MonochromeShader&>(shader).GetColor();
        const float luminance =
            source.red * 0.2126F +
            source.green * 0.7152F +
            source.blue * 0.0722F;
        return {color.red * luminance, color.green * luminance,
            color.blue * luminance, source.alpha * color.alpha};
    }
    if (shader.RuntimeType() ==
        Media::ConicGradientShader::StaticTypeId()) {
        const double angle =
            std::atan2(uv.y - 0.5, uv.x - 0.5) /
                (2.0 * 3.14159265358979323846) + 0.5;
        return SampleStops(
            static_cast<const Media::ConicGradientShader&>(shader)
                .GetGradientStops(),
            angle, source);
    }
    const double time = ShaderDouble(shader, "Time", 0.0);
    if (shader.RuntimeType() == Media::WavesShader::StaticTypeId()) {
        const float wave = static_cast<float>(
            0.65 + 0.35 * std::sin(
                (uv.x * 18.0 + uv.y * 12.0) + time * 0.01));
        return {source.red * wave, source.green * wave,
            source.blue * wave, source.alpha};
    }
    const double scaleX = ShaderDouble(shader, "ScaleX", 64.0);
    const double scaleY = ShaderDouble(shader, "ScaleY", 64.0);
    const double seed = ShaderDouble(shader, "Seed", 0.0);
    const double phase =
        std::sin((uv.x * scaleX + uv.y * scaleY + seed) * 12.9898 +
                 time * 0.0001) * 43758.5453;
    const float noise = static_cast<float>(phase - std::floor(phase));
    const Base::Color color = ShaderColor(shader, "Color", source);
    const float amount = 0.35F + noise * 0.65F;
    return {color.red * amount, color.green * amount,
        color.blue * amount, source.alpha * color.alpha};
}

inline Base::Color SampleBrush(
    const Base::Ref<Media::Brush>& brush,
    double position = 0.5,
    Base::Color fallback = {0.0F, 0.0F, 0.0F, 0.0F},
    Base::Point uv = {0.5, 0.5},
    Base::Size size = {1.0, 1.0}) noexcept {
    if (!brush) return fallback;
    Base::Color sampled = fallback;
    if (brush->RuntimeType() == Media::SolidColorBrush::StaticTypeId()) {
        sampled = static_cast<Media::SolidColorBrush*>(
            brush.Get())->GetColor();
        sampled.alpha *= static_cast<float>(brush->GetOpacity());
    } else if (brush->RuntimeType() ==
                   Media::LinearGradientBrush::StaticTypeId() ||
               brush->RuntimeType() ==
                   Media::RadialGradientBrush::StaticTypeId()) {
        sampled = SampleGradient(
            *static_cast<Media::GradientBrush*>(brush.Get()), position);
    } else if (brush->RuntimeType() == Media::ImageBrush::StaticTypeId()) {
        sampled = {1.0F, 1.0F, 1.0F,
            static_cast<float>(brush->GetOpacity())};
    }
    Base::Ref<Base::Object> shaderObject = brush->GetShader();
    if (shaderObject && AeroGuiInternal::PropertyRegistry(brush.Get()).Types().IsDerivedFrom(
            shaderObject->RuntimeType(), Media::BrushShader::StaticTypeId())) {
        sampled = ApplyShader(
            static_cast<const Media::BrushShader&>(*shaderObject),
            sampled, uv, size);
    }
    return sampled;
}

} // namespace Aero::Media
