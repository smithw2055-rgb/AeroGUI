#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Meta/Registration.hpp>
#include <Aero/Meta/TypeRegistry.hpp>

#include <cstdint>

namespace Aero::Core {

namespace Detail {
class MetaTable;
class MetadataPrivate;
}


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
        return ownerType != InvalidTypeId && member != InvalidMemberId;
    }
    bool IsVisual() const noexcept {
        return HasContentFlag(flags, ContentFlags::Visual);
    }
};

using MetadataModuleId = std::uint64_t;
inline constexpr MetadataModuleId InvalidMetadataModuleId = 0U;

constexpr MetadataModuleId MakeMetadataModuleId(
    Base::StringView name) noexcept {
    constexpr char domain[] = "AERO.METADATA.MODULE.V1";
    Base::Detail::StableMetadataIdBuilder builder;
    builder.AddText(domain, static_cast<std::uint32_t>(sizeof(domain) - 1U));
    builder.AddString(name);
    return builder.Finish();
}

using MetadataModuleRegisterCallback = Base::Result<void> (*)(
    MetaRegistration& context) noexcept;
using MetadataModuleRegisterContextCallback = Base::Result<void> (*)(
    MetaRegistration& context,
    void* userContext) noexcept;

struct MetadataModuleRegistration final {
    MetadataModuleId id = InvalidMetadataModuleId;
    Base::StringView name;
    std::uint32_t schemaVersion = 1U;
    MetadataModuleRegisterCallback registerModule = nullptr;
    MetadataModuleRegisterContextCallback registerModuleWithContext = nullptr;
    void* context = nullptr;
};

// A MetaRegistry has two explicit phases:
//
// 1. Registration phase: deterministic module callbacks populate mutable
//    TypeRegistry, dependency/routed registries, and registration value services.
// 2. Runtime phase: Seal() freezes the structural registry and materializes
//    internal executable runtime tables.
//
// Runtime structural lookup uses Types(). Registry references and
// registration-value views obtained before the next module
// transaction are provisional because a successful transaction replaces the
// complete candidate storage.
class AERO_API MetaRegistry final {
public:
    MetaRegistry() noexcept;
    ~MetaRegistry() noexcept;

    MetaRegistry(const MetaRegistry&) = delete;
    MetaRegistry& operator=(const MetaRegistry&) = delete;
    MetaRegistry(MetaRegistry&&) = delete;
    MetaRegistry& operator=(MetaRegistry&&) = delete;

    bool IsValid() const noexcept;
    bool IsSealed() const noexcept;
    std::uint32_t ModuleCount() const noexcept;

    Base::Result<void> TryRegisterModule(
        const MetadataModuleRegistration& registration) noexcept;
    Base::Result<void> Seal() noexcept;

    // Property providers are registered after the structural type graph is
    // sealed. Complete() finalizes executable metadata on this same object.
    Base::Result<void> TryRegisterPropertyProvider(
        const MetadataPropertyProviderRegistration& registration) noexcept;
    Base::Result<void> Complete() noexcept;
    bool IsReady() const noexcept;

    bool CanReadProperty(MemberId member) const noexcept;
    bool CanWriteProperty(MemberId member) const noexcept;
    bool CanReadValueMember(MemberId member) const noexcept;
    bool CanWriteValueMember(MemberId member) const noexcept;
    MemberId FindContentMember(TypeId type) const noexcept;
    Base::Result<ContentInfo> GetContentInfo(MemberId member) const noexcept;
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

    // Structural registration data is exposed read-only. Mutable registration
    // is confined to module callbacks and their MetaRegistration.
    const TypeRegistry& Types() const noexcept;
    const DependencyPropertyRegistry& DependencyProperties() const noexcept;
    Base::Result<Base::HashCode> ComputeSchemaHash() const noexcept;

private:
    friend class Detail::MetadataPrivate;

    struct Storage;
    Storage* storage_ = nullptr;

    DependencyPropertyRegistry& DependencyProperties() noexcept;
    void* RoutedEventState() noexcept;
    const Detail::MetaTable& RuntimeData() const noexcept;

    static Base::Status OutOfMemoryStatus() noexcept;
    static bool HasPropertyFlag(
        PropertyFlags value,
        PropertyFlags flag) noexcept;
    static Base::Status MetadataNotReady() noexcept;
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

    static Base::Result<void> ValidateRegistration(
        const MetadataModuleRegistration& registration) noexcept;
    Base::Result<Storage*> BuildCandidate(
        const MetadataModuleRegistration* extra,
        bool seal) const noexcept;
};

} // namespace Aero::Core
