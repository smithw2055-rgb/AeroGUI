#pragma once

#include "TemplateProgram.hpp"
#include "gui/meta/MetadataState.hpp"
#include "gui/core/state/ElementTree.hpp"
#include "gui/core/state/LayoutEngine.hpp"
#include "gui/core/state/FreezableState.hpp"
#include "gui/core/state/EffectiveValueEngine.hpp"
#include "gui/core/state/RoutedEvents.hpp"
#include "gui/core/state/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/input/InputState.hpp" 
#include "gui/data/BindingEngine.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include <Aero/Base/HashMap.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Resources.hpp>
#include "render/RenderTree.hpp"

namespace Aero::Controls {

using namespace Aero::Meta;

// The implementation objects live in Aero::Base, while their public model
// types are owned by Controls.  Keep that dependency explicit in this
// source-only header instead of leaking a Controls namespace.
using namespace ::Aero::Controls;

struct TemplatePart {
    Base::String name;
    Base::Ref<Base::Object> owner;
    ::Aero::Media::Visual* visual = nullptr;
    DependencyObject* object = nullptr;
    FrameworkElement* frameworkElement = nullptr;
    Aero::ElementAttachment mount;
};

struct TemplateContentProjection {
    ContentControl* owner = nullptr;
    ContentPresenter* presenter = nullptr;
    ContentControl* contentHost = nullptr;
    UIElement* content = nullptr;
    ::Aero::Media::Visual* originalVisualParent = nullptr;
    Aero::VisualAttachment projectedMount;
    bool attachedLogical = false;
    bool detachedOriginalVisual = false;
};

struct TemplateBuildState {
    TemplateBuildState(ElementTree& tree, Control& parent) noexcept
        : tree(&tree), parent(&parent) {}

    ElementTree* tree = nullptr;
    Control* parent = nullptr;
    ::Aero::Media::Visual* rootVisual = nullptr;
    UIElement* rootElement = nullptr;
    Base::Vector<TemplatePart> parts;
    Base::Vector<TemplateContentProjection> projections;

    Aero::LayoutEngine* Layout() const noexcept {
        return tree != nullptr ? tree->Layout() : nullptr;
    }
    Aero::Render::RenderTree* RenderTree() const noexcept {
        return tree != nullptr ? tree->RenderTree() : nullptr;
    }
    Aero::BindingEngine* Bindings() const noexcept {
        return tree != nullptr ? tree->Bindings() : nullptr;
    }
};


class TemplateEngine {
public:
    TemplateEngine(
        ElementTree& tree,
        EffectiveValueEngine& values,
        DependencyPropertyRegistry& properties,
        LayoutEngine* layout = nullptr,
        ::Aero::Render::RenderTree* renderer = nullptr,
        ::Aero::Meta::Registry* metadata = nullptr,
        Aero::BindingEngine* bindings = nullptr,
        Aero::ResourceDictionary* resources = nullptr) noexcept
        : tree_(&tree),
          effectiveValues_(&values),
          providerSession_(values),
          values_(&providerSession_),
          properties_(&properties),
          layout_(layout),
          renderer_(renderer),
          metadata_(metadata),
          bindings_(bindings),
          resources_(resources),
          propertyChangedHandler_(
              this, &TemplateEngine::OnPropertyChanged) {}
    ~TemplateEngine() noexcept;

    Base::Result<TemplateHandle> Apply(
        Control& control,
        const ControlTemplate& plan) noexcept;
    Base::Result<bool> Clear(
        TemplateHandle handle) noexcept;
    Base::Result<bool> Clear(
        Control& control) noexcept;
    DependencyObject* FindName(
        TemplateHandle handle,
        Base::StringView name) const noexcept;
    DependencyObject* FindPart(
        TemplateHandle handle,
        TypeId type) const noexcept;
    TemplateHandle AppliedHandle(
        const Control& control) const noexcept;
    const ControlTemplate* AppliedTemplate(
        TemplateHandle handle) const noexcept;
    bool HasTemplateBinding(
        DependencyObject& target,
        DependencyPropertyHandle property) const noexcept;
    Base::Result<void> RefreshTemplateBinding(
        DependencyObject& target,
        DependencyPropertyHandle property) noexcept;

private:
    struct Instance {
        TemplateHandle handle;
        Control* parent = nullptr;
        const ControlTemplate* plan = nullptr;
        ::Aero::Media::Visual* rootVisual = nullptr;
        UIElement* rootElement = nullptr;
        Base::Vector<Aero::Controls::TemplatePart> parts;
        Base::Vector<Aero::Controls::TemplateContentProjection> projections;
        NameScope names;
        Base::Vector<Data::BindingHandle>
            metadataBindings;
        Base::Vector<DependencyObject*> dynamicResourceTargets;
    };

    ElementTree* tree_ = nullptr;
    EffectiveValueEngine* effectiveValues_ = nullptr;
    ::Aero::TemplatedParentProviderSession providerSession_;
    ::Aero::TemplatedParentProviderSession* values_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    LayoutEngine* layout_ = nullptr;
    ::Aero::Render::RenderTree* renderer_ = nullptr;
    ::Aero::Meta::Registry* metadata_ = nullptr;
    Aero::BindingEngine* bindings_ = nullptr;
    Aero::ResourceDictionary* resources_ = nullptr;
    Base::Vector<Instance> instances_;
    Base::HashMap<const Control*, std::uint32_t> controlToInstance_;
    Base::HashMap<std::uint64_t, std::uint32_t> handleToInstance_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    std::uint64_t nextHandle_ = 1U;

    std::uint32_t FindInstance(
        TemplateHandle handle) const noexcept;
    std::uint32_t FindInstance(
        const Control& control) const noexcept;
    DependencyObject* FindTarget(
        const Instance& instance,
        Base::StringView name) const noexcept;
    Base::Result<void> Subscribe(
        Instance& instance) noexcept;
    void Unsubscribe(Instance& instance) noexcept;
    Base::Result<void> ApplyBindings(
        Instance& instance,
        DependencyPropertyHandle changed =
            DependencyPropertyHandle{}) noexcept;
    Base::Result<void> AttachMetadataBindings(
        Instance& instance) noexcept;
    void DetachMetadataBindings(
        Instance& instance) noexcept;
    Base::Result<void> AttachDynamicResources(
        Instance& instance) noexcept;
    void DetachDynamicResources(
        Instance& instance) noexcept;
    Base::Result<void> EvaluateTriggers(
        Instance& instance) noexcept;
    Base::Result<void> ClearProviders(
        Instance& instance) noexcept;
    Base::Result<void> ClearAt(
        std::uint32_t index) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
};

} // namespace Aero::Controls
