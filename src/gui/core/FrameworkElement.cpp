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
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Controls.hpp>
#include <cstdio>
#include "gui/core/State.hpp" 
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

Result<void> FrameworkElement::SetFontFamily(StringView value) noexcept {
    Result<Ref<Media::FontFamily>> family =
        Base::MakeRef<Media::FontFamily>();
    if (!family) return family.GetStatus();
    family.Value()->SetSource(value);
    SetFontFamily(std::move(family).Value());
    return {};
}

// from src/gui/controls/Layout.cpp
VerticalAlignment FrameworkElement::GetVerticalAlignment() const noexcept {
    return GetValueOr(
        VerticalAlignmentProperty,
        VerticalAlignment::Stretch);
}

// from src/gui/controls/Layout.cpp
HorizontalAlignment FrameworkElement::GetHorizontalAlignment() const noexcept {
    return GetValueOr(
        HorizontalAlignmentProperty,
        HorizontalAlignment::Stretch);
}

// from src/gui/controls/Layout.cpp
Thickness FrameworkElement::GetMargin() const noexcept {
    return GetValueOr(MarginProperty, Thickness{});
}

// from src/gui/controls/Layout.cpp
Size FrameworkElement::GetMaxSize() const noexcept {
    const Size minimum = GetMinSize();
    const double authoredWidth =
        GetValueOr(MaxWidthProperty, 1.0e12);
    const double authoredHeight =
        GetValueOr(MaxHeightProperty, 1.0e12);
    // Resolve contradictory template/style ordering at layout time. Min values
    // take precedence without rejecting an otherwise valid WPF template.
    return {
        authoredWidth < minimum.width ? minimum.width : authoredWidth,
        authoredHeight < minimum.height ? minimum.height : authoredHeight};
}

// from src/gui/controls/Layout.cpp
Size FrameworkElement::GetMinSize() const noexcept {
    return {
        GetValueOr(MinWidthProperty, 0.0),
        GetValueOr(MinHeightProperty, 0.0)};
}

// from src/gui/controls/Layout.cpp
double FrameworkElement::GetHeight() const noexcept {
    const Length length =
        GetValueOr(HeightProperty, Length::Auto());
    return length.isAuto ? 0.0 : length.value;
}

// from src/gui/controls/Layout.cpp
double FrameworkElement::GetWidth() const noexcept {
    const Length length =
        GetValueOr(WidthProperty, Length::Auto());
    return length.isAuto ? 0.0 : length.value;
}

// from src/gui/controls/Layout.cpp
bool FrameworkElement::GetHasHeight() const noexcept {
    return !GetValueOr(
        HeightProperty, Length::Auto()).isAuto;
}

// from src/gui/controls/Layout.cpp
bool FrameworkElement::GetHasWidth() const noexcept {
    return !GetValueOr(
        WidthProperty, Length::Auto()).isAuto;
}

// from src/gui/controls/Layout.cpp
bool FrameworkElement::GetSnapsToDevicePixels() const noexcept {
    return GetValueOr(SnapsToDevicePixelsProperty, false);
}

// from src/gui/controls/Layout.cpp


















bool FrameworkElement::GetUseLayoutRounding() const noexcept {
    return GetValueOr(UseLayoutRoundingProperty, false);
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
    Base::Object* object = FindRegisteredName(name);
    if (object != nullptr) {
        if (expectedType == Meta::InvalidTypeId) {
            return object;
        }
        return PropertyRegistry().Types().IsDerivedFrom(
            object->RuntimeType(), expectedType)
            ? object
            : nullptr;
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

} // namespace Aero {
