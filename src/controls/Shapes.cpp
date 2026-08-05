#include "../render/DisplayList.hpp"
#include <Aero/Shapes.hpp>
#include "../render/RenderPrivate.hpp"

#include "media/MediaPrivate.hpp"

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

double AlignmentFactor(HorizontalAlignment alignment) noexcept {
    switch (alignment) {
    case HorizontalAlignment::Left:
        return 0.0;
    case HorizontalAlignment::Right:
        return 1.0;
    default:
        return 0.5;
    }
}

double AlignmentFactor(VerticalAlignment alignment) noexcept {
    switch (alignment) {
    case VerticalAlignment::Top:
        return 0.0;
    case VerticalAlignment::Bottom:
        return 1.0;
    default:
        return 0.5;
    }
}

ImageBrushGeometry FitImageBrush(
    const ImageBrush& brush,
    Rect destination,
    Rect sourceUv) noexcept {
    const double sourceWidth =
        static_cast<double>(
            Aero::Media::Detail::BrushPrivate::
                PixelWidth(brush)) *
        std::fabs(sourceUv.width);
    const double sourceHeight =
        static_cast<double>(
            Aero::Media::Detail::BrushPrivate::
                PixelHeight(brush)) *
        std::fabs(sourceUv.height);
    if (sourceWidth <= 0.0 ||
        sourceHeight <= 0.0 ||
        destination.width <= 0.0 ||
        destination.height <= 0.0 ||
        brush.GetStretch() == Stretch::Fill) {
        return {destination, sourceUv};
    }
    if (brush.GetStretch() == Stretch::None) {
        destination.x +=
            (destination.width - sourceWidth) *
            AlignmentFactor(brush.GetAlignmentX());
        destination.y +=
            (destination.height - sourceHeight) *
            AlignmentFactor(brush.GetAlignmentY());
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
            AlignmentFactor(brush.GetAlignmentX());
        sourceUv.y +=
            sourceUv.height *
            (1.0 -
             destination.height / drawnHeight) *
            AlignmentFactor(brush.GetAlignmentY());
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
        (destination.width - fittedWidth) *
        AlignmentFactor(brush.GetAlignmentX());
    destination.y +=
        (destination.height - fittedHeight) *
        AlignmentFactor(brush.GetAlignmentY());
    destination.width = fittedWidth;
    destination.height = fittedHeight;
    return {destination, sourceUv};
}

Base::Result<void> PaintImageBrush(
    DrawingContext& context,
    const ImageBrush& brush,
    Rect bounds) noexcept {
    auto& builder = Aero::Render::Detail::DrawingPrivate::Builder(context);
    const RenderImageId image =
        Aero::Media::Detail::BrushPrivate::
            RuntimeImage(brush);
    if (image == InvalidRenderImageId ||
        Aero::Media::Detail::BrushPrivate::
            PixelWidth(brush) == 0U ||
        Aero::Media::Detail::BrushPrivate::
            PixelHeight(brush) == 0U) {
        return {};
    }
    const double pixelWidth = static_cast<double>(
        Aero::Media::Detail::BrushPrivate::PixelWidth(brush));
    const double pixelHeight = static_cast<double>(
        Aero::Media::Detail::BrushPrivate::PixelHeight(brush));
    const Rect authoredViewbox = brush.GetViewbox();
    Rect sourceUv = brush.GetViewboxUnits() ==
            BrushMappingMode::Absolute
        ? Rect{
            authoredViewbox.x / pixelWidth,
            authoredViewbox.y / pixelHeight,
            authoredViewbox.width / pixelWidth,
            authoredViewbox.height / pixelHeight}
        : authoredViewbox;
    sourceUv = {
        std::clamp(sourceUv.x, 0.0, 1.0),
        std::clamp(sourceUv.y, 0.0, 1.0),
        std::clamp(sourceUv.width, 0.0, 1.0),
        std::clamp(sourceUv.height, 0.0, 1.0)};
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
    Rect cell = brush.GetViewportUnits() ==
            BrushMappingMode::Absolute
        ? Rect{
            bounds.x + authoredViewport.x,
            bounds.y + authoredViewport.y,
            authoredViewport.width,
            authoredViewport.height}
        : Rect{
            bounds.x + authoredViewport.x * bounds.width,
            bounds.y + authoredViewport.y * bounds.height,
            authoredViewport.width * bounds.width,
            authoredViewport.height * bounds.height};
    if (cell.width <= 0.0 ||
        cell.height <= 0.0) {
        return {};
    }
    const Color tint{
        1.0F, 1.0F, 1.0F,
        static_cast<float>(brush.GetOpacity())};
    const Base::Ref<Transform> relativeTransform =
        brush.GetRelativeTransform();
    const bool transformed = relativeTransform &&
        bounds.width > 0.0 && bounds.height > 0.0;
    const bool clipped = transformed ||
        brush.GetTileMode() != TileMode::None;
    Base::Result<void> status;
    if (clipped) status = builder.PushClip(bounds);
    if (!status) return status.GetStatus();
    if (transformed) {
        Base::Transform2D toNormalized;
        toNormalized.m11 = 1.0 / bounds.width;
        toNormalized.m22 = 1.0 / bounds.height;
        toNormalized.dx = -bounds.x / bounds.width;
        toNormalized.dy = -bounds.y / bounds.height;
        Base::Transform2D fromNormalized;
        fromNormalized.m11 = bounds.width;
        fromNormalized.m22 = bounds.height;
        fromNormalized.dx = bounds.x;
        fromNormalized.dy = bounds.y;
        const Base::Transform2D absolute = ComposeTransforms(
            ComposeTransforms(
                toNormalized,
                relativeTransform->GetMatrix()),
            fromNormalized);
        status = builder.PushTransform(absolute);
        if (!status) {
            static_cast<void>(builder.PopClip());
            return status.GetStatus();
        }
    }

    if (brush.GetTileMode() == TileMode::None) {
        const ImageBrushGeometry geometry =
            FitImageBrush(brush, cell, sourceUv);
        status = builder.DrawImage(
            image, geometry.destination, geometry.sourceUv, tint);
    } else {
        constexpr std::uint32_t maxTiles = 4096U;
        std::uint32_t tileCount = 0U;
        const std::int64_t firstColumn = static_cast<std::int64_t>(
            std::floor((bounds.x - cell.x) / cell.width));
        const std::int64_t firstRow = static_cast<std::int64_t>(
            std::floor((bounds.y - cell.y) / cell.height));
        for (std::int64_t row = firstRow;
             cell.y + static_cast<double>(row) * cell.height < bounds.y + bounds.height &&
             tileCount < maxTiles && status; ++row) {
            for (std::int64_t column = firstColumn;
                 cell.x + static_cast<double>(column) * cell.width < bounds.x + bounds.width &&
                 tileCount < maxTiles; ++column) {
                Rect tile{
                    cell.x + static_cast<double>(column) * cell.width,
                    cell.y + static_cast<double>(row) * cell.height,
                    cell.width,
                    cell.height};
                ImageBrushGeometry geometry =
                    FitImageBrush(brush, tile, sourceUv);
                const bool oddColumn = (column & 1) != 0;
                const bool oddRow = (row & 1) != 0;
                const TileMode mode = brush.GetTileMode();
                if (oddColumn &&
                    (mode == TileMode::FlipX || mode == TileMode::FlipXY)) {
                    geometry.sourceUv.x += geometry.sourceUv.width;
                    geometry.sourceUv.width = -geometry.sourceUv.width;
                }
                if (oddRow &&
                    (mode == TileMode::FlipY || mode == TileMode::FlipXY)) {
                    geometry.sourceUv.y += geometry.sourceUv.height;
                    geometry.sourceUv.height = -geometry.sourceUv.height;
                }
                status = builder.DrawImage(
                    image, geometry.destination, geometry.sourceUv, tint);
                if (!status) break;
                ++tileCount;
            }
        }
    }
    const Base::Status paintStatus = status
        ? Base::Status::Ok()
        : status.GetStatus();
    if (transformed) {
        Base::Result<void> popped = builder.PopTransform();
        if (status && !popped) status = popped.GetStatus();
    }
    if (clipped) {
        Base::Result<void> popped = builder.PopClip();
        if (status && !popped) status = popped.GetStatus();
    }
    return paintStatus.IsOk() ? status : Base::Result<void>(paintStatus);
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
    auto& builder = Aero::Render::Detail::DrawingPrivate::Builder(context);
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
            const Color color = ::Aero::Media::Detail::SampleGradient(
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
                bounds, ::Aero::Media::Detail::SampleGradient(
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
                ::Aero::Media::Detail::SampleGradient(
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
        const Color fill = ::Aero::Media::Detail::SampleBrush(fillBrush);
        if (fill.alpha > 0.0F) {
        Base::Result<void> painted = radius > 0.0
            ? builder.FillRoundedRect(bounds, fill, radius)
            : builder.FillRect(bounds, fill);
        if (!painted) return;
        }
    }

    const Color stroke = ::Aero::Media::Detail::SampleBrush(GetStroke());
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
    auto& builder = Aero::Render::Detail::DrawingPrivate::Builder(context);
    const Size renderSize = GetRenderSize();
    if (renderSize.width <= 0.0 ||
        renderSize.height <= 0.0) {
        return;
    }

    const Color fill = ::Aero::Media::Detail::SampleBrush(GetFill());
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
    const Color stroke = ::Aero::Media::Detail::SampleBrush(GetStroke());
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
