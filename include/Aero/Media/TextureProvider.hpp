#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Stream.hpp>
#include <Aero/Markup/XamlProvider.hpp>

#include <cstdint>

namespace Aero::Integration {

struct TextureInfo {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint32_t x = 0U;
    std::uint32_t y = 0U;
    float dpiScale = 1.0F;
};

struct TextureResourceInfo {
    StreamResourceInfo source;
    TextureInfo texture;
};

class AERO_API TextureProvider {
public:
    virtual ~TextureProvider() = default;

    virtual Base::Result<TextureResourceInfo> Open(
        const Base::ResourceUri& uri) const noexcept = 0;
    virtual Base::Result<std::uint64_t> Revision(
        const Base::ResourceUri&) const noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Texture provider does not expose revision probes");
    }
    virtual std::uint64_t CacheIdentity() const noexcept {
        return Base::DefaultHash<const TextureProvider*>{}(this);
    }
};

} // namespace Aero::Integration

