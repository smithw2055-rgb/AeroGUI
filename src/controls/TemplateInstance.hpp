#pragma once

#include "TemplateProgram.hpp"
#include "gui/ElementInternal.hpp"

namespace Aero::Controls::Detail {

struct TemplatePart final {
    Base::String name;
    Base::Ref<Base::Object> owner;
    Visual* visual = nullptr;
    DependencyObject* object = nullptr;
    FrameworkElement* frameworkElement = nullptr;
    Aero::Detail::ElementAttachment mount;
};

struct TemplateContentProjection final {
    ContentControl* owner = nullptr;
    ContentPresenter* presenter = nullptr;
    ContentControl* contentHost = nullptr;
    UIElement* content = nullptr;
    Visual* originalVisualParent = nullptr;
    Aero::Detail::VisualAttachment projectedMount;
    bool attachedLogical = false;
    bool detachedOriginalVisual = false;
};

struct TemplateBuildState final {
    TemplateBuildState(GuiContext& tree, Control& parent,
        Aero::Detail::LayoutManager* layout, Render::RenderTree* renderer) noexcept
        : tree(&tree), layout(layout), renderer(renderer), parent(&parent) {}

    GuiContext* tree = nullptr;
    Aero::Detail::LayoutManager* layout = nullptr;
    Render::RenderTree* renderer = nullptr;
    Control* parent = nullptr;
    Visual* rootVisual = nullptr;
    UIElement* rootElement = nullptr;
    Base::Vector<TemplatePart> parts;
    Base::Vector<TemplateContentProjection> projections;
};

} // namespace Aero::Controls::Detail
