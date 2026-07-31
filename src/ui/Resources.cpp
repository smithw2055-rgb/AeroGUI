#include <Aero/Resources.hpp>

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/FrameworkElement.hpp>

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

Base::Result<void> NameScope::TryRegister(
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
        entry.name.TryAssign(name);
    if (!assigned) {
        return assigned.GetStatus();
    }
    entry.object = &object;
    return entries_.TryPushBack(std::move(entry));
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
        key.string_.TryAssign(value);
    if (!assigned) {
        return assigned.GetStatus();
    }
    key.kind_ = ResourceKeyKind::String;
    return key;
}

ResourceKey ResourceKey::FromType(
    Core::TypeId value) noexcept {
    ResourceKey key;
    if (value != Core::InvalidTypeId) {
        key.kind_ = ResourceKeyKind::Type;
        key.type_ = value;
    }
    return key;
}

bool ResourceKey::IsValid() const noexcept {
    return (kind_ == ResourceKeyKind::String &&
            !string_.Empty()) ||
        (kind_ == ResourceKeyKind::Type &&
         type_ != Core::InvalidTypeId);
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

struct ResourceDictionary::Impl final {
    struct Entry final {
        ResourceKey key;
        ResourceValue value;
        Core::SourceSpan source;
    };

    struct Listener final {
        ResourceChangeSubscription subscription;
        ResourceChangedCallback callback = nullptr;
        void* context = nullptr;
    };

    struct Merged final {
        Impl* dictionary = nullptr;
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

namespace {

ResourceDictionary::Impl::Entry* FindLocal(
    ResourceDictionary::Impl& impl,
    const ResourceKey& key) noexcept {
    for (ResourceDictionary::Impl::Entry& entry :
         impl.entries) {
        if (entry.key == key) {
            return &entry;
        }
    }
    return nullptr;
}

const ResourceDictionary::Impl::Entry* FindLocal(
    const ResourceDictionary::Impl& impl,
    const ResourceKey& key) noexcept {
    for (const ResourceDictionary::Impl::Entry& entry :
         impl.entries) {
        if (entry.key == key) {
            return &entry;
        }
    }
    return nullptr;
}

void Notify(
    ResourceDictionary::Impl& impl,
    Base::StringView key,
    ResourceChangeKind kind) noexcept {
    if (impl.generation != UINT64_MAX) {
        ++impl.generation;
    }
    const std::uint64_t boundary =
        impl.nextSubscription - 1U;
    std::uint32_t index = 0U;
    while (index < impl.listeners.Size()) {
        const ResourceDictionary::Impl::Listener listener =
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
    const ResourceDictionary::Impl& impl,
    const ResourceKey& key,
    Base::Vector<const ResourceDictionary::Impl*>& visited) noexcept {
    for (const ResourceDictionary::Impl* active : visited) {
        if (active == &impl) {
            return Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "ResourceDictionary merge cycle was detected");
        }
    }
    Base::Result<void> pushed =
        visited.TryPushBack(&impl);
    if (!pushed) {
        return pushed.GetStatus();
    }
    const ResourceDictionary::Impl::Entry* local =
        FindLocal(impl, key);
    if (local != nullptr) {
        ResourceValue value = local->value;
        visited.PopBack();
        return value;
    }
    for (std::uint32_t index = impl.merged.Size();
         index > 0U;
         --index) {
        const ResourceDictionary::Impl* dictionary =
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
    const ResourceDictionary::Impl& root,
    const ResourceDictionary::Impl& candidate,
    Base::Vector<const ResourceDictionary::Impl*>& visited) noexcept {
    if (&root == &candidate) {
        return true;
    }
    for (const ResourceDictionary::Impl* active : visited) {
        if (active == &root) {
            return false;
        }
    }
    if (!visited.TryPushBack(&root)) {
        return true;
    }
    for (const ResourceDictionary::Impl::Merged& merged :
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
    ResourceDictionary::Impl& impl,
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
        impl.listeners.TryPushBack({
            subscription, callback, context});
    if (!appended) {
        return appended.GetStatus();
    }
    return subscription;
}

bool UnsubscribeImpl(
    ResourceDictionary::Impl& impl,
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
        static_cast<ResourceDictionary::Impl*>(context);
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
    Impl* impl,
    bool addReference) noexcept
    : impl_(impl) {
    if (addReference) {
        AddImplRef(impl_);
    }
}

ResourceDictionary::~ResourceDictionary() noexcept {
    ReleaseImpl(impl_);
}

ResourceDictionary::ResourceDictionary(
    ResourceDictionary&& other) noexcept
    : Base::Object(),
      impl_(other.impl_) {
    other.impl_ = nullptr;
}

ResourceDictionary& ResourceDictionary::operator=(
    ResourceDictionary&& other) noexcept {
    if (this != &other) {
        ReleaseImpl(impl_);
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

Base::Result<ResourceDictionary::Impl*>
ResourceDictionary::EnsureImpl() noexcept {
    if (impl_ != nullptr) {
        return impl_;
    }
    Base::IAllocator& allocator =
        Base::GetDefaultAllocator();
    void* memory = allocator.Allocate({
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Container});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "ResourceDictionary allocation failed");
    }
    impl_ = new (memory) Impl();
    return impl_;
}

void ResourceDictionary::AddImplRef(
    Impl* impl) noexcept {
    if (impl != nullptr && impl->references != UINT32_MAX) {
        ++impl->references;
    }
}

void ResourceDictionary::ReleaseImpl(
    Impl* impl) noexcept {
    if (impl == nullptr || impl->references == UINT32_MAX) {
        return;
    }
    if (--impl->references != 0U) {
        return;
    }
    while (!impl->merged.Empty()) {
        Impl::Merged merged = impl->merged.Back();
        impl->merged.PopBack();
        if (merged.dictionary != nullptr) {
            UnsubscribeImpl(
                *merged.dictionary,
                merged.subscription);
            ReleaseImpl(merged.dictionary);
        }
    }
    Base::IAllocator& allocator =
        Base::GetDefaultAllocator();
    impl->~Impl();
    allocator.Deallocate(
        impl,
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Container);
}

Base::Result<void> ResourceDictionary::TryAdd(
    const ResourceKey& key,
    const ResourceValue& value,
    Core::SourceSpan source) noexcept {
    if (!key.IsValid() ||
        value.Type() == Core::InvalidTypeId ||
        value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }
    Base::Result<Impl*> storage = EnsureImpl();
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
    Impl::Entry entry;
    entry.key = key;
    entry.value = value;
    entry.source = source;
    Base::Result<void> appended =
        storage.Value()->entries.TryPushBack(
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

Base::Result<void> ResourceDictionary::TryAdd(
    Base::StringView key,
    const ResourceValue& value,
    Core::SourceSpan source) noexcept {
    Base::Result<ResourceKey> resourceKey =
        ResourceKey::FromString(key);
    if (!resourceKey) {
        return resourceKey.GetStatus();
    }
    return TryAdd(resourceKey.Value(), value, source);
}

Base::Result<void> ResourceDictionary::TryAdd(
    Core::TypeId key,
    const ResourceValue& value,
    Core::SourceSpan source) noexcept {
    return TryAdd(
        ResourceKey::FromType(key), value, source);
}

Base::Result<void> ResourceDictionary::TryAdd(
    Base::StringView key,
    Core::TypeId type,
    const Base::Ref<Base::Object>& object,
    Core::SourceSpan source) noexcept {
    if (type == Core::InvalidTypeId || !object) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }
    return TryAdd(
        key,
        Core::Value::FromObject(type, object),
        source);
}

Base::Result<void> ResourceDictionary::TrySet(
    const ResourceKey& key,
    const ResourceValue& value,
    Core::SourceSpan source) noexcept {
    if (!key.IsValid() ||
        value.Type() == Core::InvalidTypeId ||
        value.IsUnset()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }
    Base::Result<Impl*> storage = EnsureImpl();
    if (!storage) {
        return storage.GetStatus();
    }
    if (storage.Value()->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            MessageReadOnly);
    }
    Impl::Entry* entry =
        FindLocal(*storage.Value(), key);
    if (entry == nullptr) {
        return TryAdd(key, value, source);
    }
    entry->value = value;
    entry->source = source;
    Notify(
        *storage.Value(),
        CallbackKey(key),
        ResourceChangeKind::Replaced);
    return {};
}

Base::Result<void> ResourceDictionary::TrySet(
    Base::StringView key,
    const ResourceValue& value,
    Core::SourceSpan source) noexcept {
    Base::Result<ResourceKey> resourceKey =
        ResourceKey::FromString(key);
    if (!resourceKey) {
        return resourceKey.GetStatus();
    }
    return TrySet(resourceKey.Value(), value, source);
}

Base::Result<void> ResourceDictionary::TrySet(
    Core::TypeId key,
    const ResourceValue& value,
    Core::SourceSpan source) noexcept {
    return TrySet(
        ResourceKey::FromType(key), value, source);
}

Base::Result<void> ResourceDictionary::TrySet(
    Base::StringView key,
    Core::TypeId type,
    const Base::Ref<Base::Object>& object,
    Core::SourceSpan source) noexcept {
    if (type == Core::InvalidTypeId || !object) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }
    return TrySet(
        key,
        Core::Value::FromObject(type, object),
        source);
}

Base::Result<bool> ResourceDictionary::Remove(
    const ResourceKey& key) noexcept {
    if (!key.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }
    if (impl_ == nullptr) {
        return false;
    }
    if (impl_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            MessageReadOnly);
    }
    for (std::uint32_t index = 0U;
         index < impl_->entries.Size();
         ++index) {
        if (impl_->entries[index].key != key) {
            continue;
        }
        if (index + 1U != impl_->entries.Size()) {
            impl_->entries[index] =
                std::move(impl_->entries.Back());
        }
        impl_->entries.PopBack();
        Notify(
            *impl_,
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
    Core::TypeId key) noexcept {
    return Remove(ResourceKey::FromType(key));
}

Base::Result<ResourceValue> ResourceDictionary::Lookup(
    const ResourceKey& key) const noexcept {
    if (!key.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            MessageResourceNotFound);
    }
    Base::Vector<const Impl*> visited;
    return LookupImpl(*impl_, key, visited);
}

Base::Result<ResourceValue> ResourceDictionary::Lookup(
    Base::StringView key) const noexcept {
    Base::Result<ResourceKey> resourceKey =
        ResourceKey::FromString(key);
    if (!resourceKey) {
        return resourceKey.GetStatus();
    }
    return Lookup(resourceKey.Value());
}

Base::Result<ResourceValue> ResourceDictionary::Lookup(
    Core::TypeId key) const noexcept {
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
    Core::TypeId key) const noexcept {
    return static_cast<bool>(Lookup(key));
}

Core::SourceSpan ResourceDictionary::SourceOf(
    const ResourceKey& key) const noexcept {
    if (impl_ == nullptr || !key.IsValid()) {
        return {};
    }
    const Impl::Entry* entry =
        FindLocal(*impl_, key);
    return entry != nullptr
        ? entry->source
        : Core::SourceSpan{};
}

Core::SourceSpan ResourceDictionary::SourceOf(
    Base::StringView key) const noexcept {
    Base::Result<ResourceKey> resourceKey =
        ResourceKey::FromString(key);
    return resourceKey
        ? SourceOf(resourceKey.Value())
        : Core::SourceSpan{};
}

Base::Result<void> ResourceDictionary::TryAddMerged(
    ResourceDictionary& dictionary) noexcept {
    Base::Result<Impl*> owner = EnsureImpl();
    if (!owner) {
        return owner.GetStatus();
    }
    Base::Result<Impl*> child = dictionary.EnsureImpl();
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
    for (const Impl::Merged& merged :
         owner.Value()->merged) {
        if (merged.dictionary == child.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "ResourceDictionary is already merged");
        }
    }
    Base::Vector<const Impl*> visited;
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
    AddImplRef(child.Value());
    Base::Result<void> appended =
        owner.Value()->merged.TryPushBack({
            child.Value(), subscribed.Value()});
    if (!appended) {
        UnsubscribeImpl(
            *child.Value(),
            subscribed.Value());
        ReleaseImpl(child.Value());
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
    if (impl_ == nullptr ||
        dictionary.impl_ == nullptr) {
        return false;
    }
    if (impl_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            MessageReadOnly);
    }
    for (std::uint32_t index = 0U;
         index < impl_->merged.Size();
         ++index) {
        Impl::Merged merged = impl_->merged[index];
        if (merged.dictionary != dictionary.impl_) {
            continue;
        }
        UnsubscribeImpl(
            *merged.dictionary,
            merged.subscription);
        if (index + 1U != impl_->merged.Size()) {
            impl_->merged[index] =
                impl_->merged.Back();
        }
        impl_->merged.PopBack();
        ReleaseImpl(merged.dictionary);
        Notify(
            *impl_,
            {},
            ResourceChangeKind::MergedDictionaryChanged);
        return true;
    }
    return false;
}

Base::Result<void>
ResourceDictionary::ClearMergedDictionaries() noexcept {
    if (impl_ == nullptr) return {};
    if (impl_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            MessageReadOnly);
    }
    if (impl_->merged.Empty()) return {};
    while (!impl_->merged.Empty()) {
        Impl::Merged merged = impl_->merged.Back();
        impl_->merged.PopBack();
        if (merged.dictionary != nullptr) {
            UnsubscribeImpl(
                *merged.dictionary,
                merged.subscription);
            ReleaseImpl(merged.dictionary);
        }
    }
    Notify(
        *impl_,
        {},
        ResourceChangeKind::MergedDictionaryChanged);
    return {};
}

std::uint32_t
ResourceDictionary::MergedDictionaryCount() const noexcept {
    return impl_ != nullptr
        ? impl_->merged.Size()
        : 0U;
}

Base::Result<ResourceDictionary>
ResourceDictionary::MergedDictionaryAt(
    std::uint32_t index) const noexcept {
    if (impl_ == nullptr ||
        index >= impl_->merged.Size() ||
        impl_->merged[index].dictionary == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Merged resource dictionary index is out of range");
    }
    return ResourceDictionary(
        impl_->merged[index].dictionary,
        true);
}

Base::Result<ResourceDictionary>
ResourceDictionary::Share() const noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ResourceDictionary has no backing store");
    }
    return ResourceDictionary(impl_, true);
}

Base::Result<void> ResourceDictionary::SetSource(
    const Base::ResourceUri& source) noexcept {
    if (source.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ResourceDictionary Source cannot be empty");
    }
    Base::Result<Impl*> storage = EnsureImpl();
    if (!storage) {
        return storage.GetStatus();
    }
    if (storage.Value()->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            MessageReadOnly);
    }
    storage.Value()->source = source;
    Notify(
        *storage.Value(),
        {},
        ResourceChangeKind::SourceChanged);
    return {};
}

