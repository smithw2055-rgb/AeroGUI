#include <Aero/Markup/XamlLayout.hpp>

#include <Aero/Base/String.hpp>

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace Aero::Markup {
namespace {

Base::Status InvalidLayoutXaml(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

bool IsWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

bool EqualsAsciiInsensitive(Base::StringView value, const char* literal) noexcept {
    std::uint32_t length = 0U;
    while (literal[length] != '\0') ++length;
    if (value.SizeBytes() != length) return false;
    for (std::uint32_t index = 0U; index < length; ++index) {
        const unsigned char left = static_cast<unsigned char>(value[index]);
        const unsigned char right = static_cast<unsigned char>(literal[index]);
        if (std::tolower(left) != std::tolower(right)) return false;
    }
    return true;
}

Base::Result<double> NumericValue(const XamlValue& value) noexcept {
    double result = 0.0;
    switch (value.Kind()) {
    case XamlValueKind::Double: result = value.AsDouble(); break;
    case XamlValueKind::SignedInteger: result = static_cast<double>(value.AsSignedInteger()); break;
    case XamlValueKind::UnsignedInteger: result = static_cast<double>(value.AsUnsignedInteger()); break;
    default: return InvalidLayoutXaml("Layout dimension requires a numeric XAML value");
    }
    if (!std::isfinite(result)) {
        return InvalidLayoutXaml("Layout dimension must be finite");
    }
    return result;
}

Base::Result<Core::Thickness> ParseMargin(Base::StringView input) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.TryAssign(input);
    if (!assigned) return assigned.GetStatus();
    const char* cursor = text.CStr();
    double values[4]{};
    std::uint32_t count = 0U;
    while (*cursor != '\0') {
        while (IsWhitespace(*cursor)) ++cursor;
        if (*cursor == '\0') break;
        if (count == 4U) return InvalidLayoutXaml("Margin accepts one, two, or four values");
        char* end = nullptr;
        values[count] = std::strtod(cursor, &end);
        if (end == cursor || !std::isfinite(values[count])) {
            return InvalidLayoutXaml("Margin contains an invalid number");
        }
        ++count;
        cursor = end;
        while (IsWhitespace(*cursor)) ++cursor;
        if (*cursor == ',') ++cursor;
        else if (*cursor != '\0' && !IsWhitespace(*cursor)) {
            return InvalidLayoutXaml("Margin values must be comma or whitespace separated");
        }
    }
    if (count == 1U) return Core::Thickness{values[0], values[0], values[0], values[0]};
    if (count == 2U) return Core::Thickness{values[0], values[1], values[0], values[1]};
    if (count == 4U) return Core::Thickness{values[0], values[1], values[2], values[3]};
    return InvalidLayoutXaml("Margin is empty");
}

} // namespace

XamlLayoutExtension::XamlLayoutExtension(
    Core::TypeId layoutElementType,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      types_(allocator_),
      layoutElementType_(layoutElementType) {}

Base::Result<void> XamlLayoutExtension::TryRegisterType(
    const XamlLayoutTypeRegistration& registration) noexcept {
    if (schema_ != nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "XAML Layout extension is already registered");
    }
    if (registration.type == Core::InvalidTypeId || registration.cast == nullptr) {
        return InvalidLayoutXaml("XAML Layout type registration is invalid");
    }
    for (const XamlLayoutTypeRegistration& current : types_) {
        if (current.type == registration.type) {
            return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
                "XAML Layout type is already registered");
        }
    }
    return types_.TryPushBack(registration);
}

