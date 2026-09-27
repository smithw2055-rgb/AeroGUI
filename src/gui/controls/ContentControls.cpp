#include "gui/core/ElementTree.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/media/BrushRendering.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ItemContainerGenerator.hpp>
#include <Aero/Controls/Decorator.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Documents.hpp>
#include "gui/core/TypeRegistryDetail.hpp"
#include "gui/core/ValueConversion.hpp"
#include "ControlsMetadata.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/TryCast.hpp>
#include <Aero/VisualTreeHelper.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero::Controls {

using namespace Primitives;

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Render;



HeaderedContentControl::HeaderedContentControl(
    TypeId runtimeType) noexcept
    : ContentControl(runtimeType) {}

HeaderedContentControl::~HeaderedContentControl() = default;

void HeaderedContentControl::OnHeaderChanged(
    const Value&,
    const Value&) {
    ProjectHeaderContent();
}

void HeaderedContentControl::OnHeaderTemplateChanged(
    const Ref<DataTemplate>&,
    const Ref<DataTemplate>&) {}

void HeaderedContentControl::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    ContentControl::OnPropertyChanged(args);
    const DependencyPropertyHandle prop = args.GetProperty();
    if (prop == HeaderProperty) {
        OnHeaderChanged(args.GetOldValue(), args.GetNewValue());
    } else if (prop == HeaderTemplateProperty) {
        const auto toTemplate = [](const Value& v) -> Ref<DataTemplate> {
            if (v.Kind() == ValueKind::Object && v.AsObject()) {
                if (auto* dt = TryCast<DataTemplate>(v.AsObject().Get())) {
                    return Ref<DataTemplate>::FromBorrowed(*dt);
                }
            }
            return {};
        };
        OnHeaderTemplateChanged(
            toTemplate(args.GetOldValue()),
            toTemplate(args.GetNewValue()));
    }
}

Meta::Value
HeaderedContentControl::GetHeader() const noexcept {
    return GetValue(HeaderProperty);
}

void HeaderedContentControl::SetHeader(
    const Meta::Value& value) noexcept {
    SetValue(HeaderProperty, value);
}

void HeaderedContentControl::SetHeader(
    Base::StringView value) noexcept {
    Base::Result<Value> boxed = Value::TryFromString(
        Meta::TypeOf<Base::String>(), value);
    if (!boxed) { AERO_ASSERT(false); return; }
    SetHeader(std::move(boxed).Value());
}

Base::Ref<DataTemplate>
HeaderedContentControl::GetHeaderTemplate() const noexcept {
    return GetValue(HeaderTemplateProperty);
}

void
HeaderedContentControl::SetHeaderTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    SetValue(
        HeaderTemplateProperty,
        std::move(value));
}

void HeaderedContentControl::OnApplyTemplate() noexcept {
    Control::OnApplyTemplate();
    ProjectHeaderContent();
}

void HeaderedContentControl::ProjectHeaderContent() noexcept {
    const Value header = GetHeader();
    if (header.Kind() != ValueKind::Object ||
        header.IsNullObject() ||
        !header.AsObject()) {
        return;
    }
    Base::Object* obj = header.AsObject().Get();
    if (!DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
            obj->RuntimeType(), UIElement::StaticTypeId())) {
        return;
    }
    auto* element = static_cast<UIElement*>(obj);
    DependencyObject* part = GetTemplateChild(Base::StringView("HeaderHost"));
    if (part == nullptr) {
        part = GetTemplateChild(Base::StringView("PART_Header"));
    }
    auto* presenter =
        part != nullptr ? ::Aero::TryCast<ContentPresenter>(part) : nullptr;
    if (presenter == nullptr) {
        return;
    }
    presenter->HostUiElement(header.AsObject(), *element);
}

Expander::Expander() noexcept
    : HeaderedContentControl(StaticTypeId()),
      headerCheckedHandler_(
          this,
          &Expander::OnHeaderCheckedChanged) {}

Expander::~Expander() {
    UnbindHeaderToggle();
}

bool Expander::GetIsExpanded() const noexcept {
    return GetValue(IsExpandedProperty);
}

void Expander::SetIsExpanded(
    bool value) noexcept {
    const bool old = GetIsExpanded();
    if (old == value) return;
    SetValue(IsExpandedProperty, value);
}

void Expander::OnExpanded() {
    RoutedEventArgs eventArgs;
    RaiseEvent(ExpandedEvent, &eventArgs);
}

void Expander::OnCollapsed() {
    RoutedEventArgs eventArgs;
    RaiseEvent(CollapsedEvent, &eventArgs);
}

