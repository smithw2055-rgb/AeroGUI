#include "render/DisplayList.hpp"
#include "gui/core/Describe.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/Decorators.hpp>
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/media/BrushRendering.hpp"
#include "ControlPropertyValidators.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Aero::Controls {

using namespace ::Aero::Render;

namespace {

float LimitScale(
    float scale,
    StretchDirection direction) noexcept {
    if (!std::isfinite(scale) || scale < 0.0f) {
        return 0.0f;
    }
    if (direction == StretchDirection::UpOnly) {
        return std::max(1.0f, scale);
    }
    if (direction == StretchDirection::DownOnly) {
        return std::min(1.0f, scale);
    }
    return scale;
}

struct ViewboxScale {
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};

ViewboxScale ComputeViewboxScaleFactor(
    Size availableSize,
    Size natural,
    Stretch stretch,
    StretchDirection direction) noexcept {
    const bool isConstrainedWidth = availableSize.width < 1.0e10f;
    const bool isConstrainedHeight = availableSize.height < 1.0e10f;

    float scaleX = 1.0f;
    float scaleY = 1.0f;

    if ((stretch == Stretch::Uniform || stretch == Stretch::UniformToFill || stretch == Stretch::Fill)
        && (isConstrainedWidth || isConstrainedHeight)) {
        scaleX = (natural.width > 0.0f) ? (availableSize.width / natural.width) : 0.0f;
        scaleY = (natural.height > 0.0f) ? (availableSize.height / natural.height) : 0.0f;

        if (!isConstrainedWidth) {
            scaleX = scaleY;
        } else if (!isConstrainedHeight) {
            scaleY = scaleX;
        } else {
            switch (stretch) {
            case Stretch::Uniform: {
                const float scale = std::min(scaleX, scaleY);
                scaleX = scale;
                scaleY = scale;
                break;
            }
            case Stretch::UniformToFill: {
                const float scale = std::max(scaleX, scaleY);
                scaleX = scale;
                scaleY = scale;
                break;
            }
            case Stretch::Fill:
                break;
            default:
                break;
            }
        }
    }

    if (direction == StretchDirection::UpOnly) {
        scaleX = std::max(1.0f, scaleX);
        scaleY = std::max(1.0f, scaleY);
    } else if (direction == StretchDirection::DownOnly) {
        scaleX = std::min(1.0f, scaleX);
        scaleY = std::min(1.0f, scaleY);
    }

    return {scaleX, scaleY};
}

void SetDecoratorContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) return;
    (void)(static_cast<Decorator&>(owner)).SetOwnedChild(child, *static_cast<Aero::UIElement*>(child.Get()));
}

void ClearDecoratorContent(
    Base::Object& owner,
    void*) noexcept {
    static_cast<Decorator&>(owner).SetChild(nullptr);
}

void AddBulletDecoratorContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) return;
    auto& decorator = static_cast<BulletDecorator&>(owner);
    Base::Ref<UIElement> retained =
        Base::Ref<UIElement>::TryFromBorrowed(
            *static_cast<UIElement*>(child.Get()));
    if (!retained) return;
    if (decorator.GetChild() == nullptr) {
        decorator.SetChild(std::move(retained));
    } else if (decorator.GetBullet() == nullptr) {
        decorator.SetBullet(std::move(retained));
    }
}

void ClearBulletDecoratorContent(
    Base::Object& owner,
    void*) noexcept {
    auto& decorator = static_cast<BulletDecorator&>(owner);
    decorator.SetBullet(nullptr);
    decorator.SetChild(nullptr);
}

inline Thickness Deflate(Thickness value, Thickness delta) noexcept {
    return {
        std::max(0.0f, value.left - delta.left),
        std::max(0.0f, value.top - delta.top),
        std::max(0.0f, value.right - delta.right),
        std::max(0.0f, value.bottom - delta.bottom)};
}

inline Size Deflate(Size value, Thickness delta) noexcept {
    return {
        std::max(0.0f, value.width - delta.left - delta.right),
        std::max(0.0f, value.height - delta.top - delta.bottom)};
}

inline Size Inflate(Size value, Thickness delta) noexcept {
    return {
        value.width + delta.left + delta.right,
        value.height + delta.top + delta.bottom};
}

} // namespace

// ---------------- Image Implementation ----------------

Base::Ref<ImageSource> Image::GetSource() const noexcept {
    return GetValue(SourceProperty);
}

