#include <Aero/Markup/XamlNamesResources.hpp>

#include <utility>

namespace Aero::Markup {
namespace {

constexpr const char* MessageInvalidName =
    "XAML name must be a non-empty identifier without a namespace prefix";
constexpr const char* MessageDuplicateName =
    "XAML name is already registered in this name scope";
constexpr const char* MessageInvalidResource =
    "XAML resource requires a non-empty key, registered type, and object value";
constexpr const char* MessageDuplicateResource =
    "XAML resource key is already present in this dictionary";
constexpr const char* MessageResourceNotFound =
    "XAML resource key was not found";
constexpr const char* MessageNamespaceUnavailable =
    "XAML namespace scope is not available";
constexpr const char* MessageResourceResolverUnavailable =
    "XAML resource resolver is not available";

bool IsAsciiLetter(char value) noexcept {
    return (value >= 'A' && value <= 'Z') ||
        (value >= 'a' && value <= 'z');
}

bool IsNameStart(unsigned char value) noexcept {
    return IsAsciiLetter(static_cast<char>(value)) ||
        value == static_cast<unsigned char>('_') || value >= 0x80U;
}

bool IsNameContinue(unsigned char value) noexcept {
    return IsNameStart(value) ||
        (value >= static_cast<unsigned char>('0') &&
         value <= static_cast<unsigned char>('9'));
}

} // namespace

NameScope::NameScope(Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      entries_(allocator_) {}

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

    Entry entry(allocator_);
    Base::Result<void> assignResult = entry.name.TryAssign(name);
    if (!assignResult) {
        return assignResult.GetStatus();
    }
    entry.object = &object;
    return entries_.TryPushBack(std::move(entry));
}

Base::Object* NameScope::Find(Base::StringView name) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.name.View() == name) {
            return entry.object;
        }
    }
    return nullptr;
}

void NameScope::Clear() noexcept {
    entries_.Clear();
}

bool NameScope::IsValidName(Base::StringView name) noexcept {
    if (name.Empty() || name.Data() == nullptr) {
        return false;
    }

    const auto* bytes = reinterpret_cast<const unsigned char*>(name.Data());
    if (!IsNameStart(bytes[0])) {
        return false;
    }
    for (std::uint32_t index = 1U; index < name.SizeBytes(); ++index) {
        if (!IsNameContinue(bytes[index])) {
            return false;
        }
    }
    return true;
}

ResourceDictionary::ResourceDictionary(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      entries_(allocator_),
      listeners_(allocator_) {}

Base::Result<void> ResourceDictionary::TryAdd(
    Base::StringView key,
    Core::TypeId type,
    const Base::Ref<Base::Object>& object,
    Core::SourceSpan source) noexcept {
    if (key.Empty() || key.Data() == nullptr ||
        type == Core::InvalidTypeId || !object) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }
    if (FindEntry(key) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            MessageDuplicateResource);
    }

    Entry entry(allocator_);
    Base::Result<void> assignResult = entry.key.TryAssign(key);
    if (!assignResult) {
        return assignResult.GetStatus();
    }
    entry.type = type;
    entry.object = object;
    entry.source = source;
    Base::Result<void> appended = entries_.TryPushBack(std::move(entry));
    if (!appended) {
        return appended.GetStatus();
    }
    NotifyChanged(key, ResourceChangeKind::Added);
    return {};
}

Base::Result<void> ResourceDictionary::TrySet(
    Base::StringView key,
    Core::TypeId type,
    const Base::Ref<Base::Object>& object,
    Core::SourceSpan source) noexcept {
    if (key.Empty() || key.Data() == nullptr ||
        type == Core::InvalidTypeId || !object) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }

    for (Entry& entry : entries_) {
        if (entry.key.View() != key) {
            continue;
        }
        entry.type = type;
        entry.object = object;
        entry.source = source;
        NotifyChanged(key, ResourceChangeKind::Replaced);
        return {};
    }

    Entry entry(allocator_);
    Base::Result<void> assignResult = entry.key.TryAssign(key);
    if (!assignResult) {
        return assignResult.GetStatus();
    }
    entry.type = type;
    entry.object = object;
    entry.source = source;
    Base::Result<void> appended = entries_.TryPushBack(std::move(entry));
    if (!appended) {
        return appended.GetStatus();
    }
    NotifyChanged(key, ResourceChangeKind::Added);
    return {};
}

