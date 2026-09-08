// Auto-relocated base-class method definitions (WPF semantic kernel).
#include <Aero/FrameworkElement.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Events.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/Animation/EventTrigger.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Controls.hpp>
#include <cmath>
#include <cstdio>
#include "gui/meta/ElementsFill.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/state/ElementTree.hpp"
#include "gui/core/state/LayoutEngine.hpp"
#include "gui/core/state/FreezableState.hpp"
#include "gui/core/state/EffectiveValueEngine.hpp"
#include "gui/core/state/RoutedEvents.hpp"
#include "gui/core/state/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/input/InputState.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/meta/MetadataState.hpp"
#include "gui/controls/ControlBehavior.hpp"

using namespace Aero;
using namespace Aero::Media;
using namespace Aero::Meta;
using namespace Aero::Threading;

namespace Aero {

FrameworkElement* FrameworkElement::GetRenderParent() const noexcept {
    ::Aero::Media::Visual* parent = GetVisualParent();
    return parent != nullptr
        ? ::Aero::TryCast<FrameworkElement>(parent)
        : nullptr;
}

// from src/gui/controls/Layout.cpp

void FrameworkElement::SetVerticalAlignment(
    VerticalAlignment value) noexcept {
    SetValue(VerticalAlignmentProperty, value);
}

// from src/gui/controls/Layout.cpp

void FrameworkElement::SetHorizontalAlignment(
    HorizontalAlignment value) noexcept {
    SetValue(HorizontalAlignmentProperty, value);
}

// from src/gui/controls/Layout.cpp

void FrameworkElement::SetMargin(Thickness value) noexcept {
    SetValue(MarginProperty, value);
}

// from src/gui/controls/Layout.cpp

void FrameworkElement::SetMaxSize(Size value) noexcept {
    const Size minimum = GetMinSize();
    if (!IsValidLayoutSize(value) || value.width < minimum.width ||
        value.height < minimum.height) {
        return;
    }
    SetValue(MaxWidthProperty, value.width);
    SetValue(MaxHeightProperty, value.height);
}

// from src/gui/controls/Layout.cpp

void FrameworkElement::SetMinSize(Size value) noexcept {
    const Size maximum = GetMaxSize();
    if (!IsValidLayoutSize(value) || value.width > maximum.width ||
        value.height > maximum.height) {
        return;
    }
    SetValue(MinWidthProperty, value.width);
    SetValue(MinHeightProperty, value.height);
}

// from src/gui/controls/Layout.cpp

void FrameworkElement::ClearHeight() noexcept {
    ClearValue(HeightProperty);
}

// from src/gui/controls/Layout.cpp

void FrameworkElement::SetHeight(double value) noexcept {
    SetValue(HeightProperty, Length::Pixels(value));
}

// from src/gui/controls/Layout.cpp

void FrameworkElement::ClearWidth() noexcept {
    ClearValue(WidthProperty);
}

// from src/gui/controls/Layout.cpp





















void FrameworkElement::SetWidth(double value) noexcept {
    SetValue(WidthProperty, Length::Pixels(value));
}

void FrameworkElement::SetFontFamily(StringView value) noexcept {
    Result<Ref<Media::FontFamily>> family =
        Base::MakeRef<Media::FontFamily>();
    if (!family) { AERO_ASSERT(false); return; }
    family.Value()->SetSource(value);
    SetFontFamily(std::move(family).Value());
}

// from src/gui/controls/Layout.cpp
VerticalAlignment FrameworkElement::GetVerticalAlignment() const noexcept {
    return GetValue(VerticalAlignmentProperty);
}

// from src/gui/controls/Layout.cpp
HorizontalAlignment FrameworkElement::GetHorizontalAlignment() const noexcept {
    return GetValue(HorizontalAlignmentProperty);
}

// from src/gui/controls/Layout.cpp
Thickness FrameworkElement::GetMargin() const noexcept {
    return GetValue(MarginProperty);
}

// from src/gui/controls/Layout.cpp
Size FrameworkElement::GetMaxSize() const noexcept {
    const Size minimum = GetMinSize();
    const double authoredWidth =
        GetValue(MaxWidthProperty);
    const double authoredHeight =
        GetValue(MaxHeightProperty);
    // Resolve contradictory template/style ordering at layout time. Min values
    // take precedence without rejecting an otherwise valid WPF template.
    return {
        authoredWidth < minimum.width ? minimum.width : authoredWidth,
        authoredHeight < minimum.height ? minimum.height : authoredHeight};
}

// from src/gui/controls/Layout.cpp
Size FrameworkElement::GetMinSize() const noexcept {
    return {
        GetValue(MinWidthProperty),
        GetValue(MinHeightProperty)};
}

// from src/gui/controls/Layout.cpp
double FrameworkElement::GetHeight() const noexcept {
    const Length length =
        GetValue(HeightProperty);
    return length.isAuto ? 0.0 : length.value;
}

// from src/gui/controls/Layout.cpp
double FrameworkElement::GetWidth() const noexcept {
    const Length length =
        GetValue(WidthProperty);
    return length.isAuto ? 0.0 : length.value;
}

// from src/gui/controls/Layout.cpp
bool FrameworkElement::GetHasHeight() const noexcept {
    return !GetValue(HeightProperty).isAuto;
}

// from src/gui/controls/Layout.cpp
bool FrameworkElement::GetHasWidth() const noexcept {
    return !GetValue(WidthProperty).isAuto;
}

// from src/gui/controls/Layout.cpp
bool FrameworkElement::GetSnapsToDevicePixels() const noexcept {
    return GetValue(SnapsToDevicePixelsProperty);
}

// from src/gui/controls/Layout.cpp


















bool FrameworkElement::GetUseLayoutRounding() const noexcept {
    return GetValue(UseLayoutRoundingProperty);
}

// from src/gui/controls/Layout.cpp








void FrameworkElement::SetUseLayoutRounding(
    bool enabled,
    double dpiScale) noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) return;
    if (!std::isfinite(dpiScale) || dpiScale <= 0.0) {
        return;
    }
    const bool scaleChanged = dpiScale_ != dpiScale;
    dpiScale_ = dpiScale;
    SetValue(UseLayoutRoundingProperty, enabled);
    if (scaleChanged && enabled) (void)InvalidateMeasure();
}

