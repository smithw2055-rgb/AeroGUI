// Auto-relocated base-class method definitions (WPF semantic kernel).
#include <Aero/FrameworkElement.hpp>
#include "gui/core/FrameworkElementSeams.hpp"
#include <Aero/Data/BindingOperations.hpp>
#include "gui/styles/StyleEngine.hpp"
#include "gui/styles/FrameworkResourceResolver.hpp"
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Events.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Controls.hpp>
#include <cmath>
#include <cstdio>
#include "gui/core/ValueConversion.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/input/InputManager.hpp"
#include "gui/templates/TemplateInstance.hpp"
#include "gui/media/AnimationModel.hpp"
#include "gui/core/DependencyObjectAccess.hpp"


using namespace Aero;
using namespace Aero::Media;
using namespace Aero::Meta;
using namespace Aero::Threading;

namespace Aero {

Result<Data::BindingExpression> FrameworkElement::SetBinding(
    DependencyPropertyHandle property,
    const Data::Binding& binding) noexcept {
    return Data::BindingOperations::SetBinding(this, property, binding);
}

Result<Data::BindingExpression> FrameworkElement::SetBinding(
    DependencyPropertyHandle property,
    StringView path) noexcept {
    Data::Binding binding(path);
    return Data::BindingOperations::SetBinding(this, property, binding);
}

void FrameworkElement::ClearBinding(DependencyPropertyHandle property) noexcept {
    Data::BindingOperations::ClearBinding(this, property);
}

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
    return layoutScalars_.margin;
}

// from src/gui/controls/Layout.cpp
Size FrameworkElement::GetMaxSize() const noexcept {
    const Size minimum = GetMinSize();
    const double authoredWidth = layoutScalars_.maxWidth;
    const double authoredHeight = layoutScalars_.maxHeight;
    // Resolve contradictory template/style ordering at layout time. Min values
    // take precedence without rejecting an otherwise valid WPF template.
    return {
        authoredWidth < minimum.width ? minimum.width : authoredWidth,
        authoredHeight < minimum.height ? minimum.height : authoredHeight};
}

// from src/gui/controls/Layout.cpp
Size FrameworkElement::GetMinSize() const noexcept {
    return {layoutScalars_.minWidth, layoutScalars_.minHeight};
}

// from src/gui/controls/Layout.cpp
double FrameworkElement::GetHeight() const noexcept {
    return layoutScalars_.height.isAuto ? 0.0 : layoutScalars_.height.value;
}

// from src/gui/controls/Layout.cpp
double FrameworkElement::GetWidth() const noexcept {
    return layoutScalars_.width.isAuto ? 0.0 : layoutScalars_.width.value;
}

// from src/gui/controls/Layout.cpp
bool FrameworkElement::GetHasHeight() const noexcept {
    return !layoutScalars_.height.isAuto;
}

// from src/gui/controls/Layout.cpp
bool FrameworkElement::GetHasWidth() const noexcept {
    return !layoutScalars_.width.isAuto;
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
    if (scaleChanged && enabled) InvalidateMeasure();
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
            return DependencyObjectAccess::PropertyRegistry((*this)).Types().IsDerivedFrom(
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
    return ElementTree::FindInTree(
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
        ElementTree::TemplatesOf(control);
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
    return FrameworkResourceResolver::Lookup(
        this,
        key,
        TemplateResourcesFor(*this),
        ElementTree::EnvironmentOf(*this));
}

Result<ResourceValue> FrameworkElement::FindResource(
    StringView key) const noexcept {
    return FrameworkResourceResolver::Lookup(
        this,
        key,
        TemplateResourcesFor(*this),
        ElementTree::EnvironmentOf(*this));
}

ResourceValue FrameworkElement::TryFindResource(
    const ResourceKey& key) const noexcept {
    Result<ResourceValue> found = FindResource(key);
    return found ? found.Value() : ResourceValue{};
}

ResourceValue FrameworkElement::TryFindResource(
    StringView key) const noexcept {
    Result<ResourceValue> found = FindResource(key);
    return found ? found.Value() : ResourceValue{};
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

// ---- Length metadata (Describe lives with the layout value type consumers) ----
namespace Aero::MetadataSupport {
using namespace ::Aero::Media;
namespace {

Base::Result<Length> ConvertLength(
    Base::StringView text) noexcept {
    const Base::StringView value = ::Aero::Base::ValueConversion::Trim(text);
    Length length = Length::Auto();
    if (!::Aero::Base::ValueConversion::EqualsAsciiInsensitive(value, "auto")) {
        Base::Result<double> parsed =
            ::Aero::Base::ValueConversion::ParseDouble(value);
        if (!parsed || parsed.Value() < 0.0) {
            return Base::Status::Failure(Base::ErrorCode::ValidationFailed,
                "Length must be Auto or a nonnegative number");
        }
        length = Length::Pixels(parsed.Value());
    }
    return length;
}

bool EqualLength(const void* left, const void* right, void*) noexcept {
    const Length& a = *static_cast<const Length*>(left);
    const Length& b = *static_cast<const Length*>(right);
    return a.isAuto == b.isAuto && (a.isAuto || a.value == b.value);
}

} // namespace
} // namespace Aero::MetadataSupport

namespace Aero {

AERO_DESCRIBE(Length) {
    using namespace Aero::Meta;
    Register<Length>(context)
            .ValueSemantics({sizeof(Length), alignof(Length), nullptr, nullptr, &::Aero::MetadataSupport::EqualLength, nullptr, true})
            .TextConverter<&::Aero::MetadataSupport::ConvertLength>();
}

} // namespace Aero
