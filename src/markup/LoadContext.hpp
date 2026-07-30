#pragma once

#include "Loader.hpp"
#include "LoaderResult.hpp"

namespace Aero::Markup {

using LoadFinalizeCallback = Base::Result<void> (*)(
    LoaderResult& result,
    void* context) noexcept;

struct LoadContext final {
    const Presentation::ResourceDictionary* resources = nullptr;
    Core::EffectiveValueEngine* effectiveValues = nullptr;
    Presentation::BindingManager* bindings = nullptr;
    Presentation::ResourceDictionary* fallbackResources = nullptr;
    const Base::ResourceUri* baseUri = nullptr;
    Base::Object* templatedParent = nullptr;
    Base::Ref<Base::Object> existingRoot;
    Base::Ref<EffectLifetime> effectLifetime;
    EffectCommitMode effectCommitMode =
        EffectCommitMode::Immediate;
    DocumentCache* documentCache = nullptr;
    Core::Dispatcher* dispatcher = nullptr;
    Core::DependencyPropertyRegistry* dependencyProperties = nullptr;
    std::uint32_t maxObjects = UINT32_MAX;
    bool deferUnresolvedStaticResources = false;
    LoadFinalizeCallback finalize = nullptr;
    void* finalizeContext = nullptr;
};

} // namespace Aero::Markup
