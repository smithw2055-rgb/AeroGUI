#include <Aero/Resources.hpp>

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/FrameworkElement.hpp>

#include <cstdio>
#include <new>
#include <utility>

namespace Aero {
namespace {

constexpr const char* MessageInvalidName =
    "XAML name must be a non-empty identifier without a namespace prefix";
constexpr const char* MessageDuplicateName =
    "XAML name is already registered in this name scope";
constexpr const char* MessageInvalidResource =
    "ResourceDictionary requires a valid key and typed value";
constexpr const char* MessageDuplicateResource =
    "ResourceDictionary key is already present";
constexpr const char* MessageResourceNotFound =
    "ResourceDictionary key was not found";
constexpr const char* MessageReadOnly =
    "ResourceDictionary is sealed";

bool IsAsciiLetter(char value) noexcept {
    return (value >= 'A' && value <= 'Z') ||
        (value >= 'a' && value <= 'z');
}

bool IsNameStart(unsigned char value) noexcept {
    return IsAsciiLetter(static_cast<char>(value)) ||
        value == static_cast<unsigned char>('_') ||
        value >= 0x80U;
}

bool IsNameContinue(unsigned char value) noexcept {
    return IsNameStart(value) ||
        (value >= static_cast<unsigned char>('0') &&
         value <= static_cast<unsigned char>('9'));
}

Base::StringView CallbackKey(
    const ResourceKey& key) noexcept {
    return key.Kind() == ResourceKeyKind::String
        ? key.StringValue()
        : Base::StringView{};
}

} // namespace

NameScope::NameScope() noexcept = default;

Base::Result<void> NameScope::Register(
    Base::StringView name,
    Base::Object& object) noexcept {
    if (!IsValidName(name)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidName);
    }
    if (Find(name) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            MessageDuplicateName);
    }
    Entry entry;
    Base::Result<void> assigned =
        entry.name.Assign(name);
    if (!assigned) {
        return assigned.GetStatus();
    }
    entry.object = &object;
    return entries_.PushBack(std::move(entry));
}

Base::Object* NameScope::Find(
    Base::StringView name) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.name.View() == name) {
            return entry.object;
        }
    }
    return nullptr;
}

Base::StringView NameScope::NameOf(
    const Base::Object& object) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.object == &object) {
            return entry.name.View();
        }
    }
    return {};
}

void NameScope::Clear() noexcept {
    entries_.Clear();
}

bool NameScope::IsValidName(
    Base::StringView name) noexcept {
    if (name.Empty() || name.Data() == nullptr) {
        return false;
    }
    const auto* bytes =
        reinterpret_cast<const unsigned char*>(name.Data());
    if (!IsNameStart(bytes[0])) {
        return false;
    }
    for (std::uint32_t index = 1U;
         index < name.SizeBytes();
         ++index) {
        if (!IsNameContinue(bytes[index])) {
            return false;
        }
    }
    return true;
}

Base::Result<ResourceKey> ResourceKey::FromString(
    Base::StringView value) noexcept {
    if (value.Empty() || value.Data() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "String resource key cannot be empty");
    }
    ResourceKey key;
    Base::Result<void> assigned =
        key.string_.Assign(value);
    if (!assigned) {
        return assigned.GetStatus();
    }
    key.kind_ = ResourceKeyKind::String;
    return key;
}

ResourceKey ResourceKey::FromType(
    Meta::TypeId value) noexcept {
    ResourceKey key;
    if (value != Meta::InvalidTypeId) {
        key.kind_ = ResourceKeyKind::Type;
        key.type_ = value;
    }
    return key;
}

bool ResourceKey::IsValid() const noexcept {
    return (kind_ == ResourceKeyKind::String &&
            !string_.Empty()) ||
        (kind_ == ResourceKeyKind::Type &&
         type_ != Meta::InvalidTypeId);
}

bool operator==(
    const ResourceKey& left,
    const ResourceKey& right) noexcept {
    if (left.Kind() != right.Kind()) {
        return false;
    }
    switch (left.Kind()) {
    case ResourceKeyKind::String:
        return left.StringValue() == right.StringValue();
    case ResourceKeyKind::Type:
        return left.TypeValue() == right.TypeValue();
    case ResourceKeyKind::Invalid:
        return true;
    }
    return false;
}