Stretch Image::GetStretch() const noexcept {
    return GetValue(StretchProperty);
}

StretchDirection Image::GetStretchDirection() const noexcept {
    return GetValue(StretchDirectionProperty);
}

void Image::SetSource(Base::Ref<ImageSource> value) noexcept {
    SetValue(SourceProperty, std::move(value));
}

void Image::SetStretch(Stretch value) noexcept {
    SetValue(StretchProperty, value);
}

void Image::SetStretchDirection(StretchDirection value) noexcept {
    SetValue(StretchDirectionProperty, value);
}

void Image::SetRuntimeData(
    std::uint64_t renderImage,
    std::uint32_t pixelWidth,
    std::uint32_t pixelHeight) noexcept {
    const bool measureChanged =
        pixelWidth_ != pixelWidth ||
        pixelHeight_ != pixelHeight;
    const bool renderChanged =
        renderImage_ != renderImage;
    renderImage_ = renderImage;
    pixelWidth_ = pixelWidth;
    pixelHeight_ = pixelHeight;
    if (measureChanged) {
        InvalidateMeasure();
        return;
    }
    if (renderChanged) {
        InvalidateVisual();
    }
}

Size Image::MeasureOverride(Size availableSize) noexcept {
    if (pixelWidth_ == 0U || pixelHeight_ == 0U) {
        return Size{};
    }
    const Size natural{
        static_cast<float>(pixelWidth_),
        static_cast<float>(pixelHeight_)};
    if (GetStretch() == Stretch::None) {
        return natural;
    }
    const float scaleX =
        std::isfinite(availableSize.width) && availableSize.width > 0.0f
        ? availableSize.width / natural.width
        : 1.0f;
    const float scaleY =
        std::isfinite(availableSize.height) && availableSize.height > 0.0f
        ? availableSize.height / natural.height
        : 1.0f;
    float scale = GetStretch() == Stretch::UniformToFill
        ? std::max(scaleX, scaleY)
        : std::min(scaleX, scaleY);
    if (GetStretch() == Stretch::Fill) {
        return Size{
            natural.width * LimitScale(scaleX, GetStretchDirection()),
            natural.height * LimitScale(scaleY, GetStretchDirection())};
    }
    scale = LimitScale(scale, GetStretchDirection());
    return Size{natural.width * scale, natural.height * scale};
}

void Image::OnRender(::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
    if (renderImage_ == InvalidRenderImageId || pixelWidth_ == 0U || pixelHeight_ == 0U) {
        return;
    }
    const Size size = GetRenderSize();
    if (size.width <= 0.0f || size.height <= 0.0f) {
        return;
    }
    const float sourceWidth = static_cast<float>(pixelWidth_);
    const float sourceHeight = static_cast<float>(pixelHeight_);
    const Stretch stretch = GetStretch();
    Rect destination{0.0f, 0.0f, size.width, size.height};
    Rect uv{0.0f, 0.0f, 1.0f, 1.0f};
    if (stretch == Stretch::None) {
        const float scale = LimitScale(1.0f, GetStretchDirection());
        destination.width = sourceWidth * scale;
        destination.height = sourceHeight * scale;
    } else if (stretch != Stretch::Fill) {
        const float scaleX = size.width / sourceWidth;
        const float scaleY = size.height / sourceHeight;
        float scale = stretch == Stretch::UniformToFill
            ? std::max(scaleX, scaleY)
            : std::min(scaleX, scaleY);
        scale = LimitScale(scale, GetStretchDirection());
        const float drawnWidth = sourceWidth * scale;
        const float drawnHeight = sourceHeight * scale;
        if (stretch == Stretch::UniformToFill && drawnWidth > 0.0f && drawnHeight > 0.0f) {
            uv.width = std::clamp(size.width / drawnWidth, 0.0f, 1.0f);
            uv.height = std::clamp(size.height / drawnHeight, 0.0f, 1.0f);
            uv.x = (1.0f - uv.width) * 0.5f;
            uv.y = (1.0f - uv.height) * 0.5f;
        } else {
            destination.width = drawnWidth;
            destination.height = drawnHeight;
        }
    }
    destination.x = (size.width - destination.width) * 0.5f;
    destination.y = (size.height - destination.height) * 0.5f;
    static_cast<void>(builder.DrawImage(renderImage_, destination, uv));
}

// ---------------- Viewbox Implementation ----------------

Stretch Viewbox::GetStretch() const noexcept {
    return GetValue(StretchProperty);
}