// from src/gui/core/ElementTree.cpp

Base::Object* FrameworkElement::FindNameObject(
    Base::StringView name,
    Meta::TypeId expectedType) noexcept {
    const FrameworkElement* current = this;
    for (std::uint32_t depth = 0U;
         current != nullptr && depth < 256U;
         ++depth) {
        Base::Object* object = current->FindRegisteredName(name);
        if (object != nullptr) {
            if (expectedType == Meta::InvalidTypeId) {
                return object;
            }
            return AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
                object->RuntimeType(), expectedType)
                ? object
                : nullptr;
        }
        // UserControl (LoadComponent ColorSelector) is a namescope. Walking
        // to Window made FindName("LayoutRoot") return the Window grid, then
        // SetContent tried to parent that grid under ColorSelector (cycle).
        if (::Aero::TryCast<Controls::UserControl>(
                const_cast<FrameworkElement*>(current)) != nullptr) {
            break;
        }
        ::Aero::Media::Visual* visual =
            ::Aero::TryCast<::Aero::Media::Visual>(
                const_cast<FrameworkElement*>(current));
        ::Aero::Base::Object* parent = nullptr;
        if (visual != nullptr) {
            parent = visual->GetLogicalParent();
            if (parent == nullptr) {
                parent = visual->GetVisualParent();
            }
        }
        current = ::Aero::TryCast<FrameworkElement>(parent);
    }
    return AeroGuiInternal::FindName(
        *this, name, expectedType);
}

// from src/gui/core/ElementTree.cpp

Base::Object* FrameworkElement::FindName(
    Base::StringView name) noexcept {
    return FindNameObject(name, Meta::InvalidTypeId);
}

namespace {

const ResourceDictionary* TemplateResourcesFor(
    const FrameworkElement& element) noexcept {
    const DependencyObject* templated = element.GetTemplatedParent();
    if (templated == nullptr) {
        return nullptr;
    }
    const auto& control =
        *static_cast<const Controls::Control*>(templated);
    Controls::TemplateEngine* templates =
        AeroGuiInternal::TemplatesOf(control);
    if (templates == nullptr) {
        return nullptr;
    }
    const Controls::TemplateHandle handle =
        templates->AppliedHandle(control);
    if (!handle.IsValid()) {
        return nullptr;
    }
    const Controls::ControlTemplate* plan =
        templates->AppliedTemplate(handle);
    return plan != nullptr ? &plan->GetResources() : nullptr;
}

} // namespace

