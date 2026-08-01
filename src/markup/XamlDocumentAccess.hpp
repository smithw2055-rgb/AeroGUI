#pragma once

#include <Aero/Markup/XamlDocument.hpp>

#include "markup/LoaderResult.hpp"

namespace Aero::Detail {

class UiDocumentAccess final {
public:
    static Base::Result<UiDocument> Adopt(
        Markup::LoaderResult&& result,
        Base::IAllocator& allocator) noexcept;
    static Markup::LoaderResult Take(
        UiDocument& document) noexcept;
    static const Markup::EffectLifetime* RuntimeLifetime(
        const UiDocument& document) noexcept;
};

} // namespace Aero::Detail