struct ResourceDictionary::DictionaryState {
    struct Entry {
        ResourceKey key;
        ResourceValue value;
        ::Aero::Diagnostics::SourceSpan source;
    };

    struct Listener {
        ResourceChangeSubscription subscription;
        ResourceChangedCallback callback = nullptr;
        void* context = nullptr;
    };

    struct Merged {
        ResourceDictionary::DictionaryState* dictionary = nullptr;
        ResourceChangeSubscription subscription;
    };

    Base::Vector<Entry> entries;
    Base::Vector<Listener> listeners;
    Base::Vector<Merged> merged;
    Base::ResourceUri source;
    std::uint64_t generation = 0U;
    std::uint64_t nextSubscription = 1U;
    std::uint32_t references = 1U;
    bool sealed = false;
};

using Access = ResourceDictionary::DictionaryState;

namespace {

ResourceDictionary::DictionaryState::Entry* FindLocal(
    ResourceDictionary::DictionaryState& impl,
    const ResourceKey& key) noexcept {
    for (ResourceDictionary::DictionaryState::Entry& entry :
         impl.entries) {
        if (entry.key == key) {
            return &entry;
        }
    }
    return nullptr;
}

const ResourceDictionary::DictionaryState::Entry* FindLocal(
    const ResourceDictionary::DictionaryState& impl,
    const ResourceKey& key) noexcept {
    for (const ResourceDictionary::DictionaryState::Entry& entry :
         impl.entries) {
        if (entry.key == key) {
            return &entry;
        }
    }
    return nullptr;
}

void Notify(
    ResourceDictionary::DictionaryState& impl,
    Base::StringView key,
    ResourceChangeKind kind) noexcept {
    if (impl.generation != UINT64_MAX) {
        ++impl.generation;
    }
    const std::uint64_t boundary =
        impl.nextSubscription - 1U;
    std::uint32_t index = 0U;
    while (index < impl.listeners.Size()) {
        const ResourceDictionary::DictionaryState::Listener listener =
            impl.listeners[index];
        ++index;
        if (listener.subscription.value <= boundary &&
            listener.callback != nullptr) {
            listener.callback(
                listener.context,
                key,
                kind,
                impl.generation);
        }
    }
}

Base::Result<ResourceValue> LookupImpl(
    const ResourceDictionary::DictionaryState& impl,
    const ResourceKey& key,
    Base::Vector<const ResourceDictionary::DictionaryState*>& visited) noexcept {
    for (const ResourceDictionary::DictionaryState* active : visited) {
        if (active == &impl) {
            return Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "ResourceDictionary merge cycle was detected");
        }
    }
    Base::Result<void> pushed =
        visited.PushBack(&impl);
    if (!pushed) {
        return pushed.GetStatus();
    }
    const ResourceDictionary::DictionaryState::Entry* local =
        FindLocal(impl, key);
    if (local != nullptr) {
        ResourceValue value = local->value;
        visited.PopBack();
        return value;
    }
    for (std::uint32_t index = impl.merged.Size();
         index > 0U;
         --index) {
        const ResourceDictionary::DictionaryState* dictionary =
            impl.merged[index - 1U].dictionary;
        if (dictionary == nullptr) {
            continue;
        }
        Base::Result<ResourceValue> result =
            LookupImpl(*dictionary, key, visited);
        if (result) {
            visited.PopBack();
            return result.Value();
        }
        if (result.GetStatus().code !=
            Base::ErrorCode::NotFound) {
            visited.PopBack();
            return result.GetStatus();
        }
    }
    visited.PopBack();
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        MessageResourceNotFound);
}

bool DependsOn(
    const ResourceDictionary::DictionaryState& root,
    const ResourceDictionary::DictionaryState& candidate,
    Base::Vector<const ResourceDictionary::DictionaryState*>& visited) noexcept {
    if (&root == &candidate) {
        return true;
    }
    for (const ResourceDictionary::DictionaryState* active : visited) {
        if (active == &root) {
            return false;
        }
    }
    if (!visited.PushBack(&root)) {
        return true;
    }
    for (const ResourceDictionary::DictionaryState::Merged& merged :
         root.merged) {
        if (merged.dictionary != nullptr &&
            DependsOn(
                *merged.dictionary,
                candidate,
                visited)) {
            visited.PopBack();
            return true;
        }
    }
    visited.PopBack();
    return false;
}

