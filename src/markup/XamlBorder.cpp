#include <Aero/Markup/XamlBorder.hpp>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

namespace Aero::Markup {
namespace {

Base::Status InvalidBorderXaml(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

int HexDigit(char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

Base::Result<Core::Color> ParseColor(Base::StringView text) noexcept {
    if (text.SizeBytes() != 7U && text.SizeBytes() != 9U) {
        return InvalidBorderXaml("Border Background requires #RRGGBB or #AARRGGBB");
    }
    if (text[0] != '#') {
        return InvalidBorderXaml("Border Background requires a # color literal");
    }
    std::uint8_t bytes[4]{255U, 0U, 0U, 0U};
    const std::uint32_t componentCount = text.SizeBytes() == 9U ? 4U : 3U;
    for (std::uint32_t index = 0U; index < componentCount; ++index) {
        const std::uint32_t offset = 1U + index * 2U;
        const int high = HexDigit(text[offset]);
        const int low = HexDigit(text[offset + 1U]);
        if (high < 0 || low < 0) {
            return InvalidBorderXaml("Border Background color contains a non-hex digit");
        }
        bytes[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    if (componentCount == 3U) {
        return Core::Color{bytes[0] / 255.0F, bytes[1] / 255.0F,
            bytes[2] / 255.0F, 1.0F};
    }
    return Core::Color{bytes[1] / 255.0F, bytes[2] / 255.0F,
        bytes[3] / 255.0F, bytes[0] / 255.0F};
}

} // namespace

XamlBorderExtension::XamlBorderExtension(Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      types_(allocator_),
      backgroundMembers_(allocator_) {}

Base::Result<void> XamlBorderExtension::TryRegisterType(
    const XamlBorderTypeRegistration& registration) noexcept {
    if (schema_ != nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "XAML Border extension is already registered");
    }
    if (registration.type == Core::InvalidTypeId || registration.cast == nullptr) {
        return InvalidBorderXaml("XAML Border type registration is invalid");
    }
    for (const XamlBorderTypeRegistration& current : types_) {
        if (current.type == registration.type) {
            return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
                "XAML Border type is already registered");
        }
    }
    return types_.TryPushBack(registration);
}

Base::Result<std::uint32_t> XamlBorderExtension::Register(
    XamlSchemaContext& schema) noexcept {
    if (schema.IsFrozen() || schema_ != nullptr || types_.Empty() ||
        !schema.Types().IsFrozen()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "XAML Border extension registries are not ready");
    }
    for (const XamlBorderTypeRegistration& registration : types_) {
        const Core::PropertyInfo* background = schema.Types().FindProperty(
            registration.type, Base::StringView("Background"), true);
        if (background == nullptr || IsBackgroundMember(background->Id())) continue;
        if (schema.FindMemberAdapter(background->Id()) != nullptr) {
            return InvalidBorderXaml("XAML Border Background metadata is invalid");
        }
        Base::Result<void> registered = schema.TryRegisterMemberAdapter({
            background->Id(), XamlMemberWriteMode::SetOnce, nullptr, this,
            &SetBackgroundMember, true});
        if (!registered) return registered.GetStatus();
        Base::Result<void> appended = backgroundMembers_.TryPushBack(background->Id());
        if (!appended) return appended.GetStatus();
    }
    if (backgroundMembers_.Empty()) {
        return InvalidBorderXaml("XAML Border Background metadata is invalid");
    }
    schema_ = &schema;
    return backgroundMembers_.Size();
}

const XamlBorderTypeRegistration* XamlBorderExtension::FindTypeRegistration(
    Core::TypeId type) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId && schema_ != nullptr) {
        for (const XamlBorderTypeRegistration& registration : types_) {
            if (registration.type == current) return &registration;
        }
        const Core::TypeInfo* info = schema_->Types().FindType(current);
        if (info == nullptr) break;
        current = info->BaseType();
    }
    return nullptr;
}

Base::Result<Core::Border*> XamlBorderExtension::ResolveBorder(
    Base::Object& object, const XamlServiceProvider& services) const noexcept {
    if (schema_ == nullptr || services.targetObject != &object) {
        return InvalidBorderXaml("XAML Border target object is invalid");
    }
    const XamlBorderTypeRegistration* registration = FindTypeRegistration(
        services.targetObjectType);
    if (registration == nullptr || registration->cast == nullptr) {
        return InvalidBorderXaml("XAML target is not a registered Border");
    }
    Core::Border* border = registration->cast(object, registration->context);
    if (border == nullptr || border->RuntimeType() != services.targetObjectType) {
        return InvalidBorderXaml("XAML Border runtime type does not match metadata");
    }
    return border;
}

bool XamlBorderExtension::IsBackgroundMember(Core::MemberId member) const noexcept {
    for (Core::MemberId current : backgroundMembers_) {
        if (current == member) return true;
    }
    return false;
}

Base::Result<void> XamlBorderExtension::SetBackgroundMember(
    Base::Object& object, const XamlValue& value,
    const XamlServiceProvider& services, void* context) noexcept {
    auto* extension = static_cast<XamlBorderExtension*>(context);
    if (extension == nullptr || value.Kind() != XamlValueKind::String ||
        !extension->IsBackgroundMember(services.targetMember)) {
        return InvalidBorderXaml("XAML Border Background requires a string");
    }
    Base::Result<Core::Color> color = ParseColor(value.AsString());
    if (!color) return color.GetStatus();
    Base::Result<Core::Border*> target = extension->ResolveBorder(object, services);
    return target ? target.Value()->SetBackground(color.Value()) : target.GetStatus();
}

} // namespace Aero::Markup

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