const Base::ResourceUri&
ResourceDictionary::Source() const noexcept {
    static const Base::ResourceUri empty;
    return impl_ != nullptr
        ? impl_->source
        : empty;
}

Base::Result<void> ResourceDictionary::Seal() noexcept {
    Base::Result<Impl*> storage = EnsureImpl();
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

bool ResourceDictionary::IsSealed() const noexcept {
    return impl_ != nullptr && impl_->sealed;
}

Base::Result<ResourceChangeSubscription>
ResourceDictionary::SubscribeChanged(
    ResourceChangedCallback callback,
    void* context) noexcept {
    Base::Result<Impl*> storage = EnsureImpl();
    if (!storage) {
        return storage.GetStatus();
    }
    return SubscribeImpl(
        *storage.Value(), callback, context);
}

bool ResourceDictionary::Unsubscribe(
    ResourceChangeSubscription subscription) noexcept {
    return impl_ != nullptr &&
        UnsubscribeImpl(*impl_, subscription);
}

Base::Result<void> ResourceDictionary::Clear() noexcept {
    if (impl_ == nullptr) {
        return {};
    }
    if (impl_->sealed) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            MessageReadOnly);
    }
    if (impl_->entries.Empty() &&
        impl_->merged.Empty() &&
        impl_->source.Empty()) {
        return {};
    }
    impl_->entries.Clear();
    while (!impl_->merged.Empty()) {
        Impl::Merged merged = impl_->merged.Back();
        impl_->merged.PopBack();
        if (merged.dictionary != nullptr) {
            UnsubscribeImpl(
                *merged.dictionary,
                merged.subscription);
            ReleaseImpl(merged.dictionary);
        }
    }
    impl_->source = {};
    Notify(
        *impl_,
        {},
        ResourceChangeKind::Cleared);
    return {};
}