Base::Result<ResourceChangeSubscription> SubscribeImpl(
    ResourceDictionary::DictionaryState& impl,
    ResourceChangedCallback callback,
    void* context) noexcept {
    if (callback == nullptr ||
        impl.nextSubscription == UINT64_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Resource change callback is invalid");
    }
    const ResourceChangeSubscription subscription{
        impl.nextSubscription++};
    Base::Result<void> appended =
        impl.listeners.PushBack({
            subscription, callback, context});
    if (!appended) {
        return appended.GetStatus();
    }
    return subscription;
}

bool UnsubscribeImpl(
    ResourceDictionary::DictionaryState& impl,
    ResourceChangeSubscription subscription) noexcept {
    if (!subscription.IsValid()) {
        return false;
    }
    for (std::uint32_t index = 0U;
         index < impl.listeners.Size();
         ++index) {
        if (impl.listeners[index].subscription.value !=
            subscription.value) {
            continue;
        }
        if (index + 1U != impl.listeners.Size()) {
            impl.listeners[index] =
                impl.listeners.Back();
        }
        impl.listeners.PopBack();
        return true;
    }
    return false;
}

void MergedChanged(
    void* context,
    Base::StringView key,
    ResourceChangeKind,
    std::uint64_t) noexcept {
    auto* owner =
        static_cast<ResourceDictionary::DictionaryState*>(context);
    if (owner != nullptr) {
        Notify(
            *owner,
            key,
            ResourceChangeKind::MergedDictionaryChanged);
    }
}

} // namespace

ResourceDictionary::ResourceDictionary() noexcept = default;

ResourceDictionary::ResourceDictionary(
    Access* impl,
    bool addReference) noexcept
    : state_(impl) {
    if (addReference) {
        AddStateRef(state_);
    }
}

ResourceDictionary::~ResourceDictionary() noexcept {
    ReleaseState(state_);
}

ResourceDictionary::ResourceDictionary(
    ResourceDictionary&& other) noexcept
    : Base::Object(),
      state_(other.state_) {
    other.state_ = nullptr;
}

ResourceDictionary& ResourceDictionary::operator=(
    ResourceDictionary&& other) noexcept {
    if (this != &other) {
        ReleaseState(state_);
        state_ = other.state_;
        other.state_ = nullptr;
    }
    return *this;
}

Base::Result<ResourceDictionary::DictionaryState*>
ResourceDictionary::EnsureState() noexcept {
    if (state_ != nullptr) {
        return state_;
    }
    Base::IAllocator& allocator =
        Base::GetDefaultAllocator();
    void* memory = allocator.Allocate({
        sizeof(Access),
        alignof(Access),
        Base::MemoryTag::Container});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "ResourceDictionary allocation failed");
    }
    state_ = new (memory) Access();
    return state_;
}

void ResourceDictionary::AddStateRef(
    Access* impl) noexcept {
    if (impl != nullptr && impl->references != UINT32_MAX) {
        ++impl->references;
    }
}

void ResourceDictionary::ReleaseState(
    Access* impl) noexcept {
    if (impl == nullptr || impl->references == UINT32_MAX) {
        return;
    }
    if (--impl->references != 0U) {
        return;
    }
    while (!impl->merged.Empty()) {
        Access::Merged merged = impl->merged.Back();
        impl->merged.PopBack();
        if (merged.dictionary != nullptr) {
            UnsubscribeImpl(
                *merged.dictionary,
                merged.subscription);
            ReleaseState(merged.dictionary);
        }
    }
    Base::IAllocator& allocator =
        Base::GetDefaultAllocator();
    impl->~Access();
    allocator.Deallocate(
        impl,
        sizeof(Access),
        alignof(Access),
        Base::MemoryTag::Container);
}

