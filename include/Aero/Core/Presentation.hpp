#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/BuiltinTypeIds.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/MetadataRegistrationValues.hpp>

namespace Aero::Core {

class RoutedEventRegistry;
class MetadataRuntime;
struct RoutedEventHandle;
enum class RoutingStrategy : std::uint8_t;

struct PresentationContext final {
    Dispatcher* dispatcher = nullptr;
    DependencyPropertyRegistry* dependencyProperties = nullptr;
    MetadataValueRegistrationStore* valueRegistrations = nullptr;
    MetadataRuntime* metadataRuntime = nullptr;

    bool IsValid() const noexcept {
        return dispatcher != nullptr && dependencyProperties != nullptr;
    }
};

AERO_API PresentationContext
GetCurrentPresentationContext() noexcept;

AERO_API Base::Result<Value> TryCreatePresentationValue(
    TypeId type,
    const void* source) noexcept;

class AERO_API PresentationContextScope final {
public:
    PresentationContextScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties) noexcept;
    PresentationContextScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties,
        MetadataValueRegistrationStore& values) noexcept;
    PresentationContextScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties,
        MetadataRuntime& runtime) noexcept;
    PresentationContextScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties,
        MetadataRuntime* runtime) noexcept;
    ~PresentationContextScope();

    PresentationContextScope(const PresentationContextScope&) = delete;
    PresentationContextScope& operator=(const PresentationContextScope&) = delete;
    PresentationContextScope(PresentationContextScope&&) = delete;
    PresentationContextScope& operator=(PresentationContextScope&&) = delete;

private:
    PresentationContext context_;
    PresentationContext* previous_ = nullptr;
    DispatcherThreadToken ownerThread_ = 0U;
};

class MetaRegistrationContext final {
public:
    MetaRegistrationContext(
        TypeRegistry& typeRegistry,
        MetadataBehaviorRegistrationStore& behaviors,
        MetadataValueRegistrationStore& values,
        DependencyPropertyRegistry& properties,
        RoutedEventRegistry* events = nullptr) noexcept
        : types_(&typeRegistry),
          behaviorRegistrations_(&behaviors),
          valueRegistrations_(&values),
          dependencyProperties_(&properties),
          routedEvents_(events) {}

    MetadataRegistrationTypes Types() noexcept {
        return MetadataRegistrationTypes(*types_, *behaviorRegistrations_);
    }

    MetadataRegistrationValues Values() noexcept {
        return MetadataRegistrationValues(*valueRegistrations_);
    }

    MetadataRegistrationValues Values() const noexcept {
        return MetadataRegistrationValues(
            static_cast<const MetadataValueRegistrationStore&>(
                *valueRegistrations_));
    }

    const TypeRegistry& TypeView() const noexcept { return *types_; }
    MetadataValueRegistrationStore& ValueRegistrations() noexcept {
        return *valueRegistrations_;
    }
    DependencyPropertyRegistry& DependencyProperties() noexcept {
        return *dependencyProperties_;
    }
    RoutedEventRegistry* RoutedEvents() const noexcept {
        return routedEvents_;
    }

private:
    TypeRegistry* types_ = nullptr;
    MetadataBehaviorRegistrationStore* behaviorRegistrations_ = nullptr;
    MetadataValueRegistrationStore* valueRegistrations_ = nullptr;
    DependencyPropertyRegistry* dependencyProperties_ = nullptr;
    RoutedEventRegistry* routedEvents_ = nullptr;
};

enum class ContentKind : std::uint8_t {
    Single = 0U,
    Collection
};

class AERO_API MetaRegistrationBuilder final {
public:
    MetaRegistrationBuilder(
        MetaRegistrationContext& context,
        TypeId ownerType,
        Base::StringView xamlNamespace,
        Base::StringView name,
        TypeId baseType,
        TypeFlags flags) noexcept;

    Base::Result<void> Begin() noexcept;
    Base::Result<void> Finish() const noexcept;