Base::Result<std::uint32_t> XamlLayoutExtension::Register(
    XamlSchemaContext& schema) noexcept {
    if (schema.IsFrozen() || schema_ != nullptr || layoutElementType_ == Core::InvalidTypeId ||
        !schema.Types().IsFrozen()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "XAML Layout extension registries are not ready");
    }
    const Core::TypeInfo* layout = schema.Types().FindType(layoutElementType_);
    if (layout == nullptr || (static_cast<std::uint32_t>(layout->Flags()) &
        static_cast<std::uint32_t>(Core::TypeFlags::ValueType)) != 0U) {
        return InvalidLayoutXaml("XAML LayoutElement metadata is invalid");
    }
    const Core::PropertyInfo* width = schema.Types().FindProperty(
        layoutElementType_, Base::StringView("Width"), false);
    const Core::PropertyInfo* height = schema.Types().FindProperty(
        layoutElementType_, Base::StringView("Height"), false);
    const Core::PropertyInfo* minWidth = schema.Types().FindProperty(
        layoutElementType_, Base::StringView("MinWidth"), false);
    const Core::PropertyInfo* maxWidth = schema.Types().FindProperty(
        layoutElementType_, Base::StringView("MaxWidth"), false);
    const Core::PropertyInfo* minHeight = schema.Types().FindProperty(
        layoutElementType_, Base::StringView("MinHeight"), false);
    const Core::PropertyInfo* maxHeight = schema.Types().FindProperty(
        layoutElementType_, Base::StringView("MaxHeight"), false);
    const Core::PropertyInfo* margin = schema.Types().FindProperty(
        layoutElementType_, Base::StringView("Margin"), false);
    const Core::PropertyInfo* horizontal = schema.Types().FindProperty(
        layoutElementType_, Base::StringView("HorizontalAlignment"), false);
    const Core::PropertyInfo* vertical = schema.Types().FindProperty(
        layoutElementType_, Base::StringView("VerticalAlignment"), false);
    if (width == nullptr || height == nullptr || minWidth == nullptr || maxWidth == nullptr ||
        minHeight == nullptr || maxHeight == nullptr || margin == nullptr ||
        horizontal == nullptr || vertical == nullptr) {
        return InvalidLayoutXaml("XAML LayoutElement members are incomplete");
    }
    widthMember_ = width->Id(); heightMember_ = height->Id();
    minWidthMember_ = minWidth->Id(); maxWidthMember_ = maxWidth->Id();
    minHeightMember_ = minHeight->Id(); maxHeightMember_ = maxHeight->Id();
    marginMember_ = margin->Id(); horizontalAlignmentMember_ = horizontal->Id();
    verticalAlignmentMember_ = vertical->Id();
    const Core::MemberId members[] = {widthMember_, heightMember_, minWidthMember_,
        maxWidthMember_, minHeightMember_, maxHeightMember_, marginMember_,
        horizontalAlignmentMember_, verticalAlignmentMember_};
    for (Core::MemberId member : members) {
        if (schema.FindMemberAdapter(member) != nullptr) {
            return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
                "XAML Layout member already has an adapter");
        }
        Base::Result<void> registered = schema.TryRegisterMemberAdapter({
            member, XamlMemberWriteMode::SetOnce, nullptr, this, &SetLayoutMember, true});
        if (!registered) return registered.GetStatus();
    }
    schema_ = &schema;
    return static_cast<std::uint32_t>(sizeof(members) / sizeof(members[0]));
}

const XamlLayoutTypeRegistration* XamlLayoutExtension::FindTypeRegistration(
    Core::TypeId type) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId) {
        for (const XamlLayoutTypeRegistration& registration : types_) {
            if (registration.type == current) return &registration;
        }
        const Core::TypeInfo* info = schema_->Types().FindType(current);
        if (info == nullptr) break;
        current = info->BaseType();
    }
    return nullptr;
}

Base::Result<Core::LayoutElement*> XamlLayoutExtension::ResolveElement(
    Base::Object& object, const XamlServiceProvider& services) const noexcept {
    if (services.targetObject != &object || schema_ == nullptr) {
        return InvalidLayoutXaml("XAML Layout target object is invalid");
    }
    const XamlLayoutTypeRegistration* registration = FindTypeRegistration(
        services.targetObjectType);
    if (registration == nullptr || registration->cast == nullptr) {
        return InvalidLayoutXaml("XAML target is not a registered LayoutElement");
    }
    Core::LayoutElement* element = registration->cast(object, registration->context);
    if (element == nullptr || element->RuntimeType() != services.targetObjectType) {
        return InvalidLayoutXaml("XAML Layout target runtime type does not match metadata");
    }
    return element;
}

