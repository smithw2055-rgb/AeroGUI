#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Controls/Items.hpp>
#include "../runtime/RuntimeFwd.hpp"

namespace Aero {
class ObjectTree;
namespace Render { class RenderManager; }
}

namespace Aero::Controls::Detail {

class ItemContainerGeneratorAccess final {
public:
    static Base::Result<void> SetGeneratedTextContent(
        ContentControl& container,
        const Base::Ref<Base::Object>& contentObject,
        UIElement& content) noexcept {
        return container.SetGeneratedTextContent(
            contentObject, content);
    }

    static Base::Result<ItemContainerGenerator*> Create(
        ObjectTree& tree,
        Aero::Detail::LayoutManager& layout,
        Core::EffectiveValueEngine& values,
        Aero::Detail::StyleManager* styles = nullptr,
        Render::RenderManager* renderer = nullptr,
        TemplateManager* templates = nullptr,
        ItemSubtreeCallback subtreeCallback = nullptr,
        void* subtreeContext = nullptr) noexcept;
};

} // namespace Aero::Controls::Detail
