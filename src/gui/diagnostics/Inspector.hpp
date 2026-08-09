#pragma once

namespace Aero::Controls { class TemplateEngine; }

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
#include "gui/controls/ControlRuntime.hpp"
#include "gui/controls/ItemsRuntime.hpp"
#include "gui/controls/TemplateRuntime.hpp"

#include "render/RenderTree.hpp"

#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Layout.hpp>
#include <Aero/FrameworkElement.hpp>

#include <cstdint>

namespace Aero::Controls {
}


namespace Aero::Diagnostics {

struct InspectorTreeNode {
    Aero::Media::Visual* node = nullptr;
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

    Aero::Media::Visual* target = nullptr;
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
    ::Aero::Controls::TemplateHandle
        appliedTemplate;
    Base::Rect layoutRect;
    Base::Rect layoutClip;
    Base::Size renderSize;
    ::Aero::Render::RenderDiagnostics
        render;
    ::Aero::Threading::DispatcherFrameTimings
        frameTimings;
};

class Inspector {
public:
    Inspector(
        Aero::ElementTree& tree,
        Meta::EffectiveValueEngine& values,
        Aero::BindingEngine&
            bindings,
        ::Aero::Render::RenderTree&
            renderer,
        Aero::StyleEngine*
            styles = nullptr,
        Aero::Controls::TemplateEngine*
            templates = nullptr) noexcept
        : tree_(&tree),
          values_(&values),
          bindings_(&bindings),
          renderer_(&renderer),
          styles_(styles),
          templates_(templates) {}

    Base::Result<void> Capture(
        Aero::Media::Visual& target,
        InspectorSnapshot& output,
        std::uint32_t maxTreeNodes =
            4096U) const noexcept;

    const ::Aero::Render::RenderFrame&
    RenderFrame() const noexcept {
        return renderer_->CurrentFrame();
    }

private:
    Aero::ElementTree* tree_ =
        nullptr;
    Meta::EffectiveValueEngine* values_ =
        nullptr;
    Aero::BindingEngine*
        bindings_ = nullptr;
    ::Aero::Render::RenderTree*
        renderer_ = nullptr;
    Aero::StyleEngine* styles_ =
        nullptr;
    Aero::Controls::TemplateEngine*
        templates_ = nullptr;
};

} // namespace Aero::Diagnostics
