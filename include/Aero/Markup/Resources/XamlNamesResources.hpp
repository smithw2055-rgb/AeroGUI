#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/Metadata/MetadataId.hpp>
#include <Aero/Core/Metadata/Value.hpp>
#include <Aero/Presentation/Resources.hpp>

#include <cstdint>

namespace Aero::Markup {

class XamlObjectWriter;
class XamlLoadSession;

// Compatibility names while public callers migrate to Presentation ownership.
using NameScope = Presentation::NameScope;
using ResourceKey = Presentation::ResourceKey;
using ResourceKeyKind = Presentation::ResourceKeyKind;
using ResourceChangeKind = Presentation::ResourceChangeKind;
using ResourceChangeSubscription =
    Presentation::ResourceChangeSubscription;
using ResourceChangedCallback =
    Presentation::ResourceChangedCallback;
using ResourceDictionary =
    Presentation::ResourceDictionary;
using XamlResourceValue = Presentation::ResourceValue;

inline constexpr Base::StringView
XamlLanguageNamespaceUri() noexcept {
    return Base::StringView(
        "http://schemas.microsoft.com/winfx/2006/xaml");
}

class AERO_API XamlNamespaceScope final {
public:
    using LookupCallback = Base::Result<Base::StringView> (*)(
        void* context,
        Base::StringView prefix) noexcept;

    XamlNamespaceScope() noexcept = default;

    Base::Result<Base::StringView> Lookup(
        Base::StringView prefix) const noexcept;
    bool IsAvailable() const noexcept {
        return lookup_ != nullptr;
    }

private:
    friend class XamlObjectWriter;
    friend class XamlLoadSession;

    XamlNamespaceScope(
        LookupCallback lookup,
        void* context) noexcept
        : lookup_(lookup), context_(context) {}

    LookupCallback lookup_ = nullptr;
    void* context_ = nullptr;
};

class AERO_API XamlResourceResolver final {
public:
    using LookupCallback = Base::Result<XamlResourceValue> (*)(
        void* context,
        Base::StringView key) noexcept;

    XamlResourceResolver() noexcept = default;

    Base::Result<XamlResourceValue> Lookup(
        Base::StringView key) const noexcept;
    bool IsAvailable() const noexcept {
        return lookup_ != nullptr;
    }

private:
    friend class XamlObjectWriter;
    friend class XamlLoadSession;

    XamlResourceResolver(
        LookupCallback lookup,
        void* context) noexcept
        : lookup_(lookup), context_(context) {}

    LookupCallback lookup_ = nullptr;
    void* context_ = nullptr;
};

} // namespace Aero::Markup