StretchDirection Viewbox::GetStretchDirection() const noexcept {
    return GetValue(StretchDirectionProperty);
}

void Viewbox::SetStretch(Stretch value) noexcept {
    SetValue(StretchProperty, value);
}

void Viewbox::SetStretchDirection(StretchDirection value) noexcept {
    SetValue(StretchDirectionProperty, value);
}

Size Viewbox::MeasureOverride(Size availableSize) noexcept {
    UIElement* child = GetChild();
    if (child == nullptr) {
        return Size{};
    }
    constexpr float NaturalConstraint = 1.0e12f;
    MeasureChild(*child, {NaturalConstraint, NaturalConstraint});
    const Size natural = child->GetDesiredSize();
    if (natural.width <= 0.0f || natural.height <= 0.0f) {
        return Size{};
    }
    const ViewboxScale factors = ComputeViewboxScaleFactor(
        availableSize, natural, GetStretch(), GetStretchDirection());
    return Size{natural.width * factors.scaleX, natural.height * factors.scaleY};
}

void Viewbox::ApplyViewTransform(
    float scaleX,
    float scaleY,
    float offsetX,
    float offsetY) noexcept {
    UIElement* child = GetChild();
    if (child == nullptr) {
        ClearViewboxTransform();
        viewTransform_.Reset();
        projectedChild_.Reset();
        return;
    }
    Base::Transform2D matrix;
    matrix.m11 = scaleX;
    matrix.m22 = scaleY;
    matrix.dx = offsetX;
    matrix.dy = offsetY;
    const bool changed = SetViewboxTransform(matrix);
    if (changed) {
        (*this).InvalidateRenderState();
    }
}

Size Viewbox::ArrangeOverride(Size finalSize) noexcept {
    UIElement* child = GetChild();
    if (child == nullptr) {
        ApplyViewTransform(1.0f, 1.0f, 0.0f, 0.0f);
        return finalSize;
    }

    const Size natural = child->GetDesiredSize();
    if (natural.width <= 0.0f || natural.height <= 0.0f) {
        ArrangeChild(*child, {0.0f, 0.0f, 0.0f, 0.0f});
        ApplyViewTransform(1.0f, 1.0f, 0.0f, 0.0f);
        return finalSize;
    }

    const ViewboxScale factors = ComputeViewboxScaleFactor(
        finalSize, natural, GetStretch(), GetStretchDirection());
    const float scaleX = factors.scaleX;
    const float scaleY = factors.scaleY;

    const float renderedWidth = natural.width * scaleX;
    const float renderedHeight = natural.height * scaleY;
    const float arrangeWidth = finalSize.width < 1.0e10f ? finalSize.width : renderedWidth;
    const float arrangeHeight = finalSize.height < 1.0e10f ? finalSize.height : renderedHeight;
    const float offsetX = (arrangeWidth - renderedWidth) * 0.5f;
    const float offsetY = (arrangeHeight - renderedHeight) * 0.5f;

    ApplyViewTransform(scaleX, scaleY, offsetX, offsetY);
    ArrangeChild(*child, {0.0f, 0.0f, natural.width, natural.height});
    return finalSize;
}

// ---------------- Border Implementation ----------------

Border::Border() noexcept : Decorator(StaticTypeId()) {}

Base::Ref<Brush> Border::GetBackground() const noexcept {
    return GetValue(BackgroundProperty);
}

Base::Ref<Brush> Border::GetBorderBrush() const noexcept {
    return GetValue(BorderBrushProperty);
}

Thickness Border::GetBorderThickness() const noexcept {
    return GetValue(BorderThicknessProperty);
}

CornerRadius Border::GetCornerRadius() const noexcept {
    return GetValue(CornerRadiusProperty);
}

Thickness Border::GetPadding() const noexcept {
    return GetValue(PaddingProperty);
}

void Border::SetBackground(Base::Ref<Brush> value) noexcept {
    SetValue(BackgroundProperty, std::move(value));
}

void Border::SetBorderBrush(Base::Ref<Brush> value) noexcept {
    SetValue(BorderBrushProperty, std::move(value));
}

void Border::SetBorderThickness(Thickness value) noexcept {
    SetValue(BorderThicknessProperty, value);
}

void Border::SetBorderThickness(double value) noexcept {
    SetBorderThickness({value, value, value, value});
}

void Border::SetCornerRadius(CornerRadius value) noexcept {
    SetValue(CornerRadiusProperty, value);
}