Base::Result<void> ResourceDictionary::Add(
    const ResourceKey& key,
    const ResourceValue& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (!key.IsValid() ||
        value.Type() == Meta::InvalidTypeId ||
        value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }
    Base::Result<Access*> storage = EnsureState();
    if (!storage) {
        return storage.GetStatus();
    }
    if (storage.Value()->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            MessageReadOnly);
    }
    if (FindLocal(*storage.Value(), key) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            MessageDuplicateResource);
    }
    Access::Entry entry;
    entry.key = key;
    entry.value = value;
    entry.source = source;
    Base::Result<void> appended =
        storage.Value()->entries.PushBack(
            std::move(entry));
    if (!appended) {
        return appended.GetStatus();
    }
    Notify(
        *storage.Value(),
        CallbackKey(key),
        ResourceChangeKind::Added);
    return {};
}

Base::Result<void> ResourceDictionary::Add(
    Base::StringView key,
    const ResourceValue& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    Base::Result<ResourceKey> resourceKey =
        ResourceKey::FromString(key);
    if (!resourceKey) {
        return resourceKey.GetStatus();
    }
    return Add(resourceKey.Value(), value, source);
}

Base::Result<void> ResourceDictionary::Add(
    Meta::TypeId key,
    const ResourceValue& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    return Add(
        ResourceKey::FromType(key), value, source);
}

Base::Result<void> ResourceDictionary::Add(
    Base::StringView key,
    Meta::TypeId type,
    const Base::Ref<Base::Object>& object,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (type == Meta::InvalidTypeId || !object) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }
    return Add(
        key,
        Meta::Value::FromObject(type, object),
        source);
}

Base::Result<void> ResourceDictionary::StoreResource(
    const ResourceKey& key,
    const ResourceValue& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (!key.IsValid() ||
        value.Type() == Meta::InvalidTypeId ||
        value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }
    Base::Result<Access*> storage = EnsureState();
    if (!storage) {
        return storage.GetStatus();
    }
    if (storage.Value()->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            MessageReadOnly);
    }
    Access::Entry* entry =
        FindLocal(*storage.Value(), key);
    if (entry == nullptr) {
        return Add(key, value, source);
    }
    entry->value = value;
    entry->source = source;
    Notify(
        *storage.Value(),
        CallbackKey(key),
        ResourceChangeKind::Replaced);
    return {};
}

Base::Result<void> ResourceDictionary::StoreResource(
    Base::StringView key,
    const ResourceValue& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    Base::Result<ResourceKey> resourceKey =
        ResourceKey::FromString(key);
    if (!resourceKey) {
        return resourceKey.GetStatus();
    }
    return StoreResource(resourceKey.Value(), value, source);
}

Base::Result<void> ResourceDictionary::StoreResource(
    Meta::TypeId key,
    const ResourceValue& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    return StoreResource(
        ResourceKey::FromType(key), value, source);
}

Base::Result<void> ResourceDictionary::StoreResource(
    Base::StringView key,
    Meta::TypeId type,
    const Base::Ref<Base::Object>& object,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    if (type == Meta::InvalidTypeId || !object) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }
    return StoreResource(
        key,
        Meta::Value::FromObject(type, object),
        source);
}

bool ResourceDictionary::Set(
    const ResourceKey& key,
    const ResourceValue& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    return static_cast<bool>(StoreResource(key, value, source));
}

bool ResourceDictionary::Set(
    Base::StringView key,
    const ResourceValue& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    return static_cast<bool>(StoreResource(key, value, source));
}

bool ResourceDictionary::Set(
    Meta::TypeId key,
    const ResourceValue& value,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    return static_cast<bool>(StoreResource(key, value, source));
}

bool ResourceDictionary::Set(
    Base::StringView key,
    Meta::TypeId type,
    const Base::Ref<Base::Object>& object,
    ::Aero::Diagnostics::SourceSpan source) noexcept {
    return static_cast<bool>(StoreResource(key, type, object, source));
}

Base::Result<bool> ResourceDictionary::Remove(
    const ResourceKey& key) noexcept {
    if (!key.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }
    if (state_ == nullptr) {
        return false;
    }
    if (state_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            MessageReadOnly);
    }
    for (std::uint32_t index = 0U;
         index < state_->entries.Size();
         ++index) {
        if (state_->entries[index].key != key) {
            continue;
        }
        if (index + 1U != state_->entries.Size()) {
            state_->entries[index] =
                std::move(state_->entries.Back());
        }
        state_->entries.PopBack();
        Notify(
            *state_,
            CallbackKey(key),
            ResourceChangeKind::Removed);
        return true;
    }
    return false;
}

