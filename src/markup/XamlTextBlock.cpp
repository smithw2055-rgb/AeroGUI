#include <Aero/Markup/XamlTextBlock.hpp>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

namespace Aero::Markup {
namespace {

Base::Status InvalidTextBlockXaml(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

} // namespace

XamlTextBlockExtension::XamlTextBlockExtension() noexcept = default;

Base::Result<void> XamlTextBlockExtension::TryRegisterType(
    const XamlTextBlockTypeRegistration& registration) noexcept {
    if (schema_ != nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "XAML TextBlock extension is already registered");
    }
    if (registration.type == Core::InvalidTypeId || registration.cast == nullptr) {
        return InvalidTextBlockXaml("XAML TextBlock type registration is invalid");
    }
    for (const XamlTextBlockTypeRegistration& current : types_) {
        if (current.type == registration.type) {
            return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
                "XAML TextBlock type is already registered");
        }
    }
    return types_.TryPushBack(registration);
}

Base::Result<std::uint32_t> XamlTextBlockExtension::Register(
    XamlSchemaContext& schema) noexcept {
    if (schema.IsFrozen() || schema_ != nullptr || types_.Empty() ||
        !schema.Types().IsFrozen()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "XAML TextBlock extension registries are not ready");
    }
    for (const XamlTextBlockTypeRegistration& registration : types_) {
        const Core::PropertyInfo* text = schema.Types().FindProperty(
            registration.type, Base::StringView("Text"), true);
        if (text == nullptr || IsTextMember(text->Id())) continue;
        if (schema.FindMemberAdapter(text->Id()) != nullptr) {
            return InvalidTextBlockXaml("XAML TextBlock Text metadata is invalid");
        }
        Base::Result<void> registered = schema.TryRegisterMemberAdapter({
            text->Id(), XamlMemberWriteMode::SetOnce, nullptr, this,
            &SetTextMember, true});
        if (!registered) return registered.GetStatus();
        Base::Result<void> appended = textMembers_.TryPushBack(text->Id());
        if (!appended) return appended.GetStatus();
    }
    if (textMembers_.Empty()) {
        return InvalidTextBlockXaml("XAML TextBlock Text metadata is invalid");
    }
    schema_ = &schema;
    return textMembers_.Size();
}

const XamlTextBlockTypeRegistration*
XamlTextBlockExtension::FindTypeRegistration(Core::TypeId type) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId && schema_ != nullptr) {
        for (const XamlTextBlockTypeRegistration& registration : types_) {
            if (registration.type == current) return &registration;
        }
        const Core::TypeInfo* info = schema_->Types().FindType(current);
        if (info == nullptr) break;
        current = info->BaseType();
    }
    return nullptr;
}

Base::Result<Core::TextBlock*> XamlTextBlockExtension::ResolveTextBlock(
    Base::Object& object,
    const XamlServiceProvider& services) const noexcept {
    if (schema_ == nullptr || services.targetObject != &object) {
        return InvalidTextBlockXaml("XAML TextBlock target object is invalid");
    }
    const XamlTextBlockTypeRegistration* registration = FindTypeRegistration(
        services.targetObjectType);
    if (registration == nullptr || registration->cast == nullptr) {
        return InvalidTextBlockXaml("XAML target is not a registered TextBlock");
    }
    Core::TextBlock* text = registration->cast(object, registration->context);
    if (text == nullptr || text->RuntimeType() != services.targetObjectType) {
        return InvalidTextBlockXaml(
            "XAML TextBlock runtime type does not match metadata");
    }
    return text;
}

bool XamlTextBlockExtension::IsTextMember(Core::MemberId member) const noexcept {
    for (Core::MemberId current : textMembers_) {
        if (current == member) return true;
    }
    return false;
}

Base::Result<void> XamlTextBlockExtension::SetTextMember(
    Base::Object& object,
    const XamlValue& value,
    const XamlServiceProvider& services,
    void* context) noexcept {
    auto* extension = static_cast<XamlTextBlockExtension*>(context);
    if (extension == nullptr || value.Kind() != XamlValueKind::String ||
        !extension->IsTextMember(services.targetMember)) {
        return InvalidTextBlockXaml("XAML TextBlock Text requires a string");
    }
    Base::Result<Core::TextBlock*> target = extension->ResolveTextBlock(
        object, services);
    return target ? target.Value()->SetText(value.AsString()) : target.GetStatus();
}

} // namespace Aero::Markup

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
