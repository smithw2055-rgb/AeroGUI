#include "gui/meta/TypeRegistryDetail.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/ListBox.hpp>
#include <Aero/Controls/TreeView.hpp>
#include <Aero/Shapes.hpp>
#include <Aero/Media/Transforms.hpp>
#include "gui/media/BrushRendering.hpp"
#include <Aero/Collections.hpp>
#include <Aero/Documents.hpp>
#include <Aero/TryCast.hpp>
#include "RichText.hpp"
#include "gui/meta/ValueConversion.hpp"


#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace Aero {

void AeroGuiInternal::PathInvalidateGeometry(
    Shapes::Path& path) noexcept {
    path.ResetGeometry();
}

void AeroGuiInternal::PathAttachMeshResources(
    Shapes::Path& path,
    void* services,
    bool invalidate) noexcept {
    path.AttachMeshResources(services, invalidate);
    if (invalidate) {
        path.InvalidateVisual();
    }
}

void AeroGuiInternal::SetMenuItemHighlighted(
    Controls::MenuItem& item,
    bool value) noexcept {
    item.SetHighlightedState(value);
}

void AeroGuiInternal::SyncSelectorContainers(
    Controls::Primitives::Selector& selector) noexcept {
    selector.SyncContainers();
}

std::uint32_t AeroGuiInternal::TreeViewItemCount(
    const Controls::TreeViewItem& item) noexcept {
    return item.GetCount();
}

void AeroGuiInternal::SetItemsSource(
    Controls::ItemsControl& control,
    Collections::IItemsSource* source) noexcept {
    control.SetItemsSourceCore(source);
}

void AeroGuiInternal::SetItemsSource(
    Controls::ItemsControl& control,
    Base::Ref<Base::Object> source) noexcept {
    Collections::IItemsSource* directSource =
        TryCastToInterface<Collections::IItemsSource>(source.Get());
    if (directSource == nullptr) {
        directSource = Collections::CollectionAsItemsSource(source.Get());
    }
    control.SetItemsSourceCore(directSource);
}

void AeroGuiInternal::SetItemsSourceBorrowed(
    Controls::ItemsControl& control,
    Collections::IItemsSource* source) noexcept {
    control.SetValue(
        Controls::ItemsControl::ItemsSourceProperty,
        Base::Ref<Base::Object>{});
    control.SetItemsSourceCore(source);
}

void AeroGuiInternal::SetItemTemplate(
    Controls::ItemsControl& control,
    const DataTemplate* value) noexcept {
    control.SetItemTemplateCore(value);
}

void AeroGuiInternal::SetItemTemplateSelector(
    Controls::ItemsControl& control,
    const DataTemplateSelector* value) noexcept {
    control.SetItemTemplateSelectorCore(value);
}

void AeroGuiInternal::SetItemsPanel(
    Controls::ItemsControl& control,
    const Controls::ItemsPanelTemplate* value) noexcept {
    control.SetItemsPanelCore(value);
}

void AeroGuiInternal::SetItemContainerStyle(
    Controls::ItemsControl& control,
    const Style* value) noexcept {
    control.SetItemContainerStyleCore(value);
}

void AeroGuiInternal::RefreshDisplayMemberPath(
    Controls::ItemsControl& control) noexcept {
    control.PublishReset();
}

void AeroGuiInternal::AttachTextLayout(
    Controls::TextBlock& element,
    void* /*service*/,
    bool invalidate) noexcept {
    if (invalidate) {
        element.InvalidateMeasure();
    }
}

void AeroGuiInternal::AttachTextLayout(
    Controls::TextBox& element,
    void* /*service*/,
    bool invalidate) noexcept {
    if (invalidate) {
        element.InvalidateMeasure();
    }
}

void AeroGuiInternal::AttachTextLayout(
    Controls::PasswordBox& element,
    void* /*service*/,
    bool invalidate) noexcept {
    if (invalidate) {
        element.InvalidateMeasure();
    }
}

Base::Result<GridLength> AeroGuiInternal::ConvertGridLength(
    Base::StringView text) noexcept {
    const Base::StringView value =
        ::Aero::Base::ValueConversion::Trim(text);
    if (::Aero::Base::ValueConversion::EqualsAsciiInsensitive(
            value, "auto")) {
        return GridLength::Auto();
    }
    if (!value.Empty() &&
        value[value.SizeBytes() - 1U] == '*') {
        const Base::StringView weightText =
            value.Substr(0U, value.SizeBytes() - 1U);
        double weight = 1.0;
        if (!weightText.Empty()) {
            Base::Result<double> parsed =
                ::Aero::Base::ValueConversion::ParseDouble(weightText);
            if (!parsed) return parsed.GetStatus();
            weight = parsed.Value();
        }
        if (!std::isfinite(weight) || weight <= 0.0) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "GridLength star weight must be positive and finite");
        }
        return GridLength::Star(weight);
    }
    Base::Result<double> pixels =
        ::Aero::Base::ValueConversion::ParseDouble(value);
    if (!pixels || pixels.Value() < 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "GridLength must be Auto, a nonnegative pixel value, or a star weight");
    }
    return GridLength::Pixel(pixels.Value());
}

Base::Result<void> AeroGuiInternal::ParseGridDefinitions(
    Base::StringView text,
    Base::Vector<GridLength>& output) noexcept {
    output.Clear();
    const Base::StringView value =
        ::Aero::Base::ValueConversion::Trim(text);
    if (value.Empty()) return {};
    std::uint32_t start = 0U;
    while (start <= value.SizeBytes()) {
        std::uint32_t end = start;
        while (end < value.SizeBytes() &&
            value[end] != ',') {
            ++end;
        }
        const Base::StringView token =
            ::Aero::Base::ValueConversion::Trim(
                value.Substr(start, end - start));
        if (token.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Grid definitions contain an empty track");
        }
        Base::Result<GridLength> parsed =
            ConvertGridLength(token);
        if (!parsed) return parsed.GetStatus();
        output.PushBack(parsed.Value());
        if (end == value.SizeBytes()) break;
        start = end + 1U;
    }
    return {};
}

} // namespace Aero

namespace Aero::Controls {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Render;



void Control::OnRender(
    ::Aero::Media::DrawingContext& context) noexcept {
    // A templated Control delegates its chrome to the template. Painting the
    // base Background as well produces an extra full-control rectangle behind
    // custom ComboBox, TreeView, Button and similar templates.
    if (GetTemplateRoot() != nullptr) return;
    auto& builder = Aero::Render::DrawingBridge::Builder(context);
    static_cast<void>(PaintBrushRect(
        builder,
        GetBackground(),
        Rect{
            0.0, 0.0,
            GetRenderSize().width,
            GetRenderSize().height}));
}




































































































































} // namespace Aero::Controls

namespace Aero::Controls {









} // namespace Aero::Controls

