#pragma once

#include <Aero/Controls/Base.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Controls/Items.hpp>
#include "gui/ElementInternal.hpp"

namespace Aero::Controls::Detail {

class ControlAccess final {
public:
    static bool IsTemplateApplied(const Control& control) noexcept {
        return control.templateHandleValue_ != 0U;
    }

    static std::uint64_t TemplateGeneration(const Control& control) noexcept {
        return control.templateGeneration_;
    }

    static UIElement* TemplateRoot(const Control& control) noexcept {
        return control.templateChild_;
    }

    static Base::Result<void> SetTemplateRoot(Control& control, UIElement* child) noexcept {
        return control.SetTemplateChildCore(child);
    }
};

} // namespace Aero::Controls::Detail

namespace Aero::Controls::Detail {

class ContentControlAccess final {
public:
    static UIElement* ContentElement(const ContentControl& control) noexcept { return control.content_; }
    static const Base::Ref<Base::Object>& OwnedContent(const ContentControl& control) noexcept { return control.ownedContent_; }
    static const Base::Ref<Base::Object>& ContentValue(const ContentControl& control) noexcept { return control.contentValue_; }
    static Base::Result<void> SetOwnedContent(ContentControl& control, const Base::Ref<Base::Object>& owner, UIElement& content) noexcept { return control.SetOwnedContent(owner, content); }
    static Base::Result<void> SetContentValue(ContentControl& control, Base::Ref<Base::Object> value) noexcept { return control.SetContentValue(std::move(value)); }
    static Base::Result<void> SetContentValue(ContentControl& control, Core::Value value) noexcept { return control.SetContentValue(std::move(value)); }
    static void OnContentPropertyChanged(Core::DependencyObject& object, const Core::DependencyPropertyChangedEventArgs& change) noexcept { ContentControl::OnContentPropertyChanged(object, change); }
    static Base::Result<Base::Ref<Base::Object>> CreateTemplatedContent(const ContentControl& control) noexcept { return control.TryCreateTemplatedContent(); }
};

} // namespace Aero::Controls::Detail

namespace Aero::Controls::Detail {

class PanelAccess final {
public:
    static std::uint32_t Count(const Panel& panel) noexcept { return panel.ChildCountCore(); }
    static Base::Ref<Base::Object> At(const Panel& panel, std::uint32_t index) noexcept { return panel.ChildAtCore(index); }
    static Base::Result<void> Add(Panel& panel, const Base::Ref<Base::Object>& owner, UIElement& child) noexcept { return panel.AddChildCore(owner, child); }
    static Base::Result<bool> Remove(Panel& panel, UIElement& child) noexcept { return panel.RemoveChildCore(child); }
    static Base::Result<void> Clear(Panel& panel) noexcept { return panel.ClearChildrenCore(); }
};

class DecoratorAccess final {
public:
    static const Base::Ref<Base::Object>& OwnedChild(const Decorator& decorator) noexcept { return decorator.ownedChild_; }
    static Base::Result<void> SetOwnedChild(Decorator& decorator, const Base::Ref<Base::Object>& owner, UIElement& child) noexcept { return decorator.SetOwnedChild(owner, child); }
};

} // namespace Aero::Controls::Detail

namespace Aero {
class GuiContext;
namespace Render { class RenderTree; }
}

namespace Aero::Controls::Detail {

class ItemContainerGeneratorAccess final {
public:
    static Base::Result<void> SetGeneratedTextContent(
        ContentControl& container,
        const Base::Ref<Base::Object>& contentObject,
        UIElement& content) noexcept {
        return container.SetGeneratedTextContent(
            contentObject, content);
    }

    static Base::Result<ItemContainerGenerator*> Create(
        GuiContext& tree,
        Aero::Detail::LayoutManager& layout,
        Core::EffectiveValueEngine& values,
        Aero::Detail::StyleManager* styles = nullptr,
        Render::RenderTree* renderer = nullptr,
        TemplateManager* templates = nullptr,
        ItemSubtreeCallback subtreeCallback = nullptr,
        void* subtreeContext = nullptr) noexcept;
};

} // namespace Aero::Controls::Detail
