#pragma once

#include <Aero/Controls/Core.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Controls/Items.hpp>
#include "gui/GuiPrivate.hpp"
#include "render/RenderResources.hpp"
#include "TextBlockLayout.hpp"
#include <Aero/Controls/Panels.hpp>
#include <Aero/Controls/Text.hpp>
#include <Aero/Shapes.hpp>

#include <utility>

namespace Aero::Controls::Detail { class TemplateEngine; }

namespace Aero {
class ElementTree;
namespace Base::Detail { class RenderTree; }
}

namespace Aero::Controls {

enum class ItemSubtreeChange : std::uint8_t {
    Mounted = 0U,
    Unmounting,
};

using ItemSubtreeCallback = Base::Result<void> (*)(
    Aero::Visual& root,
    ItemSubtreeChange change,
    void* context) noexcept;

} // namespace Aero::Controls

namespace Aero::Controls {

using namespace ::Aero::Controls::Detail;
using namespace ::Aero::GuiPrivate::Detail;

class VisualStateManager;

// One private entry point owns the standard control storage and template hooks.
// This replaces per-control-family Access classes without adding a new runtime
// object or virtual dispatch layer.
struct Control::Impl {
public:
    static bool IsTemplateApplied(const Control& control) noexcept {
        return control.templateHandleValue_ != 0U;
    }
    static std::uint64_t TemplateGeneration(
        const Control& control) noexcept {
        return control.templateGeneration_;
    }
    static UIElement* TemplateRoot(const Control& control) noexcept {
        return control.templateChild_;
    }
    static Base::Result<void> SetTemplateRoot(
        Control& control,
        UIElement* child) noexcept {
        control.SetTemplateChildCore(child);
        return {};
    }
    static void AttachTemplateEngine(
        Control& control,
        void* engine) noexcept {
        control.AttachTemplateRuntime(engine);
    }
    static void SetVisualStateManager(
        Control& control,
        VisualStateManager* manager) noexcept {
        control.visualStateRuntime_ = manager;
    }
    static void NotifyTemplateApplied(
        Control& control,
        std::uint64_t handleValue) noexcept {
        control.NotifyTemplateApplied(handleValue);
    }
    static void NotifyTemplateDetached(
        Control& control) noexcept {
        control.NotifyTemplateDetached();
    }
    static void InvokeTemplateApplied(
        Control& control) noexcept {
        control.OnApplyTemplate();
    }

    static UIElement* ContentElement(
        const ContentControl& control) noexcept {
        return control.content_;
    }
    static const Base::Ref<Base::Object>& OwnedContent(
        const ContentControl& control) noexcept {
        return control.ownedContent_;
    }
    static const Base::Ref<Base::Object>& ContentValue(
        const ContentControl& control) noexcept {
        return control.contentValue_;
    }
    static Base::Result<void> SetOwnedContent(
        ContentControl& control,
        const Base::Ref<Base::Object>& owner,
        UIElement& content) noexcept {
        control.SetOwnedContent(owner, content);
        return {};
    }
    static Base::Result<void> SetContentValue(
        ContentControl& control,
        Base::Ref<Base::Object> value) noexcept {
        control.SetContentValue(std::move(value));
        return {};
    }
    static Base::Result<void> SetContentValue(
        ContentControl& control,
        Meta::Value value) noexcept {
        control.SetContentValue(std::move(value));
        return {};
    }
    static void OnContentPropertyChanged(
        ::Aero::DependencyObject& object,
        const Meta::DependencyPropertyChangedEventArgs& change) noexcept {
        ContentControl::OnContentPropertyChanged(object, change);
    }
    static Base::Result<Base::Ref<Base::Object>> CreateTemplatedContent(
        const ContentControl& control) noexcept {
        return control.CreateTemplatedContent();
    }

