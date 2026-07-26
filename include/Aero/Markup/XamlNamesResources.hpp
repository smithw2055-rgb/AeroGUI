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

#include <cstdint>

namespace Aero::Markup {

class XamlObjectWriter;

enum class ResourceChangeKind : std::uint8_t {
    Added = 0U,
    Replaced,
    Removed,
    Cleared
};

struct ResourceChangeSubscription final {
    std::uint64_t value = 0U;

    constexpr bool IsValid() const noexcept {
        return value != 0U;
    }
};

using ResourceChangedCallback = void (*)(
    void* context,
    Base::StringView key,
    ResourceChangeKind kind,
    std::uint64_t generation) noexcept;

inline constexpr Base::StringView
XamlLanguageNamespaceUri() noexcept {
    return Base::StringView(
        "http://schemas.microsoft.com/winfx/2006/xaml");
}

class AERO_API NameScope final {
public:
    NameScope() noexcept;

    NameScope(NameScope&&) noexcept = default;
    NameScope& operator=(NameScope&&) noexcept = default;

    NameScope(const NameScope&) = delete;
    NameScope& operator=(const NameScope&) = delete;

    Base::Result<void> TryRegister(
        Base::StringView name,
        Base::Object& object) noexcept;
    Base::Object* Find(
        Base::StringView name) const noexcept;
    Base::StringView NameOf(
        const Base::Object& object) const noexcept;

    void Clear() noexcept;

    std::uint32_t Size() const noexcept {
        return entries_.Size();
    }
    static bool IsValidName(
        Base::StringView name) noexcept;

private:
    struct Entry final {
        Entry() noexcept = default;

        Entry(Entry&&) noexcept = default;
        Entry& operator=(Entry&&) noexcept = default;

        Entry(const Entry&) = delete;
        Entry& operator=(const Entry&) = delete;

        Base::String name;
        Base::Object* object = nullptr;
    };

    Base::Vector<Entry> entries_;
};

using XamlResourceValue = Core::Value;

class AERO_API ResourceDictionary final {
public:
    ResourceDictionary() noexcept;

    ResourceDictionary(ResourceDictionary&&) noexcept = default;
    ResourceDictionary& operator=(ResourceDictionary&&) noexcept = default;

    ResourceDictionary(const ResourceDictionary&) = delete;
    ResourceDictionary& operator=(const ResourceDictionary&) = delete;

    Base::Result<void> TryAdd(
        Base::StringView key,
        const XamlResourceValue& value,
        Core::SourceSpan source = {}) noexcept;
    Base::Result<void> TryAdd(
        Base::StringView key,
        Core::TypeId type,
        const Base::Ref<Base::Object>& object,
        Core::SourceSpan source = {}) noexcept;
    // Adds a missing key or atomically replaces its value. Resource changes
    // notify DynamicResource expressions after the dictionary state commits.
    Base::Result<void> TrySet(
        Base::StringView key,
        const XamlResourceValue& value,
        Core::SourceSpan source = {}) noexcept;
    Base::Result<void> TrySet(
        Base::StringView key,
        Core::TypeId type,
        const Base::Ref<Base::Object>& object,
        Core::SourceSpan source = {}) noexcept;
    Base::Result<bool> Remove(
        Base::StringView key) noexcept;
    Base::Result<XamlResourceValue> Lookup(
        Base::StringView key) const noexcept;
    bool Contains(Base::StringView key) const noexcept;
    Core::SourceSpan SourceOf(
        Base::StringView key) const noexcept;

    Base::Result<ResourceChangeSubscription> SubscribeChanged(
        ResourceChangedCallback callback,
        void* context) noexcept;
    bool Unsubscribe(
        ResourceChangeSubscription subscription) noexcept;

    void Clear() noexcept;

    std::uint32_t Size() const noexcept {
        return entries_.Size();
    }
    std::uint64_t Generation() const noexcept {
        return generation_;
    }
private:
    struct Entry final {
        Entry() noexcept = default;

        Entry(Entry&&) noexcept = default;
        Entry& operator=(Entry&&) noexcept = default;

        Entry(const Entry&) = delete;
        Entry& operator=(const Entry&) = delete;

        Base::String key;
        XamlResourceValue value;
        Core::SourceSpan source;
    };

    struct Listener final {
        ResourceChangeSubscription subscription;
        ResourceChangedCallback callback = nullptr;
        void* context = nullptr;
    };

    const Entry* FindEntry(
        Base::StringView key) const noexcept;

    Base::Vector<Entry> entries_;
    Base::Vector<Listener> listeners_;
    std::uint64_t generation_ = 0U;
    std::uint64_t nextSubscription_ = 1U;

    void NotifyChanged(
        Base::StringView key,
        ResourceChangeKind kind) noexcept;
};

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

    XamlResourceResolver(
        LookupCallback lookup,
        void* context) noexcept
        : lookup_(lookup), context_(context) {}

    LookupCallback lookup_ = nullptr;
    void* context_ = nullptr;
};

} // namespace Aero::Markup