void Expander::OnPropertyChanged(
    const DependencyPropertyChangedEventArgs& args) noexcept {
    HeaderedContentControl::OnPropertyChanged(args);
    if (args.GetProperty() == IsExpandedProperty) {
        const bool expanded = args.GetNewValue().AsBoolean();
        if (!synchronizingHeader_ && headerToggle_ != nullptr) {
            const Nullable<bool> isChecked = headerToggle_->GetIsChecked();
            const bool checked =
                isChecked.GetHasValue() ? isChecked.GetValue() : false;
            if (checked != expanded) {
                synchronizingHeader_ = true;
                headerToggle_->SetIsChecked(Nullable<bool>{expanded});
                synchronizingHeader_ = false;
            }
        }
        InvalidateMeasure();
        if (expanded) {
            OnExpanded();
        } else {
            OnCollapsed();
        }
    }
}

void Expander::OnHeaderCheckedChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
    if (synchronizingHeader_ || headerToggle_ == nullptr) {
        return;
    }
    const Nullable<bool> isChecked = headerToggle_->GetIsChecked();
    const bool checked =
        isChecked.GetHasValue() ? isChecked.GetValue() : false;
    if (checked == GetIsExpanded()) {
        return;
    }
    synchronizingHeader_ = true;
    SetIsExpanded(checked);
    synchronizingHeader_ = false;
}

void Expander::UnbindHeaderToggle() noexcept {
    if (headerToggle_ == nullptr) {
        return;
    }
    static_cast<void>(headerToggle_->RemoveValueChangedHandler(
        ToggleButton::IsCheckedProperty,
        headerCheckedHandler_));
    headerToggle_ = nullptr;
}

void Expander::BindHeaderToggle() noexcept {
    UnbindHeaderToggle();
    UIElement* root = GetTemplateRoot();
    if (root == nullptr) {
        return;
    }
    const auto findHeader = [this](auto& self, Aero::Media::Visual& visual)
        -> ToggleButton* {
        if (auto* toggle = TryCast<ToggleButton>(&visual)) {
            if (toggle->GetTemplatedParent() == this) {
                return toggle;
            }
        }
        const std::uint32_t count =
            Aero::Media::VisualTreeHelper::GetChildrenCount(visual);
        for (std::uint32_t index = 0U; index < count; ++index) {
            Aero::Media::Visual* child =
                Aero::Media::VisualTreeHelper::GetChild(visual, index);
            if (child == nullptr) {
                continue;
            }
            if (ToggleButton* found = self(self, *child)) {
                return found;
            }
        }
        return nullptr;
    };
    headerToggle_ = findHeader(findHeader, *root);
    if (headerToggle_ == nullptr) {
        return;
    }
    static_cast<void>(headerToggle_->AddValueChangedHandler(
        ToggleButton::IsCheckedProperty,
        headerCheckedHandler_));
    const Nullable<bool> isChecked = headerToggle_->GetIsChecked();
    const bool checked =
        isChecked.GetHasValue() ? isChecked.GetValue() : false;
    if (checked != GetIsExpanded()) {
        synchronizingHeader_ = true;
        headerToggle_->SetIsChecked(Nullable<bool>{GetIsExpanded()});
        synchronizingHeader_ = false;
    }
}

void Expander::OnApplyTemplate() noexcept {
    HeaderedContentControl::OnApplyTemplate();
    BindHeaderToggle();
}

void Expander::OnTemplateDetached() noexcept {
    UnbindHeaderToggle();
    HeaderedContentControl::OnTemplateDetached();
}

ExpandDirection Expander::GetDirection() const noexcept {
    return GetValue(ExpandDirectionProperty);
}

void Expander::SetDirection(
    ExpandDirection value) noexcept {
    SetValue(
        ExpandDirectionProperty, value);
}

Size Expander::MeasureOverride(
    Size availableSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        return ContentControl::MeasureOverride(
            availableSize);
    }
    constexpr double HeaderExtent = 24.0;
    if (!GetIsExpanded() || GetContentElement() == nullptr) {
        return GetDirection() == ExpandDirection::Left ||
                GetDirection() == ExpandDirection::Right
            ? Size{HeaderExtent, 0.0}
            : Size{0.0, HeaderExtent};
    }
    Size childAvailable = availableSize;
    if (GetDirection() == ExpandDirection::Left ||
        GetDirection() == ExpandDirection::Right) {
        childAvailable.width =
            std::max(0.0, childAvailable.width - HeaderExtent);
    } else {
        childAvailable.height =
            std::max(0.0, childAvailable.height - HeaderExtent);
    }
    Base::Result<void> measured =
        MeasureChild(*GetContentElement(), childAvailable);
    if (!measured) return Size{};
    const Size desired = GetContentElement()->GetDesiredSize();
    return GetDirection() == ExpandDirection::Left ||
            GetDirection() == ExpandDirection::Right
        ? Size{desired.width + HeaderExtent, desired.height}
        : Size{desired.width, desired.height + HeaderExtent};
}

