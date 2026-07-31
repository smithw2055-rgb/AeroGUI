#pragma once

#include <Aero/Styling.hpp>
#include "../ui/MountService.hpp"

namespace Aero::Controls { class ContentPresenter; }

namespace Aero::Controls::Detail {

struct TemplatePart final {
    Base::String name;
    Base::Ref<Base::Object> owner;
    Visual* visual = nullptr;
    DependencyObject* object = nullptr;
    FrameworkElement* frameworkElement = nullptr;
    Aero::Detail::MountEdgeState mount;
};

struct TemplateContentProjection final {
    ContentControl* owner = nullptr;
    ContentPresenter* presenter = nullptr;
    ContentControl* contentHost = nullptr;
    UIElement* content = nullptr;
    Visual* originalVisualParent = nullptr;
    Aero::Detail::UiMountState projectedMount;
    bool attachedLogical = false;
    bool detachedOriginalVisual = false;
};

struct TemplateBuildState final {
    TemplateBuildState(
        ObjectTree& tree,
        Control& parent,
        Aero::Detail::LayoutManager* layout,
        Render::RenderManager* renderer) noexcept
        : tree(&tree),
          layout(layout),
          renderer(renderer),
          mounts(tree, layout, renderer),
          parent(&parent) {}

    ObjectTree* tree = nullptr;
    Aero::Detail::LayoutManager* layout = nullptr;
    Render::RenderManager* renderer = nullptr;
    Aero::Detail::MountService mounts;
    Control* parent = nullptr;
    Visual* rootVisual = nullptr;
    UIElement* rootElement = nullptr;
    Base::Vector<TemplatePart> parts;
    Base::Vector<TemplateContentProjection> projections;
};


} // namespace Aero::Controls::Detail
