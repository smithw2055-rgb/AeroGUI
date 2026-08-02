#pragma once

#include "TemplateProgram.hpp"
#include "gui/ElementInternal.hpp"

namespace Aero::Internal {

// The implementation objects live in Base::Detail, while their public model
// types are owned by Controls.  Keep that dependency explicit in this
// source-only header instead of leaking a Controls::Detail namespace.
using namespace ::Aero::Controls;

struct TemplatePart final {
    Base::String name;
    Base::Ref<Base::Object> owner;
    Visual* visual = nullptr;
    DependencyObject* object = nullptr;
    FrameworkElement* frameworkElement = nullptr;
    Aero::Internal::ElementAttachment mount;
};

struct TemplateContentProjection final {
    ContentControl* owner = nullptr;
    ContentPresenter* presenter = nullptr;
    ContentControl* contentHost = nullptr;
    UIElement* content = nullptr;
    Visual* originalVisualParent = nullptr;
    Aero::Internal::VisualAttachment projectedMount;
    bool attachedLogical = false;
    bool detachedOriginalVisual = false;
};

struct TemplateBuildState final {
    TemplateBuildState(ElementTree& tree, Control& parent,
        Aero::Internal::LayoutEngine* layout,
        Aero::Internal::RenderTree* renderer) noexcept
        : tree(&tree), layout(layout), renderer(renderer), parent(&parent) {}

    ElementTree* tree = nullptr;
    Aero::Internal::LayoutEngine* layout = nullptr;
    Aero::Internal::RenderTree* renderer = nullptr;
    Control* parent = nullptr;
    Visual* rootVisual = nullptr;
    UIElement* rootElement = nullptr;
    Base::Vector<TemplatePart> parts;
    Base::Vector<TemplateContentProjection> projections;
};

} // namespace Aero::Internal