Size Expander::ArrangeOverride(
    Size finalSize) noexcept {
    if (GetTemplateRoot() != nullptr) {
        return ContentControl::ArrangeOverride(
            finalSize);
    }
    if (!GetIsExpanded() || GetContentElement() == nullptr) {
        return finalSize;
    }
    constexpr double HeaderExtent = 24.0;
    Rect slot{0.0, 0.0, finalSize.width, finalSize.height};
    switch (GetDirection()) {
    case ExpandDirection::Down:
        slot.y += HeaderExtent;
        slot.height = std::max(
            0.0, slot.height - HeaderExtent);
        break;
    case ExpandDirection::Up:
        slot.height = std::max(
            0.0, slot.height - HeaderExtent);
        break;
    case ExpandDirection::Right:
        slot.x += HeaderExtent;
        slot.width = std::max(
            0.0, slot.width - HeaderExtent);
        break;
    case ExpandDirection::Left:
        slot.width = std::max(
            0.0, slot.width - HeaderExtent);
        break;
    }
    Base::Result<void> arranged =
        ArrangeChild(*GetContentElement(), slot);
    if (!arranged) return finalSize;
    return finalSize;
}


Stretch Viewbox::GetStretch() const noexcept {
    return GetValue(StretchProperty);
}
StretchDirection
Viewbox::GetStretchDirection() const noexcept {
    return GetValue(StretchDirectionProperty);
}
void Viewbox::SetStretch(
    Stretch value) noexcept {
    SetValue(StretchProperty, value);
}
void Viewbox::SetStretchDirection(
    StretchDirection value) noexcept {
    SetValue(StretchDirectionProperty, value);
}
Size Viewbox::MeasureOverride(
    Size availableSize) noexcept {
    UIElement* child = GetChild();
    if (child == nullptr) {
        if (!LayoutChildren().Empty()) {
            return Size{};
        }
        return Size{};
    }

    // The layout kernel keeps all constraints finite. A large finite measure
    // gives Viewbox content its natural size while preserving that invariant.
    constexpr double NaturalConstraint = 1.0e12;
    Base::Result<void> measured = MeasureChild(
        *child,
        {NaturalConstraint, NaturalConstraint});
    if (!measured) return Size{};

    const Size natural = child->GetDesiredSize();
    if (natural.width <= 0.0 || natural.height <= 0.0) {
        return Size{};
    }

    double scaleX = availableSize.width / natural.width;
    double scaleY = availableSize.height / natural.height;
    switch (GetStretch()) {
    case Stretch::None:
        scaleX = 1.0;
        scaleY = 1.0;
        break;
    case Stretch::Uniform: {
        const double scale = std::min(scaleX, scaleY);
        scaleX = scale;
        scaleY = scale;
        break;
    }
    case Stretch::UniformToFill: {
        const double scale = std::max(scaleX, scaleY);
        scaleX = scale;
        scaleY = scale;
        break;
    }
    case Stretch::Fill:
        break;
    }

    const StretchDirection direction =
        GetStretchDirection();
    if (direction == StretchDirection::UpOnly) {
        scaleX = std::max(1.0, scaleX);
        scaleY = std::max(1.0, scaleY);
    } else if (
        direction == StretchDirection::DownOnly) {
        scaleX = std::min(1.0, scaleX);
        scaleY = std::min(1.0, scaleY);
    }
    return Size{
        natural.width * scaleX,
        natural.height * scaleY};
}
void Viewbox::ApplyViewTransform(
    double scaleX,
    double scaleY,
    double offsetX,
    double offsetY) noexcept {
    UIElement* child = GetChild();
    FrameworkElement* framework = child != nullptr
        ? ::Aero::TryCast<::Aero::FrameworkElement>(child)
        : nullptr;
    auto clearStretch = [](FrameworkElement* element) noexcept {
        if (element == nullptr) return;
        Base::Transform2D leftover{};
        if (!element->TryGetViewboxTransform(leftover)) return;
        element->ClearViewboxTransform();
        static_cast<void>(
            (*element).InvalidateRenderState());
    };
    // Stretch stays on this Viewbox (AeroGUI wrapper Decorator), never on the
    // child: Hexagon grids have ScaleTransform 1.2, Board has RotationY.
    // Putting stretch on those nodes composed it after their own transforms
    // around the unscaled origin, which shifted score digits and collapsed
    // a 90° flip. DropShadow offscreen bakes this matrix in FrameEncoder.
    if (projectedChild_ && projectedChild_.Get() != this) {
        clearStretch(projectedChild_.Get());
    }
    if (child == nullptr) {
        clearStretch(this);
        viewTransform_.Reset();
        projectedChild_.Reset();
        return;
    }
    Base::Transform2D matrix;
    matrix.m11 = scaleX;
    matrix.m22 = scaleY;
    matrix.dx = offsetX;
    matrix.dy = offsetY;
    clearStretch(framework);
    const bool changed = SetViewboxTransform(matrix);
    projectedChild_ = framework != nullptr
        ? Base::Ref<FrameworkElement>::FromBorrowed(*framework)
        : Base::Ref<FrameworkElement>{};
    if (changed) {
        static_cast<void>(
            (*this).InvalidateRenderState());
    }
}
Size Viewbox::ArrangeOverride(
    Size finalSize) noexcept {
    UIElement* child = GetChild();
    if (child == nullptr) {
        ApplyViewTransform(1.0, 1.0, 0.0, 0.0);
        return finalSize;
    }

    const Size natural = child->GetDesiredSize();
    if (natural.width <= 0.0 || natural.height <= 0.0) {
        Base::Result<void> arranged = ArrangeChild(
            *child, {0.0, 0.0, 0.0, 0.0});
        if (!arranged) return finalSize;
        ApplyViewTransform(1.0, 1.0, 0.0, 0.0);
        return finalSize;
    }

    double scaleX = finalSize.width / natural.width;
    double scaleY = finalSize.height / natural.height;
    switch (GetStretch()) {
    case Stretch::None:
        scaleX = 1.0;
        scaleY = 1.0;
        break;
    case Stretch::Uniform: {
        const double scale = std::min(scaleX, scaleY);
        scaleX = scale;
        scaleY = scale;
        break;
    }
    case Stretch::UniformToFill: {
        const double scale = std::max(scaleX, scaleY);
        scaleX = scale;
        scaleY = scale;
        break;
    }
    case Stretch::Fill:
        break;
    }

    const StretchDirection direction =
        GetStretchDirection();
    if (direction == StretchDirection::UpOnly) {
        scaleX = std::max(1.0, scaleX);
        scaleY = std::max(1.0, scaleY);
    } else if (
        direction == StretchDirection::DownOnly) {
        scaleX = std::min(1.0, scaleX);
        scaleY = std::min(1.0, scaleY);
    }

    const double renderedWidth =
        natural.width * scaleX;
    const double renderedHeight =
        natural.height * scaleY;
    const double offsetX =
        (finalSize.width - renderedWidth) * 0.5;
    const double offsetY =
        (finalSize.height - renderedHeight) * 0.5;
    // Child layout stays in unscaled local pixels (Transform3D CenterX/Y,
    // ScaleTransform origin). Stretch lives on this Viewbox. Centering
    // offset is part of that matrix, not the child slot.
    ApplyViewTransform(
        scaleX,
        scaleY,
        offsetX,
        offsetY);
    Base::Result<void> arranged = ArrangeChild(
        *child,
        {0.0, 0.0, natural.width, natural.height});
    if (!arranged) return finalSize;
    // Non-FrameworkElement children compensate RenderTransformOrigin from
    // the arranged RenderSize; re-apply so that origin stays correct.
    ApplyViewTransform(
        scaleX,
        scaleY,
        offsetX,
        offsetY);
    return finalSize;
}
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
void Border::SetBackground(
    Base::Ref<Brush> value) noexcept {
    SetValue(
        BackgroundProperty, std::move(value));
}
void Border::SetBorderBrush(
    Base::Ref<Brush> value) noexcept {
    SetValue(
        BorderBrushProperty, std::move(value));
}
void Border::SetBorderThickness(
    Thickness value) noexcept {
    SetValue(BorderThicknessProperty, value);
}
void Border::SetBorderThickness(
    double value) noexcept {
    SetBorderThickness({value, value, value, value});
}
void Border::SetCornerRadius(
    CornerRadius value) noexcept {
    SetValue(CornerRadiusProperty, value);
}
void Border::SetCornerRadius(
    double value) noexcept {
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
    Base::Result<void> measured = MeasureChild(
        *child, Deflate(availableSize, chrome));
    if (!measured) return Size{};
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
    Base::Result<void> arranged = ArrangeChild(*child,
        {chrome.left, chrome.top,
         childSize.width, childSize.height});
    if (!arranged) return finalSize;
    return finalSize;
}
void Border::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
    const Rect bounds{0.0, 0.0, GetRenderSize().width, GetRenderSize().height};
    if (bounds.width <= 0.0 ||
        bounds.height <= 0.0) {
        return;
    }
    const Base::Ref<Brush> background = GetBackground();
    const bool paintedBackground = background &&
        (background->RuntimeType() == Media::ImageBrush::StaticTypeId() ||
         background->RuntimeType() == Media::LinearGradientBrush::StaticTypeId() ||
         background->RuntimeType() == Media::RadialGradientBrush::StaticTypeId());
    const CornerRadius radii = GetCornerRadius();
    const double radius = std::min(
        std::max(
            std::max(
                radii.topLeft,
                radii.topRight),
            std::max(
                radii.bottomRight,
                radii.bottomLeft)),
        std::min(
            bounds.width,
            bounds.height) * 0.5);
    const Color brush = ::Aero::Media::SampleBrush(GetBorderBrush());
    const Thickness thickness = GetBorderThickness();
    const bool uniform =
        thickness.left == thickness.top &&
        thickness.left == thickness.right &&
        thickness.left == thickness.bottom;
    const double uniformThickness =
        uniform ? thickness.left : 0.0;
    const Color backgroundColor =
        ::Aero::Media::SampleBrush(background);
    if (radius > 0.0 && uniformThickness > 0.0 &&
        brush.alpha > 0.0F && backgroundColor.alpha <= 0.0F) {
        // Filling an outer rounded rectangle and then drawing transparent
        // into its center cannot punch a hole with source-over blending.
        // Preserve transparent rounded-border templates as an outline; the
        // square-corner approximation is preferable to an opaque block.
        static_cast<void>(builder.StrokeRect(
            bounds, brush, uniformThickness, radius));
        return;
    }
    if (radius > 0.0 && uniformThickness > 0.0 &&
        brush.alpha > 0.0F) {
        Base::Result<void> border =
            builder.FillRoundedRect(
                bounds, brush, radius);
        if (!border) return;
        const double inset = std::min(
            uniformThickness,
            std::min(
                bounds.width * 0.5,
                bounds.height * 0.5));
        const Rect inner{
            inset,
            inset,
            std::max(0.0, bounds.width -
                inset * 2.0),
            std::max(0.0, bounds.height -
                inset * 2.0)};
        if (inner.width <= 0.0 ||
            inner.height <= 0.0) {
            return;
        }
        const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
        if (paintedBackground) {
            static_cast<void>(PaintBrushRect(builder, background, inner, 0.0, isRtl));
        } else {
            static_cast<void>(builder.FillRoundedRect(
                inner,
                ::Aero::Media::SampleBrush(background),
                std::min(
                    std::max(0.0, radius - inset),
                    std::min(
                        inner.width,
                        inner.height) * 0.5)));
        }
        return;
    }
    const bool isRtl = GetFlowDirection() == FlowDirection::RightToLeft;
    Base::Result<void> fill = paintedBackground
        ? PaintBrushRect(builder, background, bounds, radius, isRtl)
        : (radius > 0.0
            ? builder.FillRoundedRect(
                  bounds, backgroundColor, radius)
            : builder.FillRect(
                  bounds, backgroundColor));
    if (!fill) return;
    if (uniformThickness > 0.0 &&
        brush.alpha > 0.0F) {
        static_cast<void>(builder.StrokeRect(
            bounds, brush, uniformThickness, radius));
        return;
    }
    if (!uniform && brush.alpha > 0.0F) {
        const auto fillSide =
            [&](Rect side) noexcept -> Base::Result<void> {
                return side.width > 0.0 && side.height > 0.0
                    ? PaintBrushRect(builder, GetBorderBrush(), side)
                    : Base::Result<void>();
            };
        Base::Result<void> side = fillSide({
            0.0, 0.0,
            std::min(bounds.width, thickness.left),
            bounds.height});
        if (!side) return;
        side = fillSide({
            std::max(0.0, bounds.width - thickness.right),
            0.0,
            std::min(bounds.width, thickness.right),
            bounds.height});
        if (!side) return;
        side = fillSide({
            0.0, 0.0,
            bounds.width,
            std::min(bounds.height, thickness.top)});
        if (!side) return;
        static_cast<void>(fillSide({
            0.0,
            std::max(0.0, bounds.height - thickness.bottom),
            bounds.width,
            std::min(bounds.height, thickness.bottom)}));
        return;
    }
    return;
}

