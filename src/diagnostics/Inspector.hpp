#pragma once

namespace Aero::Internal { class TemplateEngine; }

#include "gui/ElementInternal.hpp"
#include "../controls/TemplateInternals.hpp"
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
    Meta::TypeId runtimeType =
        Meta::InvalidTypeId;
    std::uint32_t depth = 0U;
};

struct InspectorProperty final {
    Meta::DependencyPropertyHandle property;
    Meta::PropertyValue value;
    Meta::EffectiveValueSource valueSource =
        Meta::EffectiveValueSource::Default;
    Meta::EffectiveValueDiagnostics
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
    Internal::TemplateHandle
        appliedTemplate;
    Base::Rect layoutRect;
    Base::Rect layoutClip;
    Base::Size renderSize;
    Integration::RenderDiagnostics
        render;
    ::Aero::Threading::DispatcherFrameTimings
        frameTimings;
};

class AERO_API Inspector final {
public:
    Inspector(
        Aero::ElementTree& tree,
        Meta::EffectiveValueEngine& values,
        Aero::Internal::BindingEngine&
            bindings,
        Internal::RenderTree&
            renderer,
        Aero::Internal::StyleEngine*
            styles = nullptr,
        Aero::Internal::TemplateEngine*
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
    Aero::Internal::BindingEngine*
        bindings_ = nullptr;
    Internal::RenderTree*
        renderer_ = nullptr;
    Aero::Internal::StyleEngine* styles_ =
        nullptr;
    Aero::Internal::TemplateEngine*
        templates_ = nullptr;
};

} // namespace Aero::Diagnostics
