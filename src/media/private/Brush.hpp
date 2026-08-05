#pragma once

#include "../../render/DisplayList.hpp"

#include <Aero/Media/Brushes.hpp>

#include <algorithm>

namespace Aero::Media::Detail {

inline Base::Color SampleGradient(
    const Media::GradientBrush& brush,
    double position) noexcept {
    const auto stops = brush.GetGradientStops();
    if (stops.Empty()) return {};
    position = std::clamp(position, 0.0, 1.0);
    const Media::GradientStop* lower = nullptr;
    const Media::GradientStop* upper = nullptr;
    for (const Base::Ref<Media::GradientStop>& stop : stops) {
        if (!stop) continue;
        if (stop->GetOffset() <= position &&
            (lower == nullptr || stop->GetOffset() >= lower->GetOffset())) {
            lower = stop.Get();
        }
        if (stop->GetOffset() >= position &&
            (upper == nullptr || stop->GetOffset() <= upper->GetOffset())) {
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

// Brush-to-color sampling is a renderer concern, not part of the WPF Brush
// authoring surface. Keep the fallback sampler private to implementation
// units that need to rasterize a brush into a display list.
inline Base::Color SampleBrush(
    const Base::Ref<Media::Brush>& brush,
    double position = 0.5,
    Base::Color fallback = {0.0F, 0.0F, 0.0F, 0.0F}) noexcept {
    if (!brush) return fallback;
    if (brush->RuntimeType() == Media::SolidColorBrush::StaticTypeId()) {
        Base::Color sampled = static_cast<Media::SolidColorBrush*>(
            brush.Get())->GetColor();
        sampled.alpha *= static_cast<float>(brush->GetOpacity());
        return sampled;
    }
    if (brush->RuntimeType() == Media::LinearGradientBrush::StaticTypeId() ||
        brush->RuntimeType() == Media::RadialGradientBrush::StaticTypeId()) {
        return SampleGradient(
            *static_cast<Media::GradientBrush*>(brush.Get()), position);
    }
    return fallback;
}

} // namespace Aero::Media::Detail

namespace Aero::Media {

struct Brush::Impl {
public:
    static Render::RenderImageId RuntimeImage(
        const ImageBrush& brush) noexcept {
        return brush.renderImage_;
    }

    static std::uint32_t PixelWidth(
        const ImageBrush& brush) noexcept {
        return brush.pixelWidth_;
    }

    static std::uint32_t PixelHeight(
        const ImageBrush& brush) noexcept {
        return brush.pixelHeight_;
    }

    static Base::Result<void> SetRuntimeImage(
        ImageBrush& brush,
        Render::RenderImageId image,
        std::uint32_t width,
        std::uint32_t height) noexcept {
        const bool changed =
            brush.renderImage_ != image ||
            brush.pixelWidth_ != width ||
            brush.pixelHeight_ != height;
        brush.renderImage_ = image;
        brush.pixelWidth_ = width;
        brush.pixelHeight_ = height;
        if (changed) brush.WritePostscript();
        return {};
    }

    static std::uint64_t Revision(const Brush& brush) noexcept;
};

} // namespace Aero::Media

namespace Aero::Media::Detail {

using BrushPrivate = ::Aero::Media::Brush::Impl;

} // namespace Aero::Media::Detail
