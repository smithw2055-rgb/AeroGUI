#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Styling.hpp>

namespace Aero::Markup {

namespace Detail {
class ResourcePrivate;
}

inline constexpr Base::StringView
LanguageNamespaceUri() noexcept {
    return Base::StringView(
        "http://schemas.microsoft.com/winfx/2006/xaml");
}

class AERO_API NamespaceScope final {
public:
    using LookupCallback = Base::Result<Base::StringView> (*)(
        void* context,
        Base::StringView prefix) noexcept;

    NamespaceScope() noexcept = default;

    Base::Result<Base::StringView> Lookup(
        Base::StringView prefix) const noexcept;
    bool IsAvailable() const noexcept {
        return lookup_ != nullptr;
    }

private:
    friend class Detail::ResourcePrivate;

    NamespaceScope(
        LookupCallback lookup,
        void* context) noexcept
        : lookup_(lookup), context_(context) {}

    LookupCallback lookup_ = nullptr;
    void* context_ = nullptr;
};

class AERO_API ResourceResolver final {
public:
    using LookupCallback =
        Base::Result<Aero::ResourceValue> (*)(
        void* context,
        Base::StringView key) noexcept;

    ResourceResolver() noexcept = default;

    Base::Result<Aero::ResourceValue> Lookup(
        Base::StringView key) const noexcept;
    bool IsAvailable() const noexcept {
        return lookup_ != nullptr;
    }

private:
    friend class Detail::ResourcePrivate;

    ResourceResolver(
        LookupCallback lookup,
        void* context) noexcept
        : lookup_(lookup), context_(context) {}

    LookupCallback lookup_ = nullptr;
    void* context_ = nullptr;
};

} // namespace Aero::Markup