void Border::SetCornerRadius(double value) noexcept {
    SetCornerRadius({value, value, value, value});
}

void Border::SetPadding(Thickness value) noexcept {
    SetValue(PaddingProperty, value);
}

Size Border::MeasureOverride(Size availableSize) noexcept {
    const Thickness border = GetBorderThickness();
    const Thickness padding = GetPadding();
    const Thickness chrome{
        border.left + padding.left,
        border.top + padding.top,
        border.right + padding.right,
        border.bottom + padding.bottom};
    UIElement* child = GetChild();
    if (child == nullptr) return Inflate({}, chrome);
    MeasureChild(*child, Deflate(availableSize, chrome));
    return Inflate(child->GetDesiredSize(), chrome);
}

Size Border::ArrangeOverride(Size finalSize) noexcept {
    UIElement* child = GetChild();
    if (child == nullptr) return finalSize;
    const Thickness border = GetBorderThickness();
    const Thickness padding = GetPadding();
    const Thickness chrome{
        border.left + padding.left,
        border.top + padding.top,
        border.right + padding.right,
        border.bottom + padding.bottom};
    const Size childSize = Deflate(finalSize, chrome);
    ArrangeChild(*child, {chrome.left, chrome.top, childSize.width, childSize.height});
    return finalSize;
}

void Border::OnRender(::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
    const Rect bounds{0.0, 0.0, GetRenderSize().width, GetRenderSize().height};
    if (bounds.width <= 0.0 || bounds.height <= 0.0) {
        return;
    }
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    const Base::Ref<Brush> background = GetBackground();
    const bool paintedBackground = background &&
        (background->RuntimeType() == Media::ImageBrush::StaticTypeId() ||
         background->RuntimeType() == Media::LinearGradientBrush::StaticTypeId() ||
         background->RuntimeType() == Media::RadialGradientBrush::StaticTypeId());
    const Base::Ref<Brush> borderBrush = GetBorderBrush();
    const bool paintedBorder = borderBrush &&
        (borderBrush->RuntimeType() == Media::ImageBrush::StaticTypeId() ||
         borderBrush->RuntimeType() == Media::LinearGradientBrush::StaticTypeId() ||
         borderBrush->RuntimeType() == Media::RadialGradientBrush::StaticTypeId());
    const CornerRadius radii = GetCornerRadius();
    const float radius = std::min(
        std::max(
            std::max(radii.topLeft, radii.topRight),
            std::max(radii.bottomRight, radii.bottomLeft)),
        std::min(bounds.width, bounds.height) * 0.5f);
    const Color brush = ::Aero::Media::SampleBrush(borderBrush);
    const Thickness thickness = GetBorderThickness();
    const bool uniform =
        thickness.left == thickness.top &&
        thickness.left == thickness.right &&
        thickness.left == thickness.bottom;
    const float uniformThickness =
        uniform ? thickness.left : 0.0f;
    const Color backgroundColor =
        ::Aero::Media::SampleBrush(background);
    const bool hasBorder = brush.alpha > 0.0F || paintedBorder;
    const bool hasBackground = paintedBackground || backgroundColor.alpha > 0.0F;

    if (uniformThickness > 0.0f && hasBorder && !hasBackground) {
        static_cast<void>(builder.StrokeRect(
            bounds, brush, uniformThickness, radius));
        return;
    }
    if (uniformThickness > 0.0f && hasBorder && hasBackground) {
        if (paintedBorder) {
            PaintBrushRect(builder, borderBrush, bounds, radius, isRtl);
        } else if (radius > 0.0f) {
            builder.FillRoundedRect(bounds, brush, radius);
        } else {
            builder.FillRect(bounds, brush);
        }
        const float inset = std::min(
            uniformThickness,
            std::min(bounds.width * 0.5f, bounds.height * 0.5f));
        const Rect inner{
            inset, inset,
            std::max(0.0f, bounds.width - inset * 2.0f),
            std::max(0.0f, bounds.height - inset * 2.0f)};
        if (inner.width <= 0.0f || inner.height <= 0.0f) {
            return;
        }
        const float innerRadius = std::min(
            std::max(0.0f, radius - inset),
            std::min(inner.width, inner.height) * 0.5f);
        if (paintedBackground) {
            PaintBrushRect(builder, background, inner, innerRadius, isRtl);
        } else if (innerRadius > 0.0f) {
            builder.FillRoundedRect(
                inner, backgroundColor, innerRadius);
        } else {
            builder.FillRect(inner, backgroundColor);
        }
        return;
    }
    if (hasBackground) {
        if (paintedBackground) {
            PaintBrushRect(builder, background, bounds, radius, isRtl);
        } else if (radius > 0.0f) {
            builder.FillRoundedRect(bounds, backgroundColor, radius);
        } else {
            builder.FillRect(bounds, backgroundColor);
        }
    }
    if (!uniform && hasBorder) {
        const auto fillSide = [&](Rect side) noexcept {
            if (side.width > 0.0f && side.height > 0.0f) {
                PaintBrushRect(builder, borderBrush, side, 0.0f, isRtl);
            }
        };
        fillSide({
            0.0f, 0.0f,
            std::min(bounds.width, thickness.left),
            bounds.height});
        fillSide({
            std::max(0.0f, bounds.width - thickness.right),
            0.0f,
            std::min(bounds.width, thickness.right),
            bounds.height});
        fillSide({
            0.0f, 0.0f,
            bounds.width,
            std::min(bounds.height, thickness.top)});
        fillSide({
            0.0f,
            std::max(0.0f, bounds.height - thickness.bottom),
            bounds.width,
            std::min(bounds.height, thickness.bottom)});
        return;
    }
}

