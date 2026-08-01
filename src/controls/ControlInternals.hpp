#pragma once

#include <Aero/Controls/Base.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Controls/Items.hpp>
#include "gui/ElementInternal.hpp"
#include "render/RenderResources.hpp"
#include "TextBlockLayout.hpp"
#include <Aero/Controls/Panels.hpp>
#include <Aero/Controls/Text.hpp>

#include <utility>

namespace Aero::Detail { class TemplateEngine; }

namespace Aero {
class ElementTree;
namespace Render { class RenderTree; }
}

namespace Aero::Controls::Detail {

// One private entry point owns the standard control storage and template hooks.
// This replaces per-control-family Access classes without adding a new runtime
// object or virtual dispatch layer.
class ControlPrivate final {
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
        return control.SetTemplateChildCore(child);
    }
    static void AttachTemplateEngine(
        Control& control,
        void* engine) noexcept {
        control.AttachTemplateRuntime(engine);
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
        return control.SetOwnedContent(owner, content);
    }
    static Base::Result<void> SetContentValue(
        ContentControl& control,
        Base::Ref<Base::Object> value) noexcept {
        return control.SetContentValue(std::move(value));
    }
    static Base::Result<void> SetContentValue(
        ContentControl& control,
        Core::Value value) noexcept {
        return control.SetContentValue(std::move(value));
    }
    static void OnContentPropertyChanged(
        ::Aero::DependencyObject& object,
        const Core::DependencyPropertyChangedEventArgs& change) noexcept {
        ContentControl::OnContentPropertyChanged(object, change);
    }
    static Base::Result<Base::Ref<Base::Object>> CreateTemplatedContent(
        const ContentControl& control) noexcept {
        return control.TryCreateTemplatedContent();
    }

    static std::uint32_t Count(const Panel& panel) noexcept {
        return panel.ChildCountCore();
    }
    static Base::Ref<Base::Object> At(
        const Panel& panel,
        std::uint32_t index) noexcept {
        return panel.ChildAtCore(index);
    }
    static Base::Result<void> Add(
        Panel& panel,
        const Base::Ref<Base::Object>& owner,
        UIElement& child) noexcept {
        return panel.AddChildCore(owner, child);
    }
    static Base::Result<bool> Remove(
        Panel& panel,
        UIElement& child) noexcept {
        return panel.RemoveChildCore(child);
    }
    static Base::Result<void> Clear(Panel& panel) noexcept {
        return panel.ClearChildrenCore();
    }

    static const Base::Ref<Base::Object>& OwnedChild(
        const Decorator& decorator) noexcept {
        return decorator.ownedChild_;
    }
    static Base::Result<void> SetOwnedChild(
        Decorator& decorator,
        const Base::Ref<Base::Object>& owner,
        UIElement& child) noexcept {
        return decorator.SetOwnedChild(owner, child);
    }

    static Base::Result<void> SetGeneratedTextContent(
        ContentControl& container,
        const Base::Ref<Base::Object>& contentObject,
        UIElement& content) noexcept {
        return container.SetGeneratedTextContent(contentObject, content);
    }

    static Base::Result<ItemContainerGenerator*> Create(
        ElementTree& tree,
        Aero::Detail::LayoutEngine& layout,
        Core::EffectiveValueEngine& values,
        Aero::Detail::StyleEngine* styles = nullptr,
        Render::RenderTree* renderer = nullptr,
        Aero::Detail::TemplateEngine* templates = nullptr,
        ItemSubtreeCallback subtreeCallback = nullptr,
        void* subtreeContext = nullptr) noexcept;


        static void InvalidateGeometry(
            Path& path) noexcept {
            path.ResetGeometry();
        }

        static void Attach(
            Path& path,
            Aero::Detail::MeshResources* services,
            bool invalidate = false) noexcept {
            path.AttachMeshResources(
                services, invalidate);
            if (invalidate) {
                static_cast<void>(path.InvalidateVisual());
            }
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

} // namespace Aero::Controls::Detail
