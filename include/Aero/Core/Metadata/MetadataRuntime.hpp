#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>

#include <cstdint>

namespace Aero::Core {

using MetadataPropertyProviderGetCallback = Base::Result<Value> (*)(
    const Base::Object& object,
    const PropertyInfo& property,
    void* context) noexcept;
using MetadataPropertyProviderSetCallback = Base::Result<void> (*)(
    Base::Object& object,
    const PropertyInfo& property,
    const Value& value,
    void* context) noexcept;

struct MetadataPropertyProviderRegistration final {
    PropertyProviderId id = InvalidPropertyProviderId;
    TypeId objectType = InvalidTypeId;
    MetadataPropertyProviderGetCallback get = nullptr;
    MetadataPropertyProviderSetCallback set = nullptr;
    void* context = nullptr;
};

struct ContentInfo final {
    TypeId ownerType = InvalidTypeId;
    MemberId member = InvalidMemberId;
    ContentKind kind = ContentKind::Single;
    ContentFlags flags = ContentFlags::None;
    bool writable = false;
    bool clearable = false;

    bool IsValid() const noexcept {
        return ownerType != InvalidTypeId &&
            member != InvalidMemberId;
    }
    bool IsVisual() const noexcept {
        return HasContentFlag(flags, ContentFlags::Visual);
    }
};

class AERO_API MetadataRuntime final {
public:
    explicit MetadataRuntime(MetadataDomain& domain) noexcept;
    MetadataRuntime(const MetadataRuntime&) = delete;
    MetadataRuntime& operator=(const MetadataRuntime&) = delete;

    Base::Result<void> TryRegisterPropertyProvider(
        const MetadataPropertyProviderRegistration& registration) noexcept;
    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    MetadataDomain& Domain() const noexcept { return *domain_; }
    const TypeRegistry& Types() const noexcept;
    bool IsReady() const noexcept {
        return frozen_ && domain_ != nullptr && domain_->IsSealed();
    }
    bool CanReadProperty(MemberId member) const noexcept;
    bool CanWriteProperty(MemberId member) const noexcept;
    bool CanReadValueMember(MemberId member) const noexcept;
    bool CanWriteValueMember(MemberId member) const noexcept;
    MemberId FindContentMember(TypeId type) const noexcept;
    Base::Result<ContentInfo> GetContentInfo(
        MemberId member) const noexcept;
    Base::Result<void> WriteContent(
        Base::Object& owner,
        MemberId member,
        const Base::Ref<Base::Object>& value) const noexcept;
    Base::Result<void> ClearContent(
        Base::Object& owner,
        MemberId member) const noexcept;
    Base::Result<std::uint64_t> SubscribePropertyChanged(
        Base::Object& object,
        MetadataPropertyChangedCallback callback,
        void* callbackContext = nullptr) const noexcept;
    Base::Result<bool> UnsubscribePropertyChanged(
        Base::Object& object,
        std::uint64_t subscription) const noexcept;

    Base::Result<Base::Ref<Base::Object>> CreateObject(
        TypeId type) const noexcept;
    Base::Result<Value> TryCreateValue(
        TypeId type,
        const void* source) const noexcept;
    Base::Result<Value> TryConvertText(
        TypeId type,
        Base::StringView text) const noexcept;
    Base::Result<Value> GetValueMember(
        const Value& owner,
        MemberId member) const noexcept;
    Base::Result<void> SetValueMember(
        Value& owner,
        MemberId member,
        const Value& value) const noexcept;
    Base::Result<Value> GetProperty(
        const Base::Object& object,
        MemberId member) const noexcept;
    Base::Result<void> SetProperty(
        Base::Object& object,
        MemberId member,
        const Value& value) const noexcept;
    Base::Result<Value> InvokeMethod(
        Base::Object& object,
        MemberId member,
        Base::Span<const Value> arguments) const noexcept;

private:
    MetadataDomain* domain_ = nullptr;
    Base::Vector<MetadataPropertyProviderRegistration> providers_;
    bool frozen_ = false;
    static bool HasPropertyFlag(
        PropertyFlags value,
        PropertyFlags flag) noexcept;
    static Base::Status RuntimeNotFrozen() noexcept;
    static Base::Status UnsupportedProperty() noexcept;
    bool IsRegisteredEnumValue(
        TypeId type,
        const Value& value) const noexcept;
    Base::Result<Value> TryConvertEnumText(
        const TypeInfo& type,
        Base::StringView input) const noexcept;
    Base::Result<void> ValidatePropertyTarget(
        const Base::Object& object,
        const PropertyInfo& property) const noexcept;
    Base::Result<Value> GetDependencyProperty(
        const Base::Object& object,
        const PropertyInfo& property) const noexcept;
    Base::Result<void> SetDependencyProperty(
        Base::Object& object,
        const PropertyInfo& property,
        const Value& value) const noexcept;
    const MetadataPropertyProviderRegistration* FindProvider(
        PropertyProviderId id) const noexcept;
};

} // namespace Aero::Core
