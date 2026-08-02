#pragma once

#include "Providers.hpp"

namespace Aero::Text {

class FreeTypeAdapter;

// Shapes faces owned by a FreeTypeAdapter without exposing HarfBuzz or
// FreeType handles through Aero's public ABI.
class AERO_API HarfBuzzAdapter  : public ITextShaper {
public:
    explicit HarfBuzzAdapter(
        FreeTypeAdapter& fonts) noexcept
        : fonts_(&fonts) {}

    bool Supports(
        FontProviderIdentity provider) const noexcept override;
    Base::Result<void> Shape(
        const ShapingRequest& request,
        ShapedTextRun& output) noexcept override;

private:
    FreeTypeAdapter* fonts_ = nullptr;
};

} // namespace Aero::Text