std::uint32_t ResourceDictionary::Size() const noexcept {
    return impl_ != nullptr
        ? impl_->entries.Size()
        : 0U;
}

Base::Result<ResourceEntrySnapshot>
ResourceDictionary::EntryAt(
    std::uint32_t index) const noexcept {
    if (impl_ == nullptr ||
        index >= impl_->entries.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Resource dictionary entry index is out of range");
    }
    const Impl::Entry& entry =
        impl_->entries[index];
    ResourceEntrySnapshot snapshot;
    snapshot.key = entry.key;
    snapshot.value = entry.value;
    snapshot.source = entry.source;
    return snapshot;
}

std::uint64_t
ResourceDictionary::Generation() const noexcept {
    return impl_ != nullptr
        ? impl_->generation
        : 0U;
}

Base::Result<ResourceValue> ResourceResolver::Lookup(
    const FrameworkElement* element,
    const ResourceKey& key,
    const ResourceDictionary* templateResources,
    const ResourceEnvironment& environment) noexcept {
    const FrameworkElement* current = element;
    while (current != nullptr) {
        Base::Result<ResourceValue> local =
            current->Resources().Lookup(key);
        if (local) {
            return local.Value();
        }
        if (local.GetStatus().code !=
            Base::ErrorCode::NotFound) {
            return local.GetStatus();
        }
        const Visual* logical = current->GetLogicalParent();
        current = logical != nullptr
            ? logical->AsFrameworkElement()
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
    Core::TypeId key,
    const ResourceDictionary* templateResources,
    const ResourceEnvironment& environment) noexcept {
    return Lookup(
        element,
        ResourceKey::FromType(key),
        templateResources,
        environment);
}

} // namespace Aero