namespace {

class BasicControl : public Control {
public:
    BasicControl() noexcept : Control(Control::StaticTypeId()) {}
};


class BasicHeaderedContentControl : public HeaderedContentControl {
public:
    BasicHeaderedContentControl() noexcept
        : HeaderedContentControl(HeaderedContentControl::StaticTypeId()) {}
};

void SetDecoratorContent(
    Base::Object& owner,
    const Base::Ref<Base::Object>& child,
    void*) noexcept {
    if (!child) {
        return;
    }
    (void)(static_cast<Decorator&>(owner)).SetOwnedChild( child, *static_cast<Aero::UIElement*>(child.Get()));
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
    if (decorator.GetBullet() == nullptr) {
        decorator.SetBullet(std::move(retained));
    } else {
        decorator.SetChild(std::move(retained));
    }
}

void ClearBulletDecoratorContent(
    Base::Object& owner,
    void*) noexcept {
    auto& decorator = static_cast<BulletDecorator&>(owner);
    decorator.SetBullet({});
    decorator.SetChild({});
}





bool ValidateCornerRadiusValue(
    const Aero::CornerRadius& radius) noexcept {
    return std::isfinite(radius.topLeft) &&
        std::isfinite(radius.topRight) &&
        std::isfinite(radius.bottomRight) &&
        std::isfinite(radius.bottomLeft) &&
        radius.topLeft >= 0.0 &&
        radius.topRight >= 0.0 &&
        radius.bottomRight >= 0.0 &&
        radius.bottomLeft >= 0.0;
}

} // namespace

