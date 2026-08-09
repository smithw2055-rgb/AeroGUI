#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Stream.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>

#include <cstdint>

namespace Aero {
enum class FontWeight : std::uint8_t;
enum class FontStyle : std::uint8_t;
enum class FontStretch : std::uint8_t;
}

namespace Aero::Media {

// Host-facing font resource returned by FontProvider. The lower-level text
// engine has its own private file/memory source descriptor and the two models
// deliberately use distinct names.
struct FontResource {
    Base::ResourceUri uri;
    Ref<Base::Stream> stream;
    std::uint32_t faceIndex = 0U;
};

struct FontProviderChange {
    Base::ResourceUri baseUri;
    StringView familyName;
};

using FontProviderChangedHandler =
    Base::Delegate<void(const FontProviderChange&)>;

class AERO_GUI_API FontProvider : public Base::Object {
public:
    virtual ~FontProvider() = default;

    virtual Result<FontResource> MatchFont(
        const Base::ResourceUri& baseUri,
        StringView familyName,
        Aero::FontWeight& weight,
        Aero::FontStretch& stretch,
        Aero::FontStyle& style) const noexcept = 0;

    virtual bool FamilyExists(
        const Base::ResourceUri& baseUri,
        StringView familyName) const noexcept = 0;

    virtual void GetFontFamilies(
        const Base::ResourceUri&,
        Base::Vector<String>& families) const noexcept {
        families.Clear();
    }

    void AddChangedHandler(
        const FontProviderChangedHandler& handler) noexcept {
        changed_.Add(handler);
    }
    bool RemoveChangedHandler(
        const FontProviderChangedHandler& handler) noexcept {
        return changed_.Remove(handler);
    }

protected:
    // Must be raised on the owning Gui's dispatcher thread. Empty fields
    // invalidate the complete host-font cache.
    void RaiseChanged(const FontProviderChange& change = {}) noexcept {
        if (changed_) changed_(change);
    }

private:
    FontProviderChangedHandler changed_;
};

} // namespace Aero::Media