Base::Result<bool> ResourceDictionary::Remove(
    Base::StringView key) noexcept {
    if (key.Empty() || key.Data() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidResource);
    }
    for (std::uint32_t index = 0U; index < entries_.Size(); ++index) {
        if (entries_[index].key.View() != key) {
            continue;
        }
        Base::String removedKey(allocator_);
        Base::Result<void> copied = removedKey.TryAssign(entries_[index].key.View());
        if (!copied) {
            return copied.GetStatus();
        }
        if (index + 1U != entries_.Size()) {
            entries_[index] = std::move(entries_[entries_.Size() - 1U]);
        }
        entries_.PopBack();
        NotifyChanged(removedKey.View(), ResourceChangeKind::Removed);
        return true;
    }
    return false;
}

Base::Result<XamlResourceValue> ResourceDictionary::Lookup(
    Base::StringView key) const noexcept {
    const Entry* entry = FindEntry(key);
    if (entry == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            MessageResourceNotFound);
    }
    return XamlResourceValue{entry->type, entry->object};
}

bool ResourceDictionary::Contains(Base::StringView key) const noexcept {
    return FindEntry(key) != nullptr;
}

Core::SourceSpan ResourceDictionary::SourceOf(
    Base::StringView key) const noexcept {
    const Entry* entry = FindEntry(key);
    return entry != nullptr ? entry->source : Core::SourceSpan{};
}

void ResourceDictionary::Clear() noexcept {
    if (entries_.Empty()) {
        return;
    }
    entries_.Clear();
    NotifyChanged({}, ResourceChangeKind::Cleared);
}

Base::Result<ResourceChangeSubscription> ResourceDictionary::SubscribeChanged(
    ResourceChangedCallback callback,
    void* context) noexcept {
    if (callback == nullptr || nextSubscription_ == UINT64_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Resource change callback is invalid");
    }
    const ResourceChangeSubscription subscription{nextSubscription_++};
    Base::Result<void> added = listeners_.TryPushBack({
        subscription, callback, context});
    if (!added) {
        return added.GetStatus();
    }
    return subscription;
}

bool ResourceDictionary::Unsubscribe(
    ResourceChangeSubscription subscription) noexcept {
    if (!subscription.IsValid()) {
        return false;
    }
    for (std::uint32_t index = 0U; index < listeners_.Size(); ++index) {
        if (listeners_[index].subscription.value != subscription.value) {
            continue;
        }
        if (index + 1U != listeners_.Size()) {
            listeners_[index] = listeners_[listeners_.Size() - 1U];
        }
        listeners_.PopBack();
        return true;
    }
    return false;
}

void ResourceDictionary::NotifyChanged(
    Base::StringView key,
    ResourceChangeKind kind) noexcept {
    if (generation_ != UINT64_MAX) {
        ++generation_;
    }
    const std::uint64_t boundary = nextSubscription_ - 1U;
    for (std::uint32_t index = 0U; index < listeners_.Size(); ++index) {
        const Listener listener = listeners_[index];
        if (listener.subscription.value > boundary || listener.callback == nullptr) {
            continue;
        }
        listener.callback(listener.context, key, kind, generation_);
    }
}

const ResourceDictionary::Entry* ResourceDictionary::FindEntry(
    Base::StringView key) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.key.View() == key) {
            return &entry;
        }
    }
    return nullptr;
}

Base::Result<Base::StringView> XamlNamespaceScope::Lookup(
    Base::StringView prefix) const noexcept {
    if (lookup_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            MessageNamespaceUnavailable);
    }
    return lookup_(context_, prefix);
}

Base::Result<XamlResourceValue> XamlResourceResolver::Lookup(
    Base::StringView key) const noexcept {
    if (lookup_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            MessageResourceResolverUnavailable);
    }
    return lookup_(context_, key);
}

} // namespace Aero::Markup
