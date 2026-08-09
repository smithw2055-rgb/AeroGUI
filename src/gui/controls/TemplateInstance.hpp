#pragma once

#include "TemplateProgram.hpp"
#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"

namespace Aero::Controls {

// The implementation objects live in Base::Detail, while their public model
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
    TemplateBuildState(ElementTree& tree, Control& parent,
        Aero::LayoutEngine* layout,
        Aero::Render::RenderTree* renderer,
        Aero::BindingEngine* bindings) noexcept
        : tree(&tree), layout(layout), renderer(renderer),
          bindings(bindings), parent(&parent) {}

    ElementTree* tree = nullptr;
    Aero::LayoutEngine* layout = nullptr;
    Aero::Render::RenderTree* renderer = nullptr;
    Aero::BindingEngine* bindings = nullptr;
    Control* parent = nullptr;
    ::Aero::Media::Visual* rootVisual = nullptr;
    UIElement* rootElement = nullptr;
    Base::Vector<TemplatePart> parts;
    Base::Vector<TemplateContentProjection> projections;
};

} // namespace Aero::Controls
