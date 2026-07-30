#pragma once

#include "presentation/RenderingInternal.hpp"

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Presentation/Style.hpp>

#include <cstdint>

namespace Aero::Controls {
class TemplateManager;
}

namespace Aero::Presentation {
class StyleManager;
class BindingManager;
}

namespace Aero::Diagnostics {

struct InspectorTreeNode final {
    Presentation::Visual* node = nullptr;
    Presentation::VisualHandle handle;
    Presentation::VisualHandle parent;
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

    Presentation::Visual* target = nullptr;
    Base::Vector<InspectorTreeNode>
        logicalTree;
    Base::Vector<InspectorTreeNode>
        visualTree;
    Base::Vector<InspectorProperty>
        effectiveProperties;
    Base::Vector<
        Presentation::BindingInspection>
        activeBindings;
    Base::Ref<Base::Object> dataContext;
    const Presentation::Style*
        appliedStyle = nullptr;
    Controls::TemplateHandle
        appliedTemplate;
    Base::Rect layoutRect;
    Base::Rect layoutClip;
    Base::Size renderSize;
    Presentation::RenderDiagnostics
        render;
    Core::DispatcherFrameTimings
        frameTimings;
};

class AERO_API InspectorEndpoint final {
public:
    InspectorEndpoint(
        Presentation::ObjectTree& tree,
        Core::EffectiveValueEngine& values,
        Presentation::BindingManager&
            bindings,
        Presentation::RenderManager&
            renderer,
        Presentation::StyleManager*
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
        Presentation::Visual& target,
        InspectorSnapshot& output,
        std::uint32_t maxTreeNodes =
            4096U) const noexcept;

    const Presentation::RenderPlan&
    RenderPlan() const noexcept {
        return renderer_->CurrentPlan();
    }

private:
    Presentation::ObjectTree* tree_ =
        nullptr;
    Core::EffectiveValueEngine* values_ =
        nullptr;
    Presentation::BindingManager*
        bindings_ = nullptr;
    Presentation::RenderManager*
        renderer_ = nullptr;
    Presentation::StyleManager* styles_ =
        nullptr;
    Controls::TemplateManager*
        templates_ = nullptr;
};

} // namespace Aero::Diagnostics