Base::Result<bool> ResourceDictionary::Remove(
    Base::StringView key) noexcept {
    Base::Result<ResourceKey> resourceKey =
        ResourceKey::FromString(key);
    if (!resourceKey) {
        return resourceKey.GetStatus();
    }
    return Remove(resourceKey.Value());
}

Base::Result<bool> ResourceDictionary::Remove(
    Meta::TypeId key) noexcept {
    return Remove(ResourceKey::FromType(key));
}

Base::Result<ResourceValue> ResourceDictionary::Lookup(
    const ResourceKey& key) const noexcept {
    if (!key.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            MessageResourceNotFound);
    }
    Base::Vector<const Access*> visited;
    return LookupImpl(*state_, key, visited);
}

Base::Result<ResourceValue> ResourceDictionary::Lookup(
    Base::StringView key) const noexcept {
    Base::Result<ResourceKey> resourceKey =
        ResourceKey::FromString(key);
    if (!resourceKey) {
        return resourceKey.GetStatus();
    }
    Base::Result<ResourceValue> result =
        Lookup(resourceKey.Value());
    if (!result &&
        result.GetStatus().code == Base::ErrorCode::NotFound) {
        thread_local char message[384];
        std::snprintf(
            message,
            sizeof(message),
            "ResourceDictionary key '%.*s' was not found",
            static_cast<int>(key.SizeBytes()),
            key.Data());
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            message);
    }
    return result;
}

Base::Result<ResourceValue> ResourceDictionary::Lookup(
    Meta::TypeId key) const noexcept {
    return Lookup(ResourceKey::FromType(key));
}

bool ResourceDictionary::Contains(
    const ResourceKey& key) const noexcept {
    return static_cast<bool>(Lookup(key));
}

bool ResourceDictionary::Contains(
    Base::StringView key) const noexcept {
    return static_cast<bool>(Lookup(key));
}

bool ResourceDictionary::Contains(
    Meta::TypeId key) const noexcept {
    return static_cast<bool>(Lookup(key));
}

::Aero::Diagnostics::SourceSpan ResourceDictionary::SourceOf(
    const ResourceKey& key) const noexcept {
    if (state_ == nullptr || !key.IsValid()) {
        return {};
    }
    const Access::Entry* entry =
        FindLocal(*state_, key);
    return entry != nullptr
        ? entry->source
        : ::Aero::Diagnostics::SourceSpan{};
}

::Aero::Diagnostics::SourceSpan ResourceDictionary::SourceOf(
    Base::StringView key) const noexcept {
    Base::Result<ResourceKey> resourceKey =
        ResourceKey::FromString(key);
    return resourceKey
        ? SourceOf(resourceKey.Value())
        : ::Aero::Diagnostics::SourceSpan{};
}

Base::Result<void> ResourceDictionary::AddMerged(
    ResourceDictionary& dictionary) noexcept {
    Base::Result<Access*> owner = EnsureState();
    if (!owner) {
        return owner.GetStatus();
    }
    Base::Result<Access*> child = dictionary.EnsureState();
    if (!child) {
        return child.GetStatus();
    }
    if (owner.Value()->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            MessageReadOnly);
    }
    if (owner.Value() == child.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::CycleDetected,
            "ResourceDictionary cannot merge itself");
    }
    for (const Access::Merged& merged :
         owner.Value()->merged) {
        if (merged.dictionary == child.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "ResourceDictionary is already merged");
        }
    }
    Base::Vector<const Access*> visited;
    if (DependsOn(
            *child.Value(),
            *owner.Value(),
            visited)) {
        return Base::Status::Failure(
            Base::ErrorCode::CycleDetected,
            "ResourceDictionary merge would create a cycle");
    }
    Base::Result<ResourceChangeSubscription> subscribed =
        SubscribeImpl(
            *child.Value(),
            &MergedChanged,
            owner.Value());
    if (!subscribed) {
        return subscribed.GetStatus();
    }
    AddStateRef(child.Value());
    Base::Result<void> appended =
        owner.Value()->merged.PushBack({
            child.Value(), subscribed.Value()});
    if (!appended) {
        UnsubscribeImpl(
            *child.Value(),
            subscribed.Value());
        ReleaseState(child.Value());
        return appended.GetStatus();
    }
    Notify(
        *owner.Value(),
        {},
        ResourceChangeKind::MergedDictionaryChanged);
    return {};
}