ResourceDictionary& FrameworkElement::GetResources() noexcept {
    return EnsureOwnedResources(resources_);
}

const ResourceDictionary& FrameworkElement::GetResources() const noexcept {
    return EnsureOwnedResources(resources_);
}

Result<ResourceValue> FrameworkElement::FindResource(
    const ResourceKey& key) const noexcept {
    return ResourceResolver::Lookup(
        this,
        key,
        TemplateResourcesFor(*this),
        AeroGuiInternal::ResourceEnvironmentOf(*this));
}

Result<ResourceValue> FrameworkElement::FindResource(
    StringView key) const noexcept {
    return ResourceResolver::Lookup(
        this,
        key,
        TemplateResourcesFor(*this),
        AeroGuiInternal::ResourceEnvironmentOf(*this));
}

Result<ResourceValue> FrameworkElement::TryFindResource(
    const ResourceKey& key) const noexcept {
    Result<ResourceValue> found = FindResource(key);
    if (found) {
        return found;
    }
    if (found.GetStatus().code == Base::ErrorCode::NotFound) {
        return ResourceValue{};
    }
    return found.GetStatus();
}

Result<ResourceValue> FrameworkElement::TryFindResource(
    StringView key) const noexcept {
    Result<ResourceValue> found = FindResource(key);
    if (found) {
        return found;
    }
    if (found.GetStatus().code == Base::ErrorCode::NotFound) {
        return ResourceValue{};
    }
    return found.GetStatus();
}
FrameworkElement* FrameworkElementChildRange::Iterator::operator*() const noexcept {
    ::Aero::Media::Visual* child = owner_ != nullptr ? ::Aero::Media::VisualTreeHelper::GetChild(*owner_, index_) : nullptr;
    return child != nullptr ? ::Aero::TryCast<::Aero::FrameworkElement>(child) : nullptr;
}

void FrameworkElementChildRange::Iterator::Advance() noexcept {
    if (owner_ == nullptr) return;
    const std::uint32_t count = ::Aero::Media::VisualTreeHelper::GetChildrenCount(*owner_);
    while (index_ < count) {
        ::Aero::Media::Visual* child = ::Aero::Media::VisualTreeHelper::GetChild(*owner_, index_);
        if (child != nullptr && ::Aero::TryCast<::Aero::FrameworkElement>(child) != nullptr) return;
        ++index_;
    }
}

std::uint32_t FrameworkElementChildRange::Size() const noexcept {
    std::uint32_t count = 0U;
    for (FrameworkElement* child : *this) {
        (void)child;
        ++count;
    }
    return count;
}

} // namespace Aero

// ---- Fill helpers (colocated from meta/Support.inl, single TU use) ----
namespace {
constexpr double DefaultMaximum = 1.0e12;

class PlaceholderFrameworkElement : public FrameworkElement {
public:
    PlaceholderFrameworkElement() noexcept
        : FrameworkElement(FrameworkElement::StaticTypeId()) {}
};

bool ValidateLength(const Length& length) noexcept {
    return length.isAuto || (std::isfinite(length.value) && length.value >= 0.0);
}
bool ValidateMarginValue(const Thickness& t) noexcept {
    // WPF permits negative margins for overlap and shared-border layouts.
    return IsFinite(t);
}

void AddFrameworkEventTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<Media::Animation::EventTrigger> retained =
        Base::Ref<Media::Animation::EventTrigger>::TryFromBorrowed(
            static_cast<Media::Animation::EventTrigger&>(*value));
    if (!retained) {
        return;
    }
    static_cast<void>(
        AeroGuiInternal::AddAuthoredTrigger(
            static_cast<FrameworkElement&>(owner),
            Base::Ref<Base::Object>(std::move(retained))));
}

void ClearFrameworkEventTriggers(
    Base::Object& owner,
    void*) noexcept {
    static_cast<void>(
        AeroGuiInternal::ClearAuthoredTriggers(
            static_cast<FrameworkElement&>(owner)));
}

void OnLayoutTransformChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&) noexcept {
}
} // namespace

