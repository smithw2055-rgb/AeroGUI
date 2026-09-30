#pragma once

// Src-only factory for ItemContainerGenerator. Engine refs stay out of the
// installed Controls/ItemContainerGenerator.hpp surface.

#include <Aero/Base/Result.hpp>
#include <Aero/Controls/ItemContainerGenerator.hpp>

namespace Aero {
class ElementTree;
class LayoutEngine;
class StyleEngine;
namespace Meta { class EffectiveValueEngine; }
namespace Render { class RenderTree; }
}

namespace Aero::Controls {

class TemplateEngine;

enum class ItemSubtreeChange : std::uint8_t { Mounted = 0U, Unmounting };
using ItemSubtreeCallback = Base::Result<void> (*)(
    ::Aero::Media::Visual& root,
    ItemSubtreeChange change,
    void* context) noexcept;

class ItemContainerGeneratorFactory {
public:
    static Base::Result<ItemContainerGenerator*> Create(
        ::Aero::ElementTree& tree,
        ::Aero::LayoutEngine& layout,
        ::Aero::Meta::EffectiveValueEngine& values,
        ::Aero::StyleEngine* styles,
        ::Aero::Render::RenderTree* renderer,
        TemplateEngine* templates,
        ItemSubtreeCallback callback,
        void* context) noexcept;
};

} // namespace Aero::Controls
