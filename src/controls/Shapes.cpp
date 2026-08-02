#include "../render/DisplayList.hpp"
#include <Aero/Shapes.hpp>
#include "../render/DrawingInternals.hpp"

#include "media/BrushInternals.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Aero::Shapes {

using namespace ::Aero::Media;
using namespace ::Aero::Render;

namespace {

struct ImageBrushGeometry {
    Rect destination;
    Rect sourceUv;
};

ImageBrushGeometry FitImageBrush(
    const ImageBrush& brush,
    Rect destination,
    Rect sourceUv) noexcept {
    const double sourceWidth =
        static_cast<double>(
            Aero::Internal::BrushPrivate::
                PixelWidth(brush)) *
        sourceUv.width;
    const double sourceHeight =
        static_cast<double>(
            Aero::Internal::BrushPrivate::
                PixelHeight(brush)) *
        sourceUv.height;
    if (sourceWidth <= 0.0 ||
        sourceHeight <= 0.0 ||
        destination.width <= 0.0 ||
        destination.height <= 0.0 ||
        brush.GetStretch() == Stretch::Fill) {
        return {destination, sourceUv};
    }
    if (brush.GetStretch() == Stretch::None) {
        destination.width = sourceWidth;
        destination.height = sourceHeight;
        return {destination, sourceUv};
    }
    const double scaleX =
        destination.width / sourceWidth;
    const double scaleY =
        destination.height / sourceHeight;
    if (brush.GetStretch() ==
        Stretch::UniformToFill) {
        const double scale =
            std::max(scaleX, scaleY);
        const double drawnWidth =
            sourceWidth * scale;
        const double drawnHeight =
            sourceHeight * scale;
        sourceUv.x +=
            sourceUv.width *
            (1.0 -
             destination.width / drawnWidth) *
            0.5;
        sourceUv.y +=
            sourceUv.height *
            (1.0 -
             destination.height / drawnHeight) *
            0.5;
        sourceUv.width *=
            destination.width / drawnWidth;
        sourceUv.height *=
            destination.height / drawnHeight;
        return {destination, sourceUv};
    }
    const double scale =
        std::min(scaleX, scaleY);
    const double fittedWidth =
        sourceWidth * scale;
    const double fittedHeight =
        sourceHeight * scale;
    destination.x +=
        (destination.width - fittedWidth) * 0.5;
    destination.y +=
        (destination.height - fittedHeight) * 0.5;
    destination.width = fittedWidth;
    destination.height = fittedHeight;
    return {destination, sourceUv};
}

Base::Result<void> PaintImageBrush(
    DrawingContext& context,
    const ImageBrush& brush,
    Rect bounds) noexcept {
    auto& builder = Aero::Internal::DrawingPrivate::Builder(context);
    const RenderImageId image =
        Aero::Internal::BrushPrivate::
            RuntimeImage(brush);
    if (image == InvalidRenderImageId ||
        Aero::Internal::BrushPrivate::
            PixelWidth(brush) == 0U ||
        Aero::Internal::BrushPrivate::
            PixelHeight(brush) == 0U) {
        return {};
    }
    const Rect authoredViewbox =
        brush.GetViewbox();
    Rect sourceUv{
        std::clamp(authoredViewbox.x, 0.0, 1.0),
        std::clamp(authoredViewbox.y, 0.0, 1.0),
        std::clamp(authoredViewbox.width, 0.0, 1.0),
        std::clamp(authoredViewbox.height, 0.0, 1.0)};
    sourceUv.width = std::min(
        sourceUv.width, 1.0 - sourceUv.x);
    sourceUv.height = std::min(
        sourceUv.height, 1.0 - sourceUv.y);
    if (sourceUv.width <= 0.0 ||
        sourceUv.height <= 0.0) {
        return {};
    }

    const Rect authoredViewport =
        brush.GetViewport();
    Rect cell{
        bounds.x +
            authoredViewport.x * bounds.width,
        bounds.y +
            authoredViewport.y * bounds.height,
        authoredViewport.width * bounds.width,
        authoredViewport.height * bounds.height};
    if (cell.width <= 0.0 ||
        cell.height <= 0.0) {
        return {};
    }
    const Color tint{
        1.0F, 1.0F, 1.0F,
        static_cast<float>(brush.GetOpacity())};
    if (brush.GetTileMode() == TileMode::None) {
        const ImageBrushGeometry geometry =
            FitImageBrush(
                brush, cell, sourceUv);
        return builder.DrawImage(
            image,
            geometry.destination,
            geometry.sourceUv,
            tint);
    }

    Base::Result<void> clipped =
        builder.PushClip(bounds);
    if (!clipped) return clipped.GetStatus();
    Base::Status failure;
    constexpr std::uint32_t maxTiles = 4096U;
    std::uint32_t tileCount = 0U;
    const double startX =
        bounds.x +
        std::floor(
            (bounds.x - cell.x) /
            cell.width) * cell.width;
    const double startY =
        bounds.y +
        std::floor(
            (bounds.y - cell.y) /
            cell.height) * cell.height;
    for (double y = startY;
         y < bounds.y + bounds.height &&
         tileCount < maxTiles;
         y += cell.height) {
        for (double x = startX;
             x < bounds.x + bounds.width &&
             tileCount < maxTiles;
             x += cell.width) {
            Rect tile{
                x, y, cell.width, cell.height};
            const ImageBrushGeometry geometry =
                FitImageBrush(
                    brush, tile, sourceUv);
            Base::Result<void> painted =
                builder.DrawImage(
                    image,
                    geometry.destination,
                    geometry.sourceUv,
                    tint);
            if (!painted) {
                failure = painted.GetStatus();
                break;
            }
            ++tileCount;
        }
        if (!failure.IsOk()) break;
    }
    Base::Result<void> popped =
        builder.PopClip();
    if (!failure.IsOk()) return failure;
    return popped;
}

} // namespace

