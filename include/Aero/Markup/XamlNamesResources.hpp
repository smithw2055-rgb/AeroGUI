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
#include <Aero/Core/TypeRegistry.hpp>

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

    AERO_NODISCARD constexpr bool IsValid() const noexcept {
        return value != 0U;
    }
};

using ResourceChangedCallback = void (*)(
    void* context,
    Base::StringView key,
    ResourceChangeKind kind,
    std::uint64_t generation) noexcept;

AERO_NODISCARD inline constexpr Base::StringView
XamlLanguageNamespaceUri() noexcept {
    return Base::StringView(
        "http://schemas.microsoft.com/winfx/2006/xaml");
}

class AERO_API NameScope final {
public:
    explicit NameScope(Base::IAllocator* allocator = nullptr) noexcept;

    NameScope(NameScope&&) noexcept = default;
    NameScope& operator=(NameScope&&) noexcept = default;

    NameScope(const NameScope&) = delete;
    NameScope& operator=(const NameScope&) = delete;

    AERO_NODISCARD Base::Result<void> TryRegister(
        Base::StringView name,
        Base::Object& object) noexcept;
    AERO_NODISCARD Base::Object* Find(
        Base::StringView name) const noexcept;

    void Clear() noexcept;

    AERO_NODISCARD std::uint32_t Size() const noexcept {
        return entries_.Size();
    }
    AERO_NODISCARD Base::IAllocator& Allocator() const noexcept {
        return *allocator_;
    }

    AERO_NODISCARD static bool IsValidName(
        Base::StringView name) noexcept;

private:
    struct Entry final {
        explicit Entry(Base::IAllocator* allocator) noexcept
            : name(allocator) {}

        Entry(Entry&&) noexcept = default;
        Entry& operator=(Entry&&) noexcept = default;

        Entry(const Entry&) = delete;
        Entry& operator=(const Entry&) = delete;

        Base::String name;
        Base::Object* object = nullptr;
    };

    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<Entry> entries_;
};

struct XamlResourceValue final {
    Core::TypeId type = Core::InvalidTypeId;
    Base::Ref<Base::Object> object;

    AERO_NODISCARD bool IsValid() const noexcept {
        return type != Core::InvalidTypeId && static_cast<bool>(object);
    }
};

class AERO_API ResourceDictionary final {
public:
    explicit ResourceDictionary(
        Base::IAllocator* allocator = nullptr) noexcept;

    ResourceDictionary(ResourceDictionary&&) noexcept = default;
    ResourceDictionary& operator=(ResourceDictionary&&) noexcept = default;

    ResourceDictionary(const ResourceDictionary&) = delete;
    ResourceDictionary& operator=(const ResourceDictionary&) = delete;

    AERO_NODISCARD Base::Result<void> TryAdd(
        Base::StringView key,
        Core::TypeId type,
        const Base::Ref<Base::Object>& object,
        Core::SourceSpan source = {}) noexcept;
    // Adds a missing key or atomically replaces its value. Resource changes
    // notify DynamicResource expressions after the dictionary state commits.
    AERO_NODISCARD Base::Result<void> TrySet(
        Base::StringView key,
        Core::TypeId type,
        const Base::Ref<Base::Object>& object,
        Core::SourceSpan source = {}) noexcept;
    AERO_NODISCARD Base::Result<bool> Remove(
        Base::StringView key) noexcept;
    AERO_NODISCARD Base::Result<XamlResourceValue> Lookup(
        Base::StringView key) const noexcept;
    AERO_NODISCARD bool Contains(Base::StringView key) const noexcept;
    AERO_NODISCARD Core::SourceSpan SourceOf(
        Base::StringView key) const noexcept;

    AERO_NODISCARD Base::Result<ResourceChangeSubscription> SubscribeChanged(
        ResourceChangedCallback callback,
        void* context) noexcept;
    AERO_NODISCARD bool Unsubscribe(
        ResourceChangeSubscription subscription) noexcept;

    void Clear() noexcept;

    AERO_NODISCARD std::uint32_t Size() const noexcept {
        return entries_.Size();
    }
    AERO_NODISCARD std::uint64_t Generation() const noexcept {
        return generation_;
    }
    AERO_NODISCARD Base::IAllocator& Allocator() const noexcept {
        return *allocator_;
    }

private:
    struct Entry final {
        explicit Entry(Base::IAllocator* allocator) noexcept
            : key(allocator) {}

        Entry(Entry&&) noexcept = default;
        Entry& operator=(Entry&&) noexcept = default;

        Entry(const Entry&) = delete;
        Entry& operator=(const Entry&) = delete;

        Base::String key;
        Core::TypeId type = Core::InvalidTypeId;
        Base::Ref<Base::Object> object;
        Core::SourceSpan source;
    };

    struct Listener final {
        ResourceChangeSubscription subscription;
        ResourceChangedCallback callback = nullptr;
        void* context = nullptr;
    };

    AERO_NODISCARD const Entry* FindEntry(
        Base::StringView key) const noexcept;

    Base::IAllocator* allocator_ = nullptr;
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

    AERO_NODISCARD Base::Result<Base::StringView> Lookup(
        Base::StringView prefix) const noexcept;
    AERO_NODISCARD bool IsAvailable() const noexcept {
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

    AERO_NODISCARD Base::Result<XamlResourceValue> Lookup(
        Base::StringView key) const noexcept;
    AERO_NODISCARD bool IsAvailable() const noexcept {
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