    static std::uint32_t Count(const Panel& panel) noexcept {
        return ::Aero::Visual::Impl::PanelChildCount(panel);
    }
    static Base::Ref<Base::Object> At(
        const Panel& panel,
        std::uint32_t index) noexcept {
        return ::Aero::Visual::Impl::PanelChildAt(panel, index);
    }
    static Base::Result<void> Add(
        Panel& panel,
        const Base::Ref<Base::Object>& owner,
        UIElement& child) noexcept {
        return ::Aero::Visual::Impl::PanelAddChild(panel, owner, child);
    }
    static Base::Result<bool> Remove(
        Panel& panel,
        UIElement& child) noexcept {
        return ::Aero::Visual::Impl::PanelRemoveChild(panel, child);
    }
    static Base::Result<void> Clear(Panel& panel) noexcept {
        ::Aero::Visual::Impl::PanelClearChildren(panel);
        return {};
    }

    static const Base::Ref<Base::Object>& OwnedChild(
        const Decorator& decorator) noexcept {
        return ::Aero::Visual::Impl::DecoratorOwnedChild(decorator);
    }
    static Base::Result<void> SetOwnedChild(
        Decorator& decorator,
        const Base::Ref<Base::Object>& owner,
        UIElement& child) noexcept {
        return ::Aero::Visual::Impl::DecoratorSetOwnedChild(
            decorator, owner, child);
    }

    static Base::Result<void> SetGeneratedTextContent(
        ContentControl& container,
        const Base::Ref<Base::Object>& contentObject,
        UIElement& content) noexcept {
        container.SetGeneratedTextContent(contentObject, content);
        return {};
    }

    static Base::Result<ItemContainerGenerator*> Create(
        ElementTree& tree,
        Aero::GuiPrivate::Detail::LayoutEngine& layout,
        Meta::EffectiveValueEngine& values,
        Aero::GuiPrivate::Detail::StyleEngine* styles = nullptr,
        ::Aero::Render::Detail::RenderTree* renderer = nullptr,
        Aero::Controls::Detail::TemplateEngine* templates = nullptr,
        ItemSubtreeCallback subtreeCallback = nullptr,
        void* subtreeContext = nullptr) noexcept;


        static void InvalidateGeometry(
            Aero::Shapes::Path& path) noexcept {
            ::Aero::Visual::Impl::PathInvalidateGeometry(path);
        }

        static void Attach(
            Aero::Shapes::Path& path,
            Aero::Render::Detail::MeshResources* services,
            bool invalidate = false) noexcept {
            ::Aero::Visual::Impl::PathAttachMeshResources(
                path, services, invalidate);
        }

        static void Attach(
            TextBlock& text,
            TextBlockLayout* service,
            bool invalidate = false) noexcept {
            if (text.layoutService_ == service && !invalidate) {
                return;
            }
            text.ReleaseServiceGlyphRun();
            text.layoutService_ = service;
            text.glyphRuns_.Clear();
            text.glyphRunSize_ = {};
            if (invalidate) {
                static_cast<void>(text.InvalidateMeasure());
                static_cast<void>(text.InvalidateVisual());
            }
        }

        static void Attach(
            TextBox& text,
            TextBlockLayout* service,
            bool invalidate = false) noexcept {
            if (text.layoutService_ == service && !invalidate) {
                return;
            }
            text.ReleaseGlyphRuns();
            text.layoutService_ = service;
            if (invalidate) {
                static_cast<void>(text.InvalidateMeasure());
                static_cast<void>(text.InvalidateVisual());
            }
        }

        static void Attach(
            PasswordBox& password,
            TextBlockLayout* service,
            bool invalidate = false) noexcept {
            Attach(
                password.editor_,
                service,
                invalidate);
            if (invalidate) {
                static_cast<void>(
                    password.InvalidateMeasure());
                static_cast<void>(
                    password.InvalidateVisual());
            }
        }
};

} // namespace Aero::Controls

namespace Aero::Controls::Detail {

using ControlPrivate = ::Aero::Controls::Control::Impl;

} // namespace Aero::Controls::Detail

namespace Aero::Controls::Detail {
using ControlPrivate = ::Aero::Controls::Control::Impl;
}
