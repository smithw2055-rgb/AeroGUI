#include "../render/DisplayList.hpp"
#include <Aero/Controls/Panels.hpp>
#include <Aero/Controls/Common.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace Aero::Controls {

using namespace ::Aero::Render;

namespace {

double LimitScale(
    double scale,
    StretchDirection direction) noexcept {
    if (!std::isfinite(scale) ||
        scale < 0.0) {
        return 0.0;
    }
    if (direction == StretchDirection::UpOnly) {
        return std::max(1.0, scale);
    }
    if (direction ==
        StretchDirection::DownOnly) {
        return std::min(1.0, scale);
    }
    return scale;
}

} // namespace

Base::Ref<ImageSource> Image::GetSource() const noexcept {
    return GetValueOr(
        SourceProperty,
        Base::Ref<ImageSource>{});
}

Stretch Image::GetStretch() const noexcept {
    return GetValueOr(
        StretchProperty,
        Stretch::Uniform);
}

StretchDirection
Image::GetStretchDirection() const noexcept {
    return GetValueOr(
        StretchDirectionProperty,
        StretchDirection::Both);
}

void Image::SetSource(
    Base::Ref<ImageSource> value) noexcept {
    SetValue(
        SourceProperty,
        std::move(value));
}

void Image::SetStretch(
    Stretch value) noexcept {
    SetValue(StretchProperty, value);
}

void Image::SetStretchDirection(
    StretchDirection value) noexcept {
    SetValue(
        StretchDirectionProperty, value);
}

Size Image::MeasureOverride(
    Size availableSize) noexcept {
    if (pixelWidth_ == 0U ||
        pixelHeight_ == 0U) {
        return Size{};
    }
    const Size natural{
        static_cast<double>(pixelWidth_),
        static_cast<double>(pixelHeight_)};
    if (GetStretch() == Stretch::None) {
        return natural;
    }
    const double scaleX =
        std::isfinite(availableSize.width) &&
            availableSize.width > 0.0
        ? availableSize.width / natural.width
        : 1.0;
    const double scaleY =
        std::isfinite(availableSize.height) &&
            availableSize.height > 0.0
        ? availableSize.height / natural.height
        : 1.0;
    double scale = GetStretch() ==
            Stretch::UniformToFill
        ? std::max(scaleX, scaleY)
        : std::min(scaleX, scaleY);
    if (GetStretch() == Stretch::Fill) {
        return Size{
            natural.width * LimitScale(
                scaleX,
                GetStretchDirection()),
            natural.height * LimitScale(
                scaleY,
                GetStretchDirection())};
    }
    scale = LimitScale(
        scale, GetStretchDirection());
    return Size{
        natural.width * scale,
        natural.height * scale};
}

void Image::OnRender(
    DrawingContext& context) noexcept {
    auto& builder = Aero::Render::Detail::DrawingPrivate::Builder(context);
    if (renderImage_ ==
            InvalidRenderImageId ||
        pixelWidth_ == 0U ||
        pixelHeight_ == 0U) {
        return;
    }
    const Size size = GetRenderSize();
    if (size.width <= 0.0 ||
        size.height <= 0.0) {
        return;
    }
    const double sourceWidth =
        static_cast<double>(pixelWidth_);
    const double sourceHeight =
        static_cast<double>(pixelHeight_);
    const Stretch stretch = GetStretch();
    Rect destination{
        0.0, 0.0, size.width, size.height};
    Rect uv{0.0, 0.0, 1.0, 1.0};
    if (stretch == Stretch::None) {
        const double scale = LimitScale(
            1.0, GetStretchDirection());
        destination.width =
            sourceWidth * scale;
        destination.height =
            sourceHeight * scale;
    } else if (stretch != Stretch::Fill) {
        const double scaleX =
            size.width / sourceWidth;
        const double scaleY =
            size.height / sourceHeight;
        double scale = stretch ==
                Stretch::UniformToFill
            ? std::max(scaleX, scaleY)
            : std::min(scaleX, scaleY);
        scale = LimitScale(
            scale, GetStretchDirection());
        const double drawnWidth =
            sourceWidth * scale;
        const double drawnHeight =
            sourceHeight * scale;
        if (stretch ==
                Stretch::UniformToFill &&
            drawnWidth > 0.0 &&
            drawnHeight > 0.0) {
            uv.width =
                std::clamp(
                    size.width / drawnWidth,
                    0.0, 1.0);
            uv.height =
                std::clamp(
                    size.height / drawnHeight,
                    0.0, 1.0);
            uv.x = (1.0 - uv.width) * 0.5;
            uv.y = (1.0 - uv.height) * 0.5;
        } else {
            destination.width = drawnWidth;
            destination.height = drawnHeight;
        }
    }
    destination.x =
        (size.width - destination.width) * 0.5;
    destination.y =
        (size.height - destination.height) * 0.5;
    static_cast<void>(builder.DrawImage(
        renderImage_, destination, uv));
}

} // namespace Aero::Controls