    void DependencyProperty(
        DependencyPropertyHandle declaredHandle,
        Base::StringView declarationName,
        TypeId valueType,
        Value defaultValue,
        PropertyMetadataFlags metadataFlags,
        ValidateValueCallback validate = nullptr,
        CoerceValueCallback coerce = nullptr) noexcept;
    void AttachedDependencyProperty(
        DependencyPropertyHandle declaredHandle,
        Base::StringView declarationName,
        TypeId valueType,
        Value defaultValue,
        PropertyMetadataFlags metadataFlags,
        ValidateValueCallback validate = nullptr,
        CoerceValueCallback coerce = nullptr) noexcept;
    void Property(const PropertyRegistration& registration) noexcept;
    void Method(const MethodRegistration& registration) noexcept;
    void Event(const EventRegistration& registration) noexcept;
    void RoutedEvent(
        RoutedEventHandle declaredHandle,
        Base::StringView declarationName,
        TypeId eventArgsType,
        RoutingStrategy strategy) noexcept;
    void Content(Base::StringView name, ContentKind kind) noexcept;
    void Factory(ObjectFactory factory) noexcept;
    void Fail(Base::Status status) noexcept;

    MetaRegistrationContext& Context() noexcept { return *context_; }
    TypeId OwnerType() const noexcept { return ownerType_; }

private:
    MetaRegistrationContext* context_ = nullptr;
    TypeId ownerType_ = InvalidTypeId;
    Base::StringView xamlNamespace_;
    Base::StringView name_;
    TypeId baseType_ = InvalidTypeId;
    TypeFlags flags_ = TypeFlags::None;
    Base::Status status_;
    bool begun_ = false;

    void Record(Base::Result<void> result) noexcept;
    void RegisterDependencyProperty(
        DependencyPropertyHandle declaredHandle,
        Base::StringView declarationName,
        TypeId valueType,
        Value defaultValue,
        PropertyMetadataFlags metadataFlags,
        DependencyPropertyFlags propertyFlags,
        ValidateValueCallback validate,
        CoerceValueCallback coerce) noexcept;
};

// Registers built-in type/value/property metadata without freezing either
// registry. Applications register custom controls and properties afterwards.
AERO_API Base::Result<void>
TryRegisterPresentationMetadata(
    MetaRegistrationContext& context) noexcept;

} // namespace Aero::Core

#define AERO_IMPLEMENT_METADATA(classType, typeFlags) \
    Aero::Base::Result<void> classType::TryRegisterMetadata( \
        Aero::Core::MetaRegistrationContext& context) noexcept { \
        Aero::Core::MetaRegistrationBuilder helper( \
            context, StaticTypeId(), StaticMetadataNamespace(), \
            StaticMetadataName(), ParentClass::StaticTypeId(), typeFlags); \
        Aero::Base::Result<void> begun = helper.Begin(); \
        if (!begun) return begun.GetStatus(); \
        StaticFillMetadata(helper); \
        return helper.Finish(); \
    } \
    void classType::StaticFillMetadata( \
        Aero::Core::MetaRegistrationBuilder& helper) noexcept

#define AERO_IMPLEMENT_EMPTY_METADATA(classType, typeFlags) \
    AERO_IMPLEMENT_METADATA(classType, typeFlags) { (void)helper; }

#define AeroDP(property, ...) \
    helper.DependencyProperty( \
        SelfClass::property##Property, \
        Aero::Base::StringView(#property "Property"), __VA_ARGS__)
#define AeroAttachedDP(property, ...) \
    helper.AttachedDependencyProperty( \
        SelfClass::property##Property, \
        Aero::Base::StringView(#property "Property"), __VA_ARGS__)
#define AeroProp(...) helper.Property(__VA_ARGS__)
#define AeroMethod(...) helper.Method(__VA_ARGS__)
#define AeroEvent(event, ...) \
    helper.RoutedEvent( \
        SelfClass::event##Event, \
        Aero::Base::StringView(#event "Event"), __VA_ARGS__)
#define AeroMetaEvent(...) helper.Event(__VA_ARGS__)
#define AeroContent(...) helper.Content(__VA_ARGS__)
#define AeroFactory(...) helper.Factory(__VA_ARGS__)