Base::Result<bool> ResourceDictionary::RemoveMerged(
    ResourceDictionary& dictionary) noexcept {
    if (state_ == nullptr ||
        dictionary.state_ == nullptr) {
        return false;
    }
    if (state_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            MessageReadOnly);
    }
    for (std::uint32_t index = 0U;
         index < state_->merged.Size();
         ++index) {
        Access::Merged merged = state_->merged[index];
        if (merged.dictionary != dictionary.state_) {
            continue;
        }
        UnsubscribeImpl(
            *merged.dictionary,
            merged.subscription);
        if (index + 1U != state_->merged.Size()) {
            state_->merged[index] =
                state_->merged.Back();
        }
        state_->merged.PopBack();
        ReleaseState(merged.dictionary);
        Notify(
            *state_,
            {},
            ResourceChangeKind::MergedDictionaryChanged);
        return true;
    }
    return false;
}

void
ResourceDictionary::ClearMergedDictionaries() noexcept {
    if (state_ == nullptr) return;
    if (state_->sealed) {
        return;
    }
    if (state_->merged.Empty()) return;
    while (!state_->merged.Empty()) {
        Access::Merged merged = state_->merged.Back();
        state_->merged.PopBack();
        if (merged.dictionary != nullptr) {
            UnsubscribeImpl(
                *merged.dictionary,
                merged.subscription);
            ReleaseState(merged.dictionary);
        }
    }
    Notify(
        *state_,
        {},
        ResourceChangeKind::MergedDictionaryChanged);
}

std::uint32_t
ResourceDictionary::MergedDictionaryCount() const noexcept {
    return state_ != nullptr
        ? state_->merged.Size()
        : 0U;
}

Base::Result<ResourceDictionary>
ResourceDictionary::MergedDictionaryAt(
    std::uint32_t index) const noexcept {
    if (state_ == nullptr ||
        index >= state_->merged.Size() ||
        state_->merged[index].dictionary == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Merged resource dictionary index is out of range");
    }
    return ResourceDictionary(
        state_->merged[index].dictionary,
        true);
}

Base::Result<ResourceDictionary>
ResourceDictionary::Share() const noexcept {
    Base::Result<Access*> storage =
        const_cast<ResourceDictionary*>(this)->EnsureState();
    if (!storage) return storage.GetStatus();
    return ResourceDictionary(storage.Value(), true);
}

void ResourceDictionary::SetSource(
    const Base::ResourceUri& source) noexcept {
    if (source.Empty()) {
        return;
    }
    Base::Result<Access*> storage = EnsureState();
    if (!storage) return;
    if (storage.Value()->sealed) {
        return;
    }
    if (source.Scheme().Empty() && !source.Assembly().Empty()) {
        Base::String pack;
        Base::Result<void> assigned = pack.Assign(
            "pack://application:,,,/");
        Base::StringView path = source.Path();
        if (!path.Empty() && path[0] == '/') {
            path = path.Substr(1U, path.SizeBytes() - 1U);
        }
        if (assigned) assigned = pack.Append(path);
        Base::Result<Base::ResourceUri> normalized = assigned
            ? Base::ResourceUri::Parse(pack.View())
            : Base::Result<Base::ResourceUri>(assigned.GetStatus());
        storage.Value()->source = normalized
            ? std::move(normalized).Value()
            : source;
    } else {
        storage.Value()->source = source;
    }
    Notify(
        *storage.Value(),
        {},
        ResourceChangeKind::SourceChanged);
}

const Base::ResourceUri&
ResourceDictionary::GetSource() const noexcept {
    static const Base::ResourceUri empty;
    return state_ != nullptr
        ? state_->source
        : empty;
}

Base::Result<void> ResourceDictionary::Seal() noexcept {
    Base::Result<Access*> storage = EnsureState();
    if (!storage) {
        return storage.GetStatus();
    }
    if (storage.Value()->sealed) {
        return {};
    }
    storage.Value()->sealed = true;
    Notify(
        *storage.Value(),
        {},
        ResourceChangeKind::Sealed);
    return {};
}

bool ResourceDictionary::GetIsSealed() const noexcept {
    return state_ != nullptr && state_->sealed;
}

