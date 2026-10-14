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

class AERO_GUI_API NameScope {
public:
    NameScope() noexcept;

    NameScope(NameScope&&) noexcept = default;
    NameScope& operator=(NameScope&&) noexcept = default;

    NameScope(const NameScope&) = delete;
    NameScope& operator=(const NameScope&) = delete;

    Result<void> Register(StringView name, Base::Object& object) noexcept;
    Base::Object* Find(StringView name) const noexcept;
    StringView NameOf(const Base::Object& object) const noexcept;
    void Clear() noexcept;

    std::uint32_t Size() const noexcept { return entries_.Size(); }
    static bool IsValidName(StringView name) noexcept;

private:
    struct Entry {
        String name;
        Base::Object* object = nullptr;
    };

    Base::Vector<Entry> entries_;
};

enum class ResourceKeyKind : std::uint8_t {
    Invalid = 0U,
    String,
    Type
};

class AERO_GUI_API ResourceKey {
public:
    ResourceKey() noexcept = default;

    static Result<ResourceKey> FromString(StringView value) noexcept;
    static ResourceKey FromType(Meta::TypeId value) noexcept;

    ResourceKeyKind Kind() const noexcept { return kind_; }
    bool IsValid() const noexcept;
    StringView StringValue() const noexcept { return string_.View(); }
    Meta::TypeId TypeValue() const noexcept { return type_; }

private:
    ResourceKeyKind kind_ = ResourceKeyKind::Invalid;
    String string_;
    Meta::TypeId type_ = Meta::InvalidTypeId;
};

AERO_GUI_API bool operator==(const ResourceKey& left, const ResourceKey& right) noexcept;
inline bool operator!=(const ResourceKey& left, const ResourceKey& right) noexcept { return !(left == right); }

using ResourceValue = Value;

struct ResourceEntrySnapshot {
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

struct ResourceChangeSubscription {
    std::uint64_t value = 0U;

    constexpr bool IsValid() const noexcept { return value != 0U; }
};

// StringView is empty for type-keyed and dictionary-wide notifications.
using ResourceChangedCallback = void (*)(void* context, StringView key, ResourceChangeKind kind,
    std::uint64_t generation) noexcept;

// Move-stable resource table. Its heap-backed state lets dictionaries be moved
// between load sessions and runtime owners without invalidating subscriptions.
class AERO_GUI_API ResourceDictionary : public Base::Object {
    AERO_DECLARE_TYPE(ResourceDictionary, Base::Object)

public:
    // Incomplete nested storage (definition TU-local). Public only as a
    // forward name so engine helpers can mention the type; the pointer
    // state_ stays private.
    struct DictionaryState;

    ResourceDictionary() noexcept;
    ~ResourceDictionary() noexcept;
    ResourceDictionary(ResourceDictionary&& other) noexcept;
    ResourceDictionary& operator=(ResourceDictionary&& other) noexcept;
    ResourceDictionary(const ResourceDictionary&) = delete;
    ResourceDictionary& operator=(const ResourceDictionary&) = delete;

    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Result<void> Add(const ResourceKey& key, const ResourceValue& value,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    Result<void> Add(StringView key, const ResourceValue& value, ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    Result<void> Add(Meta::TypeId key, const ResourceValue& value,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    Result<void> Add(StringView key, Meta::TypeId type, const Ref<Base::Object>& object,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    // Soft-fail like Set: false on missing/invalid/sealed.
    bool Remove(const ResourceKey& key) noexcept;
    bool Remove(StringView key) noexcept;
    bool Remove(Meta::TypeId key) noexcept;
    // Try-style lookup: empty/Unset value when missing (not Result like FindResource).
    ResourceValue Lookup(const ResourceKey& key) const noexcept;
    ResourceValue Lookup(StringView key) const noexcept;
    ResourceValue Lookup(Meta::TypeId key) const noexcept;
    bool Contains(const ResourceKey& key) const noexcept;
    bool Contains(StringView key) const noexcept;
    bool Contains(Meta::TypeId key) const noexcept;
    ::Aero::Diagnostics::SourceSpan SourceOf(const ResourceKey& key) const noexcept;
    ::Aero::Diagnostics::SourceSpan SourceOf(StringView key) const noexcept;
    Result<void> AddMerged(ResourceDictionary& dictionary) noexcept;
    Result<bool> RemoveMerged(ResourceDictionary& dictionary) noexcept;
    std::uint32_t MergedDictionaryCount() const noexcept;
    // Returns a move-only shared view over the merged dictionary's stable
    // backing store. Mutations through the view affect the merged dictionary.
    Result<ResourceDictionary> MergedDictionaryAt(std::uint32_t index) const noexcept;
    // Explicitly retains the stable backing store without making the public
    // dictionary type implicitly copyable.
    Result<ResourceDictionary> Share() const noexcept;
    Result<void> Seal() noexcept;
    Result<ResourceChangeSubscription> SubscribeChanged(ResourceChangedCallback callback, void* context) noexcept;
    bool Unsubscribe(ResourceChangeSubscription subscription) noexcept;
    std::uint32_t Size() const noexcept;
    Result<ResourceEntrySnapshot> EntryAt(std::uint32_t index) const noexcept;
    std::uint64_t Generation() const noexcept;

    bool Set(const ResourceKey& key, const ResourceValue& value, ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    bool Set(StringView key, const ResourceValue& value, ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    bool Set(Meta::TypeId key, const ResourceValue& value, ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    bool Set(StringView key, Meta::TypeId type, const Ref<Base::Object>& object,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    void Clear() noexcept;
    void ClearMergedDictionaries() noexcept;
    const Base::ResourceUri& GetSource() const noexcept;
    void SetSource(const Base::ResourceUri& source) noexcept;
    bool GetIsSealed() const noexcept;

private:
    friend struct DictionaryState;

    explicit ResourceDictionary(DictionaryState* state, bool addReference) noexcept;

    Result<void> StoreResource(const ResourceKey& key, const ResourceValue& value,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    Result<void> StoreResource(StringView key, const ResourceValue& value,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    Result<void> StoreResource(Meta::TypeId key, const ResourceValue& value,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    Result<void> StoreResource(StringView key, Meta::TypeId type, const Ref<Base::Object>& object,
        ::Aero::Diagnostics::SourceSpan source = {}) noexcept;
    Result<DictionaryState*> EnsureState() noexcept;
    static void AddStateRef(DictionaryState* state) noexcept;
    static void ReleaseState(DictionaryState* state) noexcept;

    DictionaryState* state_ = nullptr;
};

struct ResourceEnvironment { const ResourceDictionary* application = nullptr;
    const ResourceDictionary* theme = nullptr;
    const ResourceDictionary* system = nullptr;
};
} // namespace Aero
