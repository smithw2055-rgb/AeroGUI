#pragma once

#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Markup/CompiledDocument.hpp>

#include <cstdint>

namespace Aero::Markup {

namespace Detail {
class LoadOptionsAccess;
}

struct LoadPolicy final {
    bool allowNetwork = false;
    bool allowFile = true;
    bool allowPackApplication = true;
};

struct LoadLimits final {
    XmlTokenizerLimits xml;
    CompiledDocumentLimits compiled;
    std::uint64_t maxSourceBytes =
        16ULL * 1024ULL * 1024ULL;
    std::uint32_t maxObjects = 100000U;
    std::uint32_t maxResources = 100000U;
    std::uint32_t maxDependencyDepth = 64U;
};

struct LoadOptions final {
    LoadPolicy policy;
    LoadLimits limits;
    Base::ResourceUri baseUri;

private:
    friend class Detail::LoadOptionsAccess;
    const void* context_ = nullptr;
};

} // namespace Aero::Markup