AERO_DESCRIBE(Control) {
    using namespace Aero::Meta;
    Register<Control>(context)
        .Event(Control::PreviewMouseDoubleClickEvent, RoutingStrategy::Tunnel)
        .Event(Control::MouseDoubleClickEvent)
        .Override(UIElement::FocusableProperty, true, FrameworkPropertyMetadataOptions::None)
        .Override(UIElement::IsTabStopProperty, true, FrameworkPropertyMetadataOptions::None)
        .Property(Control::BackgroundProperty, Base::Ref<Media::Brush>{}, AffectsRender)
        .Property(Control::BorderBrushProperty, Base::Ref<Media::Brush>{}, AffectsRender)
        .Property(Control::BorderThicknessProperty, Aero::Thickness{}, AffectsMeasure | AffectsRender, &ValidateThicknessValue)
        .Property(Control::PaddingProperty, Aero::Thickness{}, AffectsMeasure, &ValidateThicknessValue)
        .Property(Control::FontWeightProperty, FontWeight::Normal, AffectsMeasure)
        .Property(Control::HorizontalContentAlignmentProperty, Aero::HorizontalAlignment::Left, AffectsArrange)
        .Property(Control::VerticalContentAlignmentProperty, Aero::VerticalAlignment::Top, AffectsArrange)
        .Property(Control::FontSizeProperty, 15.0, Inherits | AffectsMeasure, &ValidatePositiveFiniteDouble)
        .Property(Control::FocusVisualStyleProperty, Base::Ref<Aero::Style>{})
        .Property(Control::OverridesDefaultStyleProperty, false)
        .Property(Control::TemplateProperty, Base::Ref<ControlTemplate>{}, AffectsMeasure)
        .Factory<BasicControl>();
}

