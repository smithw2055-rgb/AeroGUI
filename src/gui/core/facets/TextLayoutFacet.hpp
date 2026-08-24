#pragma once

#include "gui/core/Facet.hpp"
#include <Aero/Visual.hpp>

namespace Aero {
class TextBlockLayout;
namespace Controls {
class TextBlock;
class TextBox;
class PasswordBox;
} // namespace Controls
} // namespace Aero

namespace Aero::Core {

// Typography, text shaping & glyph runs facet
class TextLayoutFacet : public Facet {
public:
    static constexpr FacetType StaticType = FacetType::Text;

    explicit TextLayoutFacet(::Aero::Media::Visual& owner) noexcept : owner_(&owner) {}

    ::Aero::Media::Visual& Owner() const noexcept { return *owner_; }

    static void* TextLayoutRuntime(const ::Aero::Media::Visual& visual) noexcept;

    template<class TRuntime = void>
    static TRuntime* TypedTextLayoutRuntime(const ::Aero::Media::Visual& visual) noexcept {
        return static_cast<TRuntime*>(TextLayoutRuntime(visual));
    }

    static void AttachTextLayout(
        Aero::Controls::TextBlock& element,
        void* service,
        bool invalidate = false) noexcept;
    static void AttachTextLayout(
        Aero::Controls::TextBox& element,
        void* service,
        bool invalidate = false) noexcept;
    static void AttachTextLayout(
        Aero::Controls::PasswordBox& element,
        void* service,
        bool invalidate = false) noexcept;

private:
    ::Aero::Media::Visual* owner_ = nullptr;
};

template<>
struct FacetTrait<TextLayoutFacet> {
    static constexpr std::uint32_t Id = static_cast<std::uint32_t>(FacetId::Text);
    static constexpr FacetType Type = FacetType::Text;
};

} // namespace Aero::Core