// ---------------- Describe Registrations ----------------

AERO_DESCRIBE(Decorator) {
    using namespace Aero::Meta;
    Register<Decorator>(context)
        .Content<Aero::UIElement>("Content", ContentKind::Single, &SetDecoratorContent, &ClearDecoratorContent, ContentFlags::Visual)
        .Factory();
}

Base::Ref<UIElement> GetBulletDecoratorBullet(const BulletDecorator& decorator) noexcept {
    return decorator.GetBullet() != nullptr ? Base::Ref<UIElement>::FromBorrowed(*decorator.GetBullet()) : Base::Ref<UIElement>{};
}

void SetBulletDecoratorBullet(BulletDecorator& decorator, Base::Ref<UIElement> value) noexcept {
    decorator.SetBullet(std::move(value));
}

AERO_DESCRIBE(BulletDecorator) {
    using namespace Aero::Meta;
    Register<BulletDecorator>(context)
        .Property(BulletDecorator::BackgroundProperty, Base::Ref<Aero::Media::Brush>{}, AffectsRender)
        .Property<Base::Ref<UIElement>, &GetBulletDecoratorBullet, &SetBulletDecoratorBullet>("Bullet", PropertyFlags::Structural)
        .Content<Aero::UIElement>("Child", ContentKind::Single, &AddBulletDecoratorContent, &ClearBulletDecoratorContent, ContentFlags::Visual)
        .Factory();
}

AERO_DESCRIBE(Viewbox) {
    using namespace Aero::Meta;
    Register<Viewbox>(context)
        .Property(Viewbox::StretchProperty, Stretch::Uniform, AffectsMeasure)
        .Property(Viewbox::StretchDirectionProperty, StretchDirection::Both, AffectsMeasure)
        .Content<Aero::UIElement>("Content", ContentKind::Single, &SetDecoratorContent, &ClearDecoratorContent, ContentFlags::Visual)
        .Factory();
}

AERO_DESCRIBE(Border) {
    using namespace Aero::Meta;
    Register<Border>(context)
        .Property(Border::BackgroundProperty, Base::Ref<Media::Brush>{}, AffectsRender)
        .Property(Border::BorderBrushProperty, Base::Ref<Media::Brush>{}, AffectsRender)
        .Property(Border::BorderThicknessProperty, Aero::Thickness{}, AffectsMeasure | AffectsRender, &ValidateThicknessValue)
        .Property(Border::CornerRadiusProperty, Aero::CornerRadius{}, AffectsRender, &ValidateCornerRadiusValue)
        .Property(Border::PaddingProperty, Aero::Thickness{}, AffectsMeasure, &ValidateThicknessValue)
        .Factory();
}

AERO_DESCRIBE(Image) {
    using namespace Aero::Meta;
    Register<Image>(context)
        .Property(Image::SourceProperty, Base::Ref<Media::ImageSource>{}, AffectsMeasure | AffectsRender)
        .Property(Image::StretchProperty, Media::Stretch::Uniform, AffectsMeasure | AffectsRender)
        .Property(Image::StretchDirectionProperty, Media::StretchDirection::Both, AffectsMeasure | AffectsRender)
        .Factory();
}

} // namespace Aero::Controls