class PageContentHost : public ContentControl {
public:
    PageContentHost() noexcept : ContentControl(ContentControl::StaticTypeId()) {}
};

void Page::EnsureHost() noexcept {
    if (host_) return;
    Base::Result<Ref<PageContentHost>> made = Base::MakeRef<PageContentHost>();
    if (!made) return;
    host_ = std::move(made).Value();
    if (host_->GetVisualParent() == nullptr) { AddVisualChild(host_.Get()); }
}

UIElement* Page::GetContent() const noexcept {
    if (!host_) return nullptr;
    const Value value = host_->GetContent();
    if (value.Kind() != ValueKind::Object || !value.AsObject()) return nullptr;
    return TryCast<UIElement>(value.AsObject().Get());
}

Value Page::GetContentValue() const noexcept {
    return host_ ? host_->GetContent() : Value::NullObject(Meta::TypeOf<Base::Object>());
}

void Page::SetContent(UIElement* content) noexcept {
    Result<void> access = VerifyAccess();
    if (!access) return;
    EnsureHost();
    if (!host_) return;
    synchronizingContent_ = true;
    Value propertyValue = content != nullptr
        ? Value::FromObject(content->RuntimeType(), Ref<Base::Object>::FromBorrowed(*content))
        : Value::NullObject(Meta::TypeOf<Base::Object>());
    SetValue(ContentProperty, std::move(propertyValue));
    host_->SetContent(content);
    synchronizingContent_ = false;
    InvalidateMeasure();
}