Base::Result<ResourceChangeSubscription>
ResourceDictionary::SubscribeChanged(
    ResourceChangedCallback callback,
    void* context) noexcept {
    Base::Result<Access*> storage = EnsureState();
    if (!storage) {
        return storage.GetStatus();
    }
    return SubscribeImpl(
        *storage.Value(), callback, context);
}

bool ResourceDictionary::Unsubscribe(
    ResourceChangeSubscription subscription) noexcept {
    return state_ != nullptr &&
        UnsubscribeImpl(*state_, subscription);
}

void ResourceDictionary::Clear() noexcept {
    if (state_ == nullptr) {
        return;
    }
    if (state_->sealed) {
        return;
    }
    if (state_->entries.Empty() &&
        state_->merged.Empty() &&
        state_->source.Empty()) {
        return;
    }
    state_->entries.Clear();
    while (!state_->merged.Empty()) {
        Access::Merged merged = state_->merged.Back();
        state_->merged.PopBack();
        if (merged.dictionary != nullptr) {
            UnsubscribeImpl(
                *merged.dictionary,
                merged.subscription);
            ReleaseState(merged.dictionary);
        }
    }
    state_->source = {};
    Notify(
        *state_,
        {},
        ResourceChangeKind::Cleared);
}

std::uint32_t ResourceDictionary::Size() const noexcept {
    return state_ != nullptr
        ? state_->entries.Size()
        : 0U;
}

Base::Result<ResourceEntrySnapshot>
ResourceDictionary::EntryAt(
    std::uint32_t index) const noexcept {
    if (state_ == nullptr ||
        index >= state_->entries.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Resource dictionary entry index is out of range");
    }
    const Access::Entry& entry =
        state_->entries[index];
    ResourceEntrySnapshot snapshot;
    snapshot.key = entry.key;
    snapshot.value = entry.value;
    snapshot.source = entry.source;
    return snapshot;
}

std::uint64_t
ResourceDictionary::Generation() const noexcept {
    return state_ != nullptr
        ? state_->generation
        : 0U;
}

Base::Result<ResourceValue> ResourceResolver::Lookup(
    const FrameworkElement* element,
    const ResourceKey& key,
    const ResourceDictionary* templateResources,
    const ResourceEnvironment& environment) noexcept {
    const FrameworkElement* current = element;
    while (current != nullptr) {
        const ResourceDictionary* owned = current->LocalResources();
        if (owned != nullptr) {
            Base::Result<ResourceValue> local = owned->Lookup(key);
            if (local) {
                return local.Value();
            }
            if (local.GetStatus().code !=
                Base::ErrorCode::NotFound) {
                return local.GetStatus();
            }
        }
        const ::Aero::Media::Visual* logical = ::Aero::TryCast<::Aero::Media::Visual>(current->GetLogicalParent());
        current = logical != nullptr
            ? ::Aero::TryCast<::Aero::FrameworkElement>(logical)
            : nullptr;
    }
    const ResourceDictionary* layers[] = {
        templateResources,
        environment.application,
        environment.theme,
        environment.system};
    for (const ResourceDictionary* layer : layers) {
        if (layer == nullptr) {
            continue;
        }
        Base::Result<ResourceValue> value =
            layer->Lookup(key);
        if (value) {
            return value.Value();
        }
        if (value.GetStatus().code !=
            Base::ErrorCode::NotFound) {
            return value.GetStatus();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Resource was not found in the active environment");
}

Base::Result<ResourceValue> ResourceResolver::Lookup(
    const FrameworkElement* element,
    Base::StringView key,
    const ResourceDictionary* templateResources,
    const ResourceEnvironment& environment) noexcept {
    Base::Result<ResourceKey> resourceKey =
        ResourceKey::FromString(key);
    return resourceKey
        ? Lookup(
              element,
              resourceKey.Value(),
              templateResources,
              environment)
        : Base::Result<ResourceValue>(
              resourceKey.GetStatus());
}

Base::Result<ResourceValue> ResourceResolver::Lookup(
    const FrameworkElement* element,
    Meta::TypeId key,
    const ResourceDictionary* templateResources,
    const ResourceEnvironment& environment) noexcept {
    return Lookup(
        element,
        ResourceKey::FromType(key),
        templateResources,
        environment);
}

} // namespace Aero
