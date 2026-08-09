#pragma once

#include <Aero/Controls.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Controls.hpp>
#include "render/RenderResources.hpp"
#include "gui/controls/TextBlockLayout.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/PasswordBox.hpp>
#include <Aero/Shapes.hpp>

#include <utility>

namespace Aero::Controls { class TemplateEngine; }

namespace Aero {
class ElementTree;
}

namespace Aero::Controls {

enum class ItemSubtreeChange : std::uint8_t {
    Mounted = 0U,
    Unmounting,
};

using ItemSubtreeCallback = Base::Result<void> (*)(
    Aero::Media::Visual& root,
    ItemSubtreeChange change,
    void* context) noexcept;

} // namespace Aero::Controls

namespace Aero::Controls {

using namespace ::Aero::Controls;
using namespace ::Aero;

// One private entry point owns the standard control storage and template hooks.
// This replaces per-control-family Access classes without adding a new runtime
// object or virtual dispatch layer.
struct Control::Access {
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
        static_cast<void>(control);
        static_cast<void>(engine);
    }
    static void SetVisualStateManager(
        Control& control,
        Aero::VisualStateManager* manager) noexcept {
        static_cast<void>(control);
        static_cast<void>(manager);
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
        return ::Aero::Media::Visual::Access::PanelChildCount(panel);
    }
    static Base::Ref<Base::Object> At(
        const Panel& panel,
        std::uint32_t index) noexcept {
        return ::Aero::Media::Visual::Access::PanelChildAt(panel, index);
    }
    static Base::Result<void> Add(
        Panel& panel,
        const Base::Ref<Base::Object>& owner,
        UIElement& child) noexcept {
        return ::Aero::Media::Visual::Access::PanelAddChild(panel, owner, child);
    }
    static Base::Result<bool> Remove(
        Panel& panel,
        UIElement& child) noexcept {
        return ::Aero::Media::Visual::Access::PanelRemoveChild(panel, child);
    }
    static Base::Result<void> Clear(Panel& panel) noexcept {
        ::Aero::Media::Visual::Access::PanelClearChildren(panel);
        return {};
    }

    static const Base::Ref<Base::Object>& OwnedChild(
        const Decorator& decorator) noexcept {
        return ::Aero::Media::Visual::Access::DecoratorOwnedChild(decorator);
    }
    static Base::Result<void> SetOwnedChild(
        Decorator& decorator,
        const Base::Ref<Base::Object>& owner,
        UIElement& child) noexcept {
        return ::Aero::Media::Visual::Access::DecoratorSetOwnedChild(
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
        Aero::LayoutEngine& layout,
        Meta::EffectiveValueEngine& values,
        Aero::StyleEngine* styles = nullptr,
        ::Aero::Render::RenderTree* renderer = nullptr,
        Aero::Controls::TemplateEngine* templates = nullptr,
        ItemSubtreeCallback subtreeCallback = nullptr,
        void* subtreeContext = nullptr) noexcept;


        static void InvalidateGeometry(
            Aero::Shapes::Path& path) noexcept {
            ::Aero::Media::Visual::Access::PathInvalidateGeometry(path);
        }

        static void Attach(
            Aero::Shapes::Path& path,
            Aero::Render::MeshResources* services,
            bool invalidate = false) noexcept {
            ::Aero::Media::Visual::Access::PathAttachMeshResources(
                path, services, invalidate);
        }

        static void Attach(
            TextBlock& text,
            TextBlockLayout* service,
            bool invalidate = false) noexcept {
            if (::Aero::Media::Visual::Access::TextLayoutRuntime(text) == service &&
                !invalidate) {
                return;
            }
            text.ReleaseServiceGlyphRun();
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
            if (::Aero::Media::Visual::Access::TextLayoutRuntime(text) == service &&
                !invalidate) {
                return;
            }
            text.ReleaseGlyphRuns();
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

namespace Aero::Controls {

using ControlPrivate = ::Aero::Controls::Control::Access;

} // namespace Aero::Controls
