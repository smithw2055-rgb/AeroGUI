#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Value.hpp>

namespace Aero::Markup {

inline constexpr StringView LanguageNamespaceUri() noexcept {
    return StringView(
        "http://schemas.microsoft.com/winfx/2006/xaml");
}

class AERO_GUI_API NamespaceScope {
public:
    using LookupCallback = Result<StringView> (*)(
        void* context,
        StringView prefix) noexcept;

    NamespaceScope() noexcept = default;
    NamespaceScope(LookupCallback lookup, void* context) noexcept
        : lookup_(lookup), context_(context) {}

    Result<StringView> Lookup(
        StringView prefix) const noexcept;
    bool IsAvailable() const noexcept { return lookup_ != nullptr; }

private:
    LookupCallback lookup_ = nullptr;
    void* context_ = nullptr;
};

class AERO_GUI_API ResourceResolver {
public:
    using LookupCallback = Result<Aero::Value> (*)(
        void* context,
        StringView key) noexcept;

    ResourceResolver() noexcept = default;
    ResourceResolver(LookupCallback lookup, void* context) noexcept
        : lookup_(lookup), context_(context) {}

    Result<Aero::Value> Lookup(
        StringView key) const noexcept;
    bool IsAvailable() const noexcept { return lookup_ != nullptr; }

private:
    LookupCallback lookup_ = nullptr;
    void* context_ = nullptr;
};

} // namespace Aero::Markup
