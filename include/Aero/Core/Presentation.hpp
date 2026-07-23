#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/BuiltinTypeIds.hpp>
#include <Aero/Core/DependencyProperty.hpp>

namespace Aero::Core {

class RoutedEventRegistry;
struct RoutedEventHandle;
enum class RoutingStrategy : std::uint8_t;

struct PresentationContext final {
    Dispatcher* dispatcher = nullptr;
    DependencyPropertyRegistry* dependencyProperties = nullptr;

    bool IsValid() const noexcept {
        return dispatcher != nullptr && dependencyProperties != nullptr;
    }
};

AERO_API PresentationContext
GetCurrentPresentationContext() noexcept;

class AERO_API PresentationContextScope final {
public:
    PresentationContextScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties) noexcept;
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

AERO_API Base::StringView
AeroPresentationNamespaceUri() noexcept;

// Compatibility catalog for code written before canonical BuiltinTypes IDs were
// exposed. It is no longer the source of metadata identity: every field is a
// stable constexpr ID and MetaRegistrationContext supplies this catalog by
// default. New code should use BuiltinTypes directly.
struct CorePresentationMetadata final {
    TypeId objectType = BuiltinTypes::Object;
    TypeId dependencyObjectType = BuiltinTypes::DependencyObject;
    TypeId treeNodeType = BuiltinTypes::TreeNode;
    TypeId layoutElementType = BuiltinTypes::LayoutElement;
    TypeId renderElementType = BuiltinTypes::RenderElement;
    TypeId stackPanelType = BuiltinTypes::StackPanel;
    TypeId canvasType = BuiltinTypes::Canvas;
    TypeId gridType = BuiltinTypes::Grid;
    TypeId borderType = BuiltinTypes::Border;
    TypeId textBlockType = BuiltinTypes::TextBlock;
    TypeId contentPresenterType = BuiltinTypes::ContentPresenter;
    TypeId booleanType = BuiltinTypes::Boolean;
    TypeId unsignedIntegerType = BuiltinTypes::UnsignedInteger;
    TypeId doubleType = BuiltinTypes::Double;
    TypeId stringType = BuiltinTypes::String;
    TypeId lengthType = BuiltinTypes::Length;
    TypeId thicknessType = BuiltinTypes::Thickness;
    TypeId colorType = BuiltinTypes::Color;
    TypeId horizontalAlignmentType = BuiltinTypes::HorizontalAlignment;
    TypeId verticalAlignmentType = BuiltinTypes::VerticalAlignment;
    TypeId orientationType = BuiltinTypes::Orientation;
    TypeId eventArgsType = BuiltinTypes::EventArgs;
    TypeId routedEventArgsType = BuiltinTypes::RoutedEventArgs;
    TypeId inputEventArgsType = BuiltinTypes::InputEventArgs;
    TypeId mouseEventArgsType = BuiltinTypes::MouseEventArgs;
    TypeId mouseButtonEventArgsType = BuiltinTypes::MouseButtonEventArgs;
    TypeId keyEventArgsType = BuiltinTypes::KeyEventArgs;
    TypeId textCompositionEventArgsType = BuiltinTypes::TextCompositionEventArgs;
    TypeId keyboardFocusChangedEventArgsType =
        BuiltinTypes::KeyboardFocusChangedEventArgs;
};

inline const CorePresentationMetadata&
BuiltinCorePresentationMetadata() noexcept {
    static constexpr CorePresentationMetadata metadata{};
    return metadata;
}

struct MetaRegistrationContext final {
    TypeRegistry& types;
    DependencyPropertyRegistry& dependencyProperties;
    const CorePresentationMetadata& core;
    RoutedEventRegistry* routedEvents = nullptr;

    MetaRegistrationContext(
        TypeRegistry& typeRegistry,
        DependencyPropertyRegistry& properties,
        RoutedEventRegistry* events = nullptr) noexcept
        : types(typeRegistry),
          dependencyProperties(properties),
          core(BuiltinCorePresentationMetadata()),
          routedEvents(events) {}

    MetaRegistrationContext(
        TypeRegistry& typeRegistry,
        DependencyPropertyRegistry& properties,
        const CorePresentationMetadata& compatibilityCatalog,
        RoutedEventRegistry* events = nullptr) noexcept
        : types(typeRegistry),
          dependencyProperties(properties),
          core(compatibilityCatalog),
          routedEvents(events) {}
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
// The returned catalog is retained for source compatibility; its IDs are the
// same canonical values exposed by BuiltinTypes.
AERO_API Base::Result<CorePresentationMetadata>
TryRegisterCorePresentationMetadata(
    TypeRegistry& types,
    DependencyPropertyRegistry& properties,
    RoutedEventRegistry* routedEvents = nullptr) noexcept;

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
