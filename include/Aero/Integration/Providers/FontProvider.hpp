#pragma once

#include <Aero/Base/Config.hpp>
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

namespace Aero::Integration {

struct FontSource {
    Base::ResourceUri uri;
    Base::Ref<Base::Stream> stream;
    std::uint32_t faceIndex = 0U;
};

class AERO_API FontProvider {
public:
    virtual ~FontProvider() = default;

    virtual Base::Result<FontSource> MatchFont(
        const Base::ResourceUri& baseUri,
        Base::StringView familyName,
        Aero::FontWeight& weight,
        Aero::FontStretch& stretch,
        Aero::FontStyle& style) const noexcept = 0;

    virtual bool FamilyExists(
        const Base::ResourceUri& baseUri,
        Base::StringView familyName) const noexcept = 0;

    virtual void GetFontFamilies(
        const Base::ResourceUri&,
        Base::Vector<Base::String>& families) const noexcept {
        families.Clear();
    }
};

} // namespace Aero::Integration