Base::Ref<Brush> Shape::GetFill() const noexcept {
    return GetValueOr(
        FillProperty, Base::Ref<Brush>{});
}

Base::Ref<Brush> Shape::GetStroke() const noexcept {
    return GetValueOr(
        StrokeProperty, Base::Ref<Brush>{});
}

double Shape::GetStrokeThickness() const noexcept {
    return GetValueOr(StrokeThicknessProperty, 1.0);
}

void Shape::SetFill(
    Base::Ref<Brush> value) noexcept {
    SetValue(FillProperty, std::move(value));
}

void Shape::SetStroke(
    Base::Ref<Brush> value) noexcept {
    SetValue(StrokeProperty, std::move(value));
}

void Shape::SetStrokeThickness(double value) noexcept {
    SetValue(StrokeThicknessProperty, value);
}

double Rectangle::GetRadiusX() const noexcept {
    return GetValueOr(RadiusXProperty, 0.0);
}

double Rectangle::GetRadiusY() const noexcept {
    return GetValueOr(RadiusYProperty, 0.0);
}

void Rectangle::SetRadiusX(double value) noexcept {
    SetValue(RadiusXProperty, value);
}

void Rectangle::SetRadiusY(double value) noexcept {
    SetValue(RadiusYProperty, value);
}

Size Rectangle::MeasureOverride(
    Size) noexcept {
    const double stroke =
        std::max(0.0, GetStrokeThickness());
    return Size{stroke * 2.0, stroke * 2.0};
}

