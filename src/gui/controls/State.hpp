#pragma once

#include <Aero/Controls.hpp>
#include <Aero/Base/Result.hpp>
#include "gui/core/State.hpp"
#include "render/RenderResources.hpp"
#include "gui/controls/TextBlockLayout.hpp"
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Shapes.hpp>
#include "gui/core/facets/VisualFacet.hpp"
#include "gui/core/facets/DependencyPropertyFacet.hpp"
#include "gui/core/facets/InteractionStateFacet.hpp"
#include "gui/core/facets/TextLayoutFacet.hpp"

#include <utility>

namespace Aero::Controls {

class TemplateEngine;
struct ItemContainerGeneratorRuntime;

// Internal adapter for scalar ItemsSource values. It is deliberately kept out
// of the public controls surface; callers use AddBoxedItem helpers instead.
class BoxedItemValue : public Base::Object {
    AERO_DECLARE_TYPE(BoxedItemValue, Base::Object)
public:
    explicit BoxedItemValue(Meta::Value value) noexcept
        : value_(std::move(value)) {}

    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    const Meta::Value& Value() const noexcept {
        return value_;
    }

private:
    Meta::Value value_;
};

using ItemContainerGeneratorImpl =
    ::Aero::Controls::ItemContainerGeneratorRuntime;

} // namespace Aero::Controls
