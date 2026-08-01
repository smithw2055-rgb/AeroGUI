#pragma once

#include "gui/ElementInternal.hpp"
#include "../controls/TemplateAccess.hpp"
#include "gui/BindingInternal.hpp"

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

struct InspectorTreeNode final {
    Aero::Visual* node = nullptr;
    Aero::VisualHandle handle;
    Aero::VisualHandle parent;
    Core::TypeId runtimeType =
        Core::InvalidTypeId;
    std::uint32_t depth = 0U;
};

struct InspectorProperty final {
    Core::DependencyPropertyHandle property;
    Core::PropertyValue value;
    Core::EffectiveValueSource valueSource =
        Core::EffectiveValueSource::Default;
    Core::EffectiveValueDiagnostics
        diagnostics;
    bool hasEngineDiagnostics = false;
};

struct InspectorSnapshot final {
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
    Base::Ref<Base::Object> dataContext;
    const Aero::Style*
        appliedStyle = nullptr;
    Controls::Detail::TemplateHandle
        appliedTemplate;
    Base::Rect layoutRect;
    Base::Rect layoutClip;
    Base::Size renderSize;
    Render::RenderDiagnostics
        render;
    Core::DispatcherFrameTimings
        frameTimings;
};

class AERO_API InspectorEndpoint final {
public:
    InspectorEndpoint(
        Aero::GuiContext& tree,
        Core::EffectiveValueEngine& values,
        Aero::Detail::BindingManager&
            bindings,
        Render::RenderTree&
            renderer,
        Aero::Detail::StyleManager*
            styles = nullptr,
        Controls::TemplateManager*
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

    const Render::RenderFrame&
    RenderFrame() const noexcept {
        return renderer_->CurrentFrame();
    }

private:
    Aero::GuiContext* tree_ =
        nullptr;
    Core::EffectiveValueEngine* values_ =
        nullptr;
    Aero::Detail::BindingManager*
        bindings_ = nullptr;
    Render::RenderTree*
        renderer_ = nullptr;
    Aero::Detail::StyleManager* styles_ =
        nullptr;
    Controls::TemplateManager*
        templates_ = nullptr;
};

} // namespace Aero::Diagnostics
