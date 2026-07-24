#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Presentation/Style.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlDependencyProperty.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {

// Bridges the declarative Style/Setter object model to the sealed Presentation::Style
// plan. Register it before XamlDependencyPropertyBridge so the configured
// Style property uses this owner-aware adapter rather than a plain object DP.
struct XamlStyleExtensionOptions final {
    Presentation::StyleManager* styles = nullptr;
    Core::DependencyPropertyRegistry* properties = nullptr;
    // Optional value type returned by XamlTypeExtension. When configured,
    // Style.TargetType accepts both a literal type name and `{x:Type ...}`.
    Core::TypeId typeReferenceType = Core::InvalidTypeId;
    XamlAsDependencyObjectCallback asDependencyObject = nullptr;
    void* castContext = nullptr;
};

// Supports the following initial, deterministic XAML subset:
//
//   <Style x:Key="Card" TargetType="local:Card">
//     <Setter Property="Width" Value="240"/>
//   </Style>
//   <Card Style="{StaticResource Card}"/>
//
// TargetType accepts an unqualified name in the active default XML namespace,
// or a prefix-qualified name. Setter values are converted using the target
// dependency property's registered scalar converter. Object-valued setters
// and triggers/templates intentionally remain later XAML slices.
class AERO_API XamlStyleExtension final {
public:
    explicit XamlStyleExtension(
        const XamlStyleExtensionOptions& options) noexcept;
    ~XamlStyleExtension() noexcept;

    XamlStyleExtension(const XamlStyleExtension&) = delete;
    XamlStyleExtension& operator=(const XamlStyleExtension&) = delete;

    Base::Result<void> Register(
        XamlSchemaContext& schema,
        XamlActivationProviderRegistry& activation,
        Core::TypeId styleType,
        Core::TypeId setterType,
        Core::DependencyPropertyHandle styleProperty) noexcept;

    void SetTypeReferenceType(Core::TypeId type) noexcept {
        options_.typeReferenceType = type;
    }

    // Call before a DependencyObject is destroyed. This clears Presentation::Style
    // providers and releases the retained XAML Style object atomically.
    Base::Result<bool> DetachObject(
        Core::DependencyObject& object) noexcept;

private:
    class StyleObject;
    class SetterObject;

    struct Application final {
        Core::DependencyObject* object = nullptr;
        Base::Ref<Base::Object> style;
    };

    XamlStyleExtensionOptions options_;
    XamlSchemaContext* schema_ = nullptr;
    Base::Vector<Application> applications_;
    Core::TypeId styleType_ = Core::InvalidTypeId;
    Core::TypeId setterType_ = Core::InvalidTypeId;
    Core::DependencyPropertyHandle styleProperty_;
    Core::MemberId targetTypeMember_ = Core::InvalidMemberId;
    Core::MemberId basedOnMember_ = Core::InvalidMemberId;
    Core::MemberId settersMember_ = Core::InvalidMemberId;
    Core::MemberId setterPropertyMember_ = Core::InvalidMemberId;
    Core::MemberId setterValueMember_ = Core::InvalidMemberId;

    Base::Result<void> FinalizeStyle(
        StyleObject& style) noexcept;
    Base::Result<void> ApplyStyle(
        Core::DependencyObject& object,
        const Base::Ref<Base::Object>& style) noexcept;
    std::uint32_t FindApplication(
        const Core::DependencyObject& object) const noexcept;
    void RemoveApplication(std::uint32_t index) noexcept;

    static Base::Result<Base::Ref<Base::Object>>
    ActivateStyle(
        Core::TypeId requestedType,
        const XamlActivationContext& activation,
        void* context) noexcept;
    static Base::Result<Base::Ref<Base::Object>>
    ActivateSetter(
        Core::TypeId requestedType,
        const XamlActivationContext& activation,
        void* context) noexcept;

    static Base::Result<void> SetTargetType(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept;
    static Base::Result<void> SetBasedOn(
        Base::Object& object,
        const XamlValue& value,
        void* context) noexcept;
    static Base::Result<void> AddSetter(
        Base::Object& object,
        const XamlValue& value,
        void* context) noexcept;
    static Base::Result<void> SetSetterProperty(
        Base::Object& object,
        const XamlValue& value,
        void* context) noexcept;
    static Base::Result<void> SetSetterValue(
        Base::Object& object,
        const XamlValue& value,
        void* context) noexcept;
    static Base::Result<void> EndStyleInit(
        Base::Object& object,
        void* context) noexcept;
    static Base::Result<void> SetStyleMember(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept;
};

} // namespace Aero::Markup