Base::Result<void> XamlLayoutExtension::SetLayoutMember(
    Base::Object& object, const XamlValue& value,
    const XamlServiceProvider& services, void* context) noexcept {
    auto* extension = static_cast<XamlLayoutExtension*>(context);
    if (extension == nullptr) return InvalidLayoutXaml("XAML Layout extension is unavailable");
    Base::Result<Core::LayoutElement*> target = extension->ResolveElement(object, services);
    if (!target) return target.GetStatus();
    Core::LayoutElement& element = *target.Value();
    const Core::MemberId member = services.targetMember;
    if (member == extension->widthMember_) {
        Base::Result<double> number = NumericValue(value); if (!number) return number.GetStatus();
        return element.SetWidth(number.Value());
    }
    if (member == extension->heightMember_) {
        Base::Result<double> number = NumericValue(value); if (!number) return number.GetStatus();
        return element.SetHeight(number.Value());
    }
    if (member == extension->minWidthMember_ || member == extension->minHeightMember_) {
        Base::Result<double> number = NumericValue(value); if (!number) return number.GetStatus();
        Core::Size size = element.MinSize();
        if (member == extension->minWidthMember_) size.width = number.Value();
        else size.height = number.Value();
        return element.SetMinSize(size);
    }
    if (member == extension->maxWidthMember_ || member == extension->maxHeightMember_) {
        Base::Result<double> number = NumericValue(value); if (!number) return number.GetStatus();
        Core::Size size = element.MaxSize();
        if (member == extension->maxWidthMember_) size.width = number.Value();
        else size.height = number.Value();
        return element.SetMaxSize(size);
    }
    if (member == extension->marginMember_) {
        if (value.Kind() == XamlValueKind::Double || value.Kind() == XamlValueKind::SignedInteger ||
            value.Kind() == XamlValueKind::UnsignedInteger) {
            Base::Result<double> number = NumericValue(value); if (!number) return number.GetStatus();
            return element.SetMargin({number.Value(), number.Value(), number.Value(), number.Value()});
        }
        if (value.Kind() != XamlValueKind::String) return InvalidLayoutXaml("Margin requires text or a number");
        Base::Result<Core::Thickness> margin = ParseMargin(value.AsString());
        if (!margin) return margin.GetStatus();
        return element.SetMargin(margin.Value());
    }
    if (member == extension->horizontalAlignmentMember_) {
        if (value.Kind() != XamlValueKind::String) return InvalidLayoutXaml("HorizontalAlignment requires text");
        const Base::StringView text = value.AsString();
        if (EqualsAsciiInsensitive(text, "Stretch")) return element.SetHorizontalAlignment(Core::HorizontalAlignment::Stretch);
        if (EqualsAsciiInsensitive(text, "Left")) return element.SetHorizontalAlignment(Core::HorizontalAlignment::Left);
        if (EqualsAsciiInsensitive(text, "Center")) return element.SetHorizontalAlignment(Core::HorizontalAlignment::Center);
        if (EqualsAsciiInsensitive(text, "Right")) return element.SetHorizontalAlignment(Core::HorizontalAlignment::Right);
        return InvalidLayoutXaml("HorizontalAlignment is invalid");
    }
    if (member == extension->verticalAlignmentMember_) {
        if (value.Kind() != XamlValueKind::String) return InvalidLayoutXaml("VerticalAlignment requires text");
        const Base::StringView text = value.AsString();
        if (EqualsAsciiInsensitive(text, "Stretch")) return element.SetVerticalAlignment(Core::VerticalAlignment::Stretch);
        if (EqualsAsciiInsensitive(text, "Top")) return element.SetVerticalAlignment(Core::VerticalAlignment::Top);
        if (EqualsAsciiInsensitive(text, "Center")) return element.SetVerticalAlignment(Core::VerticalAlignment::Center);
        if (EqualsAsciiInsensitive(text, "Bottom")) return element.SetVerticalAlignment(Core::VerticalAlignment::Bottom);
        return InvalidLayoutXaml("VerticalAlignment is invalid");
    }
    return Base::Status::Failure(Base::ErrorCode::NotFound, "XAML Layout member is not registered");
}

} // namespace Aero::Markup
