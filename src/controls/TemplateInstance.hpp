#pragma once

#include "TemplateProgram.hpp"
#include "gui/MetadataInternal.hpp"
#include "gui/PropertyInternal.hpp"
#include "gui/FreezableInternal.hpp"
#include "gui/ElementInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"

namespace Aero::Controls::Detail {

// The implementation objects live in Base::Detail, while their public model
// types are owned by Controls.  Keep that dependency explicit in this
// source-only header instead of leaking a Controls::Detail namespace.
using namespace ::Aero::Controls;

struct TemplatePart {
    Base::String name;
    Base::Ref<Base::Object> owner;
    ::Aero::Media::Visual* visual = nullptr;
    DependencyObject* object = nullptr;
    FrameworkElement* frameworkElement = nullptr;
    Aero::GuiPrivate::Detail::ElementAttachment mount;
};

struct TemplateContentProjection {
    ContentControl* owner = nullptr;
    ContentPresenter* presenter = nullptr;
    ContentControl* contentHost = nullptr;
    UIElement* content = nullptr;
    ::Aero::Media::Visual* originalVisualParent = nullptr;
    Aero::GuiPrivate::Detail::VisualAttachment projectedMount;
    bool attachedLogical = false;
    bool detachedOriginalVisual = false;
};

struct TemplateBuildState {
    TemplateBuildState(ElementTree& tree, Control& parent,
        Aero::GuiPrivate::Detail::LayoutEngine* layout,
        Aero::Render::Detail::RenderTree* renderer,
        Aero::GuiPrivate::Detail::BindingEngine* bindings) noexcept
        : tree(&tree), layout(layout), renderer(renderer),
          bindings(bindings), parent(&parent) {}

    ElementTree* tree = nullptr;
    Aero::GuiPrivate::Detail::LayoutEngine* layout = nullptr;
    Aero::Render::Detail::RenderTree* renderer = nullptr;
    Aero::GuiPrivate::Detail::BindingEngine* bindings = nullptr;
    Control* parent = nullptr;
    ::Aero::Media::Visual* rootVisual = nullptr;
    UIElement* rootElement = nullptr;
    Base::Vector<TemplatePart> parts;
    Base::Vector<TemplateContentProjection> projections;
};

} // namespace Aero::Controls::Detail
