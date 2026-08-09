#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Stream.hpp>
#include <Aero/Markup/XamlProvider.hpp>

#include <cstdint>

namespace Aero::Media {

struct TextureInfo {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::uint32_t x = 0U;
    std::uint32_t y = 0U;
    float dpiScale = 1.0F;
};

struct TextureResourceInfo {
    Markup::StreamResourceInfo source;
    TextureInfo texture;
};

using TextureProviderChangedHandler =
    Base::Delegate<void(const Base::ResourceUri&)>;

class AERO_GUI_API TextureProvider : public Base::Object {
public:
    virtual ~TextureProvider() = default;

    virtual Result<TextureResourceInfo> Open(
        const Base::ResourceUri& uri) const noexcept = 0;
    virtual Result<std::uint64_t> Revision(
        const Base::ResourceUri&) const noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Texture provider does not expose revision probes");
    }
    void AddChangedHandler(
        const TextureProviderChangedHandler& handler) noexcept {
        changed_.Add(handler);
    }
    bool RemoveChangedHandler(
        const TextureProviderChangedHandler& handler) noexcept {
        return changed_.Remove(handler);
    }

protected:
    // Must be raised on the owning Gui's dispatcher thread. An empty URI
    // invalidates every texture supplied by this provider.
    void RaiseChanged(const Base::ResourceUri& uri = {}) noexcept {
        if (changed_) changed_(uri);
    }

private:
    TextureProviderChangedHandler changed_;
};

} // namespace Aero::Media

