#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero {

class FrameworkElement;

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
        Base::String name;
        Base::Object* object = nullptr;
    };

    Base::Vector<Entry> entries_;
};

enum class ResourceKeyKind : std::uint8_t {
    Invalid = 0U,
    String,
    Type
};

class AERO_API ResourceKey final {
public:
    ResourceKey() noexcept = default;

    static Base::Result<ResourceKey> FromString(
        Base::StringView value) noexcept;
    static ResourceKey FromType(
        Meta::TypeId value) noexcept;

    ResourceKeyKind Kind() const noexcept {
        return kind_;
    }
    bool IsValid() const noexcept;
    Base::StringView StringValue() const noexcept {
        return string_.View();
    }
    Meta::TypeId TypeValue() const noexcept {
        return type_;
    }

private:
    ResourceKeyKind kind_ = ResourceKeyKind::Invalid;
    Base::String string_;
    Meta::TypeId type_ = Meta::InvalidTypeId;
};

AERO_API bool operator==(
    const ResourceKey& left,
    const ResourceKey& right) noexcept;
inline bool operator!=(
    const ResourceKey& left,
    const ResourceKey& right) noexcept {
    return !(left == right);
}

using ResourceValue = Meta::Value;

struct ResourceEntrySnapshot final {
    ResourceKey key;
    ResourceValue value;
    ::Aero::Diagnostics::SourceSpan source;
};

enum class ResourceChangeKind : std::uint8_t {
    Added = 0U,
    Replaced,
    Removed,
    Cleared,
    MergedDictionaryChanged,
    SourceChanged,
    Sealed
};

struct ResourceChangeSubscription final {
    std::uint64_t value = 0U;

    constexpr bool IsValid() const noexcept {
        return value != 0U;
    }
};

// StringView is empty for type-keyed and dictionary-wide notifications.
using ResourceChangedCallback = void (*)(
    void* context,
    Base::StringView key,
    ResourceChangeKind kind,
    std::uint64_t generation) noexcept;

// Move-stable resource table. Its heap-backed state lets dictionaries be moved
// between load sessions and runtime owners without invalidating subscriptions.
class AERO_API ResourceDictionary final
    : public Base::Object {
    AERO_DECLARE_TYPE(
        ResourceDictionary,
        Base::Object)
public:
    struct Impl;

    ResourceDictionary() noexcept;
    ~ResourceDictionary() noexcept;

    ResourceDictionary(ResourceDictionary&& other) noexcept;
    ResourceDictionary& operator=(
        ResourceDictionary&& other) noexcept;

    ResourceDictionary(const ResourceDictionary&) = delete;
    ResourceDictionary& operator=(const ResourceDictionary&) = delete;

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::Result<void> TryAdd(
        const ResourceKey& key,
        const ResourceValue& value,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    Base::Result<void> TryAdd(
        Base::StringView key,
        const ResourceValue& value,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    Base::Result<void> TryAdd(
        Meta::TypeId key,
        const ResourceValue& value,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    Base::Result<void> TryAdd(
        Base::StringView key,
        Meta::TypeId type,
        const Base::Ref<Base::Object>& object,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;

    Base::Result<void> TrySet(
        const ResourceKey& key,
        const ResourceValue& value,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    Base::Result<void> TrySet(
        Base::StringView key,
        const ResourceValue& value,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    Base::Result<void> TrySet(
        Meta::TypeId key,
        const ResourceValue& value,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    Base::Result<void> TrySet(
        Base::StringView key,
        Meta::TypeId type,
        const Base::Ref<Base::Object>& object,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;

    Base::Result<bool> Remove(
        const ResourceKey& key) noexcept;
    Base::Result<bool> Remove(
        Base::StringView key) noexcept;
    Base::Result<bool> Remove(
        Meta::TypeId key) noexcept;

    Base::Result<ResourceValue> Lookup(
        const ResourceKey& key) const noexcept;
    Base::Result<ResourceValue> Lookup(
        Base::StringView key) const noexcept;
    Base::Result<ResourceValue> Lookup(
        Meta::TypeId key) const noexcept;
    bool Contains(const ResourceKey& key) const noexcept;
    bool Contains(Base::StringView key) const noexcept;
    bool Contains(Meta::TypeId key) const noexcept;
    ::Aero::Diagnostics::SourceSpan SourceOf(
        const ResourceKey& key) const noexcept;
    ::Aero::Diagnostics::SourceSpan SourceOf(
        Base::StringView key) const noexcept;

    Base::Result<void> TryAddMerged(
        ResourceDictionary& dictionary) noexcept;
    Base::Result<bool> RemoveMerged(
        ResourceDictionary& dictionary) noexcept;
    void ClearMergedDictionaries() noexcept;
    std::uint32_t MergedDictionaryCount() const noexcept;
    // Returns a move-only shared view over the merged dictionary's stable
    // backing store. Mutations through the view affect the merged dictionary.
    Base::Result<ResourceDictionary> MergedDictionaryAt(
        std::uint32_t index) const noexcept;
    // Explicitly retains the stable backing store without making the public
    // dictionary type implicitly copyable.
    Base::Result<ResourceDictionary> Share() const noexcept;

    void SetSource(
        const Base::ResourceUri& source) noexcept;
    const Base::ResourceUri& GetSource() const noexcept;

    Base::Result<void> Seal() noexcept;
    bool GetIsSealed() const noexcept;

    Base::Result<ResourceChangeSubscription> SubscribeChanged(
        ResourceChangedCallback callback,
        void* context) noexcept;
    bool Unsubscribe(
        ResourceChangeSubscription subscription) noexcept;

    void Clear() noexcept;
    std::uint32_t Size() const noexcept;
    Base::Result<ResourceEntrySnapshot> EntryAt(
        std::uint32_t index) const noexcept;
    std::uint64_t Generation() const noexcept;

private:
    explicit ResourceDictionary(
        Impl* impl,
        bool addReference) noexcept;

    Impl* impl_ = nullptr;

    Base::Result<Impl*> EnsureImpl() noexcept;
    static void AddImplRef(Impl* impl) noexcept;
    static void ReleaseImpl(Impl* impl) noexcept;
};

struct ResourceEnvironment final {
    const ResourceDictionary* application = nullptr;
    const ResourceDictionary* theme = nullptr;
    const ResourceDictionary* system = nullptr;
};

class AERO_API ResourceResolver final {
public:
    static Base::Result<ResourceValue> Lookup(
        const FrameworkElement* element,
        const ResourceKey& key,
        const ResourceDictionary* templateResources,
        const ResourceEnvironment& environment) noexcept;
    static Base::Result<ResourceValue> Lookup(
        const FrameworkElement* element,
        Base::StringView key,
        const ResourceDictionary* templateResources,
        const ResourceEnvironment& environment) noexcept;
    static Base::Result<ResourceValue> Lookup(
        const FrameworkElement* element,
        Meta::TypeId key,
        const ResourceDictionary* templateResources,
        const ResourceEnvironment& environment) noexcept;
};

} // namespace Aero
