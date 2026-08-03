#pragma once

#include "TemplateProgram.hpp"
#include "gui/GuiPrivate.hpp"

namespace Aero::Controls::Detail {

// The implementation objects live in Base::Detail, while their public model
// types are owned by Controls.  Keep that dependency explicit in this
// source-only header instead of leaking a Controls::Detail namespace.
using namespace ::Aero::Controls;

struct TemplatePart {
    Base::String name;
    Base::Ref<Base::Object> owner;
    Visual* visual = nullptr;
    DependencyObject* object = nullptr;
    FrameworkElement* frameworkElement = nullptr;
    Aero::GuiPrivate::Detail::ElementAttachment mount;
};

struct TemplateContentProjection {
    ContentControl* owner = nullptr;
    ContentPresenter* presenter = nullptr;
    ContentControl* contentHost = nullptr;
    UIElement* content = nullptr;
    Visual* originalVisualParent = nullptr;
    Aero::GuiPrivate::Detail::VisualAttachment projectedMount;
    bool attachedLogical = false;
    bool detachedOriginalVisual = false;
};

struct TemplateBuildState {
    TemplateBuildState(ElementTree& tree, Control& parent,
        Aero::GuiPrivate::Detail::LayoutEngine* layout,
        Aero::Render::Detail::RenderTree* renderer) noexcept
        : tree(&tree), layout(layout), renderer(renderer), parent(&parent) {}

    ElementTree* tree = nullptr;
    Aero::GuiPrivate::Detail::LayoutEngine* layout = nullptr;
    Aero::Render::Detail::RenderTree* renderer = nullptr;
    Control* parent = nullptr;
    Visual* rootVisual = nullptr;
    UIElement* rootElement = nullptr;
    Base::Vector<TemplatePart> parts;
    Base::Vector<TemplateContentProjection> projections;
};

} // namespace Aero::Controls::Detail