void Rectangle::OnRender(
    DrawingContext& context) noexcept {
    auto& builder = Aero::Internal::DrawingPrivate::Builder(context);
    const Size renderSize = GetRenderSize();
    if (renderSize.width <= 0.0 ||
        renderSize.height <= 0.0) {
        return;
    }

    const Rect bounds{
        0.0, 0.0, renderSize.width, renderSize.height};
    const double radius = std::max(
        0.0, std::min(GetRadiusX(), GetRadiusY()));
    Base::Ref<Brush> fillBrush = GetFill();
    if (fillBrush &&
        fillBrush->RuntimeType() ==
            LinearGradientBrush::StaticTypeId()) {
        auto& gradient =
            *static_cast<LinearGradientBrush*>(
                fillBrush.Get());
        Point start = gradient.GetStartPoint();
        Point end = gradient.GetEndPoint();
        if (gradient.GetMappingMode() ==
            BrushMappingMode::RelativeToBoundingBox) {
            start = {
                bounds.x + start.x * bounds.width,
                bounds.y + start.y * bounds.height};
            end = {
                bounds.x + end.x * bounds.width,
                bounds.y + end.y * bounds.height};
        }
        const double axisX = end.x - start.x;
        const double axisY = end.y - start.y;
        const double axisLengthSquared = std::max(
            axisX * axisX + axisY * axisY, 1.0e-12);
        const bool horizontal =
            std::fabs(axisX) >= std::fabs(axisY);
        constexpr std::uint32_t bandCount = 96U;
        for (std::uint32_t index = 0U;
             index < bandCount; ++index) {
            const double begin =
                static_cast<double>(index) /
                bandCount;
            const double finish =
                static_cast<double>(index + 1U) /
                bandCount;
            const Rect band = horizontal
                ? Rect{
                    begin * bounds.width, 0.0,
                    (finish - begin) *
                        bounds.width + 0.5,
                    bounds.height}
                : Rect{
                    0.0, begin * bounds.height,
                    bounds.width,
                    (finish - begin) *
                        bounds.height + 0.5};
            const double centerX = band.x + band.width * 0.5;
            const double centerY = band.y + band.height * 0.5;
            const Color color = ::Aero::Internal::SampleGradient(
                gradient,
                ((centerX - start.x) * axisX +
                 (centerY - start.y) * axisY) /
                axisLengthSquared);
            Base::Result<void> painted =
                builder.FillRect(band, color);
            if (!painted) {
                return;
            }
        }
    } else if (fillBrush &&
        fillBrush->RuntimeType() ==
            RadialGradientBrush::StaticTypeId()) {
        auto& gradient =
            *static_cast<RadialGradientBrush*>(
                fillBrush.Get());
        constexpr std::uint32_t bandCount = 64U;
        Base::Result<void> painted =
            builder.FillRect(
                bounds, ::Aero::Internal::SampleGradient(
                    gradient, 1.0));
        if (!painted) return;
        for (std::uint32_t index = bandCount;
             index > 0U; --index) {
            const double outer =
                static_cast<double>(index) /
                bandCount;
            const double insetX =
                bounds.width * (1.0 - outer) *
                0.5;
            const double insetY =
                bounds.height * (1.0 - outer) *
                0.5;
            painted = builder.FillRoundedRect(
                {insetX, insetY,
                 bounds.width - insetX * 2.0,
                 bounds.height - insetY * 2.0},
                ::Aero::Internal::SampleGradient(
                    gradient, outer),
                std::min(
                    bounds.width - insetX * 2.0,
                    bounds.height - insetY * 2.0) *
                    0.5);
            if (!painted) {
                return;
            }
        }
    } else if (fillBrush &&
        fillBrush->RuntimeType() ==
            ImageBrush::StaticTypeId()) {
        Base::Result<void> painted =
            PaintImageBrush(
                context,
                *static_cast<ImageBrush*>(
                    fillBrush.Get()),
                bounds);
        if (!painted) {
            return;
        }
    } else {
        const Color fill = ::Aero::Internal::SampleBrush(fillBrush);
        if (fill.alpha > 0.0F) {
        Base::Result<void> painted = radius > 0.0
            ? builder.FillRoundedRect(bounds, fill, radius)
            : builder.FillRect(bounds, fill);
        if (!painted) return;
        }
    }

    const Color stroke = ::Aero::Internal::SampleBrush(GetStroke());
    const double thickness = GetStrokeThickness();
    if (stroke.alpha > 0.0F && thickness > 0.0) {
        static_cast<void>(builder.StrokeRect(bounds, stroke, thickness));
        return;
    }
    return;
}

Size Ellipse::MeasureOverride(
    Size) noexcept {
    const double stroke =
        std::max(0.0, GetStrokeThickness());
    return Size{stroke * 2.0, stroke * 2.0};
}

void Ellipse::OnRender(
    DrawingContext& context) noexcept {
    auto& builder = Aero::Internal::DrawingPrivate::Builder(context);
    const Size renderSize = GetRenderSize();
    if (renderSize.width <= 0.0 ||
        renderSize.height <= 0.0) {
        return;
    }

    const Color fill = ::Aero::Internal::SampleBrush(GetFill());
    if (fill.alpha > 0.0F) {
        Transform2D scale;
        scale.m11 = renderSize.width;
        scale.m22 = renderSize.height;
        Base::Result<void> pushed =
            builder.PushTransform(scale);
        if (!pushed) return;
        Base::Result<void> painted =
            builder.FillRoundedRect(
                Rect{0.0, 0.0, 1.0, 1.0},
                fill, 0.5);
        Base::Result<void> popped =
            builder.PopTransform();
        if (!painted) return;
        if (!popped) return;
    }

    // The retained command model does not yet expose a rounded-stroke
    // primitive. Preserve a deterministic visible outline until ellipse
    // stroke tessellation is added.
    const Color stroke = ::Aero::Internal::SampleBrush(GetStroke());
    const double thickness = GetStrokeThickness();
    if (stroke.alpha > 0.0F && thickness > 0.0) {
        static_cast<void>(builder.StrokeRect(
            Rect{0.0, 0.0, renderSize.width, renderSize.height},
            stroke, thickness));
        return;
    }
    return;
}

} // namespace Aero::Shapes