void Page::SetContent(Ref<UIElement> content) noexcept {
    SetContent(content.Get());
}

void Page::SetContent(StringView text) noexcept {
    Result<void> access = VerifyAccess();
    if (!access) return;
    EnsureHost();
    if (!host_) return;
    synchronizingContent_ = true;
    Result<Value> encoded = Value::TryFromString(Meta::TypeOf<String>(), text);
    if (encoded) SetValue(ContentProperty, std::move(encoded).Value());
    host_->SetContent(text);
    synchronizingContent_ = false;
    InvalidateMeasure();
}

void Page::SetContent(Value value) noexcept {
    Result<void> access = VerifyAccess();
    if (!access) return;
    EnsureHost();
    if (!host_) return;
    synchronizingContent_ = true;
    SetValue(ContentProperty, value);
    host_->SetContent(std::move(value));
    synchronizingContent_ = false;
    InvalidateMeasure();
}

Ref<Base::Object> Page::GetContentTemplate() const noexcept {
    return GetValue(ContentTemplateProperty);
}

void Page::SetContentTemplate(Ref<Base::Object> value) noexcept {
    SetValue(ContentTemplateProperty, std::move(value));
}

void Page::OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept {
    FrameworkElement::OnPropertyChanged(args);
    if (synchronizingContent_) return;
    if (args.GetProperty() == ContentProperty) {
        EnsureHost();
        if (host_) host_->SetContent(args.GetNewValue());
    } else if (args.GetProperty() == ContentTemplateProperty) {
        EnsureHost();
        if (host_) host_->SetContentTemplate(GetContentTemplate());
    }
}

Size Page::MeasureOverride(Size availableSize) noexcept {
    if (!host_) return Size{};
    Result<void> measured = MeasureChild(*host_, availableSize);
    if (!measured) return Size{};
    return host_->GetDesiredSize();
}

Size Page::ArrangeOverride(Size finalSize) noexcept {
    if (!host_) return finalSize;
    Result<void> arranged = ArrangeChild(*host_, {0.0, 0.0, finalSize.width, finalSize.height});
    if (!arranged) return finalSize;
    return finalSize;
}

void SetPageContent(Base::Object& owner, const Base::Ref<Base::Object>& child, void*) noexcept {
    if (!child) return;
    UIElement* element = TryCast<UIElement>(child.Get());
    if (element == nullptr) return;
    Ref<UIElement> retained = Ref<UIElement>::TryFromBorrowed(*element);
    if (!retained) return;
    static_cast<Page&>(owner).SetContent(std::move(retained));
}

void ClearPageContent(Base::Object& owner, void*) noexcept {
    static_cast<Page&>(owner).SetContent(static_cast<UIElement*>(nullptr));
}

AERO_DESCRIBE(Page) {
    using namespace Aero::Meta;
    Register<Page>(context)
        .Property(Page::ContentProperty, Meta::Value::NullObject(Meta::TypeOf<Base::Object>()), AffectsMeasure)
        .Property(Page::ContentTemplateProperty, Base::Ref<Base::Object>{}, AffectsMeasure)
        .Content<UIElement>("Content", ContentKind::Single, &SetPageContent, &ClearPageContent, ContentFlags::Visual)
        .Factory();
}


AERO_DESCRIBE(HeaderedContentControl) {
    using namespace Aero::Meta;
    Register<HeaderedContentControl>(context)
        .Property(HeaderedContentControl::HeaderProperty, Meta::Value::NullObject(Meta::TypeOf<Base::Object>()), AffectsMeasure)
        .Property(HeaderedContentControl::HeaderTemplateProperty, Base::Ref<DataTemplate>{}, AffectsMeasure)
        .Factory<BasicHeaderedContentControl>();
}

AERO_DESCRIBE(Decorator) {
    using namespace Aero::Meta;
    Register<Decorator>(context)
        .Content<Aero::UIElement>("Content", ContentKind::Single, &SetDecoratorContent, &ClearDecoratorContent, ContentFlags::Visual)
        .Factory();
}

AERO_DESCRIBE(BulletDecorator) {
    using namespace Aero::Meta;
    Register<BulletDecorator>(context)
        .Property(BulletDecorator::BackgroundProperty, Base::Ref<Aero::Media::Brush>{}, AffectsRender)
        .Content<Aero::UIElement>("Bullet", ContentKind::Collection, &AddBulletDecoratorContent, &ClearBulletDecoratorContent, ContentFlags::Visual)
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



AERO_DESCRIBE(Expander) {
    using namespace Aero::Meta;
    Register<Expander>(context)
        .Event(Expander::ExpandedEvent)
        .Event(Expander::CollapsedEvent)
        .Property(Expander::IsExpandedProperty, false, AffectsMeasure)
        .Property(Expander::ExpandDirectionProperty, ExpandDirection::Down, AffectsMeasure)
        .Factory();
}





} // namespace Aero

