#pragma once

namespace Aero::Controls::Detail { class TemplateEngine; }

#include "gui/GuiPrivate.hpp"
#include "../controls/ControlsPrivate.hpp"

#include "render/RenderTree.hpp"

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Styling.hpp>
#include <Aero/Data.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>

#include <cstdint>

namespace Aero::Controls {
}


namespace Aero::Diagnostics {

struct InspectorTreeNode {
    Aero::Visual* node = nullptr;
    Aero::VisualHandle handle;
    Aero::VisualHandle parent;
    Meta::TypeId runtimeType =
        Meta::InvalidTypeId;
    std::uint32_t depth = 0U;
};

struct InspectorProperty {
    Meta::DependencyPropertyHandle property;
    Meta::PropertyValue value;
    Meta::EffectiveValueSource valueSource =
        Meta::EffectiveValueSource::Default;
    Meta::EffectiveValueDiagnostics
        diagnostics;
    bool hasEngineDiagnostics = false;
};

struct InspectorSnapshot {
    InspectorSnapshot() noexcept
        : logicalTree(),
          visualTree(),
          effectiveProperties(),
          activeBindings() {}

    Aero::Visual* target = nullptr;
    Base::Vector<InspectorTreeNode>
        logicalTree;
    Base::Vector<InspectorTreeNode>
        visualTree;
    Base::Vector<InspectorProperty>
        effectiveProperties;
    Base::Vector<
        Data::BindingInspection>
        activeBindings;
    Value dataContext;
    const Aero::Style*
        appliedStyle = nullptr;
    ::Aero::Controls::Detail::TemplateHandle
        appliedTemplate;
    Base::Rect layoutRect;
    Base::Rect layoutClip;
    Base::Size renderSize;
    Integration::RenderDiagnostics
        render;
    ::Aero::Threading::DispatcherFrameTimings
        frameTimings;
};

class AERO_API Inspector {
public:
    Inspector(
        Aero::ElementTree& tree,
        Meta::EffectiveValueEngine& values,
        Aero::GuiPrivate::Detail::BindingEngine&
            bindings,
        ::Aero::Render::Detail::RenderTree&
            renderer,
        Aero::GuiPrivate::Detail::StyleEngine*
            styles = nullptr,
        Aero::Controls::Detail::TemplateEngine*
            templates = nullptr) noexcept
        : tree_(&tree),
          values_(&values),
          bindings_(&bindings),
          renderer_(&renderer),
          styles_(styles),
          templates_(templates) {}

    Base::Result<void> Capture(
        Aero::Visual& target,
        InspectorSnapshot& output,
        std::uint32_t maxTreeNodes =
            4096U) const noexcept;

    const Integration::RenderFrame&
    RenderFrame() const noexcept {
        return renderer_->CurrentFrame();
    }

private:
    Aero::ElementTree* tree_ =
        nullptr;
    Meta::EffectiveValueEngine* values_ =
        nullptr;
    Aero::GuiPrivate::Detail::BindingEngine*
        bindings_ = nullptr;
    ::Aero::Render::Detail::RenderTree*
        renderer_ = nullptr;
    Aero::GuiPrivate::Detail::StyleEngine* styles_ =
        nullptr;
    Aero::Controls::Detail::TemplateEngine*
        templates_ = nullptr;
};

} // namespace Aero::Diagnostics