// ---- Builtin metadata Fill (colocated from meta/Elements.inl) ----
namespace Aero::Meta {
using namespace ::Aero::Threading;
using namespace ::Aero::Input;
using namespace ::Aero::Media;
using namespace ::Aero::Data;
using namespace ::Aero::Media::Animation::Model;

Base::Result<void> FillFrameworkElementMetadata(
    ::Aero::Meta::Registration& context) noexcept {
    Register<FrameworkElement>(context)
        .Event(FrameworkElement::LoadedEvent, RoutingStrategy::Direct)
        .Property<
            Base::Ref<ResourceDictionary>,
            &FrameworkElement::SetResources>(
                "Resources",
                PropertyFlags::Structural)
        .Property(
            FrameworkElement::DataContextProperty,
            Value::NullObject(
                TypeOf<Base::Object>()), Inherits)
        .Property(
            FrameworkElement::FontFamilyProperty,
            Base::Ref<Media::FontFamily>{}, Inherits | AffectsMeasure)
        .Property(
            FrameworkElement::FlowDirectionProperty,
            FlowDirection::LeftToRight, Inherits | AffectsMeasure)
        .Property(
            FrameworkElement::CursorProperty,
            Base::String{}, Inherits)
        .Property(
            FrameworkElement::ForceCursorProperty,
            false)
        .Property(
            FrameworkElement::InputScopeProperty,
            InputScope::Default)
        .Property(
            FrameworkElement::ForegroundProperty,
            Base::Ref<Brush>{}, Inherits | AffectsRender)
        .Property(
            FrameworkElement::StyleProperty,
            Base::Ref<Style>{})
        .Property(
            FrameworkElement::TagProperty,
            Meta::Value::NullObject(
                Meta::TypeOf<Base::Object>()))
        .Property(
            FrameworkElement::ToolTipProperty,
            Meta::Value::NullObject(
                Meta::TypeOf<Base::Object>()))
        .Property(
            FrameworkElement::WidthProperty,
            FrameworkPropertyMetadata(Length::Auto(), AffectsMeasure)
                .Validate(&ValidateLength))
        .Property(
            FrameworkElement::HeightProperty,
            FrameworkPropertyMetadata(Length::Auto(), AffectsMeasure)
                .Validate(&ValidateLength))
        .Property(
            FrameworkElement::ActualWidthProperty,
            0.0)
        .Property(
            FrameworkElement::ActualHeightProperty,
            0.0)
        .Property(
            FrameworkElement::MinWidthProperty,
            FrameworkPropertyMetadata(0.0, AffectsMeasure)
                .Validate(&::Aero::Base::Validate::NonNegative<double>))
        .Property(
            FrameworkElement::MaxWidthProperty,
            FrameworkPropertyMetadata(DefaultMaximum, AffectsMeasure)
                .Validate(&::Aero::Base::Validate::NonNegative<double>))
        .Property(
            FrameworkElement::MinHeightProperty,
            FrameworkPropertyMetadata(0.0, AffectsMeasure)
                .Validate(&::Aero::Base::Validate::NonNegative<double>))
        .Property(
            FrameworkElement::MaxHeightProperty,
            FrameworkPropertyMetadata(DefaultMaximum, AffectsMeasure)
                .Validate(&::Aero::Base::Validate::NonNegative<double>))
        .Property(
            FrameworkElement::MarginProperty,
            FrameworkPropertyMetadata(Thickness{}, AffectsMeasure)
                .Validate(&ValidateMarginValue))
        .Property(
            FrameworkElement::HorizontalAlignmentProperty,
            HorizontalAlignment::Stretch, AffectsArrange)
        .Property(
            FrameworkElement::VerticalAlignmentProperty,
            VerticalAlignment::Stretch, AffectsArrange)
        .Property(
            FrameworkElement::UseLayoutRoundingProperty,
            false, AffectsMeasure)
        .Property(
            FrameworkElement::SnapsToDevicePixelsProperty,
            false, Inherits | AffectsArrange | AffectsRender)
        .Property(
            FrameworkElement::LayoutTransformProperty,
            FrameworkPropertyMetadata(Base::Ref<Transform>{}, AffectsMeasure)
                .Changed(&OnLayoutTransformChanged))
        .Collection<Media::Animation::EventTrigger>(
            "Triggers",
            &AddFrameworkEventTrigger,
            &ClearFrameworkEventTriggers)
        .Factory<PlaceholderFrameworkElement>();
    return {};
}

} // namespace Aero::Meta