namespace Aero {

void ElementTree::EnsureVisualChildStorage(
    Media::Visual& parent,
    Media::Visual& child) noexcept {
    UIElement* childElement = ::Aero::TryCast<::Aero::UIElement>(&child);
    if (childElement == nullptr) return;
    const Meta::TypeRegistry& types = DependencyObjectAccess::PropertyRegistry((parent)).Types();
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::Panel::StaticTypeId())) {
        auto& panel = static_cast<Controls::Panel&>(parent);
        const std::uint32_t count = (panel).ChildCountCore();
        for (std::uint32_t index = 0U; index < count; ++index) {
            if ((panel).ChildAtCore( index).Get() == childElement) {
                return;
            }
        }
        Base::Ref<Base::Object> borrowed =
            Base::Ref<Base::Object>::FromBorrowed(*childElement);
        (panel).AddChildCore( borrowed, *childElement);
        return;
    }
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::ContentPresenter::StaticTypeId())) {
        auto& presenter = static_cast<Controls::ContentPresenter&>(parent);
        if (presenter.GetContent() == nullptr) {
            presenter.SetContent(childElement);
        }
        return;
    }
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::ContentControl::StaticTypeId())) {
        auto& control = static_cast<Controls::ContentControl&>(parent);
        UIElement* existing =
            (control).GetContentElement();
        if (existing == childElement) {
            return;
        }
        const Base::Ref<Controls::ControlTemplate> templ =
            control.GetValue(Controls::Control::TemplateProperty);
        if (existing == nullptr &&
            (control).GetTemplateRoot() == nullptr &&
            !templ) {
            control.SetContent(childElement);
        }
        return;
    }
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::Decorator::StaticTypeId())) {
        auto& decorator = static_cast<Controls::Decorator&>(parent);
        if (decorator.GetChild() == nullptr) {
            decorator.SetChild(childElement);
        }
        return;
    }
    if (types.IsDerivedFrom(
            parent.RuntimeType(), Controls::BulletDecorator::StaticTypeId())) {
        auto& bullet = static_cast<Controls::BulletDecorator&>(parent);
        if (bullet.GetChild() == childElement ||
            bullet.GetBullet() == childElement) {
            return;
        }
        Base::Ref<UIElement> borrowed =
            Base::Ref<UIElement>::FromBorrowed(*childElement);
        if (bullet.GetChild() == nullptr) {
            bullet.SetChild(std::move(borrowed));
            return;
        }
        if (bullet.GetBullet() == nullptr) {
            bullet.SetBullet(std::move(borrowed));
        }
        return;
    }
    return;
}

void ElementTree::AttachVisualControlTemplateRoot(
    Media::Visual& parent,
    Media::Visual& child) noexcept {
    if (DependencyObjectAccess::PropertyRegistry((parent)).Types().IsDerivedFrom(
            parent.RuntimeType(), Controls::Control::StaticTypeId()) &&
        ::Aero::TryCast<::Aero::UIElement>(&child) != nullptr) {
        auto& control = static_cast<Controls::Control&>(parent);
        const bool isContentControl =
            DependencyObjectAccess::PropertyRegistry((parent)).Types().IsDerivedFrom(
                parent.RuntimeType(),
                Controls::ContentControl::StaticTypeId());
        const bool contentVisual =
            isContentControl &&
            (static_cast<Controls::ContentControl&>(parent)).GetContentElement() ==
                ::Aero::TryCast<::Aero::UIElement>(&child);
        if ((control).GetTemplateRoot() == nullptr &&
            !contentVisual &&
            !isContentControl) {
            (void)(control).SetTemplateChildCore( ::Aero::TryCast<::Aero::UIElement>(&child));
        }
    }
}

void ElementTree::CleanVisualChildStorage(
    Media::Visual& parent,
    Media::Visual& child) noexcept {
    if (UIElement* childElement = ::Aero::TryCast<::Aero::UIElement>(&child)) {
        if (DependencyObjectAccess::PropertyRegistry((parent)).Types().IsDerivedFrom(
                parent.RuntimeType(), Controls::Panel::StaticTypeId())) {
            auto& panel = static_cast<Controls::Panel&>(parent);
            (void)(panel).RemoveChildCore( *childElement);
        }
    }
}

} // namespace Aero
