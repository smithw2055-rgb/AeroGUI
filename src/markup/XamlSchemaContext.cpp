#include <Aero/Markup/XamlSchemaContext.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace Aero::Markup {
namespace {

constexpr const char* MessageSchemaNotFrozen =
    "XAML schema context must be frozen before use";
constexpr const char* MessageSchemaAlreadyFrozen =
    "XAML schema context is frozen";
constexpr const char* MessageTypeNotFound =
    "XAML type is not registered";
constexpr const char* MessageMemberNotFound =
    "XAML member is not registered for the target type";
constexpr const char* MessageInvalidAttachedMember =
    "Qualified XAML attribute does not resolve to an attached member";
constexpr const char* MessageInvalidPropertyElement =
    "XAML property element owner is not compatible with the target type";
constexpr const char* MessageTypeNotConstructible =
    "XAML type is not constructible";
constexpr const char* MessageInvalidScalarType =
    "XAML scalar registration requires a registered value type";
constexpr const char* MessageInvalidScalarValue =
    "XAML text cannot be converted to the requested scalar type";
constexpr const char* MessageMissingScalarConverter =
    "XAML value type has no registered scalar converter";
constexpr const char* MessageMissingMemberAdapter =
    "XAML member has no registered write adapter";
constexpr const char* MessageMemberTypeMismatch =
    "XAML value is not compatible with the member value type";
constexpr const char* MessageUnsupportedEvent =
    "XAML event assignment is not implemented in this object-writer slice";
constexpr const char* MessageInvalidContentMember =
    "XAML type adapter content member is invalid";
constexpr const char* MessageContentMemberNotFound =
    "XAML type has no content member";
constexpr const char* MessageServicesRequired =
    "XAML member adapter requires a service-provider context";
constexpr const char* MessageInvalidMarkupExtension =
    "XAML markup-extension registration requires a flagged type and provider";
constexpr const char* MessageMissingMarkupExtension =
    "XAML markup-extension type has no registered value provider";

bool HasTypeFlag(Core::TypeFlags value, Core::TypeFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool HasPropertyFlag(
    Core::PropertyFlags value,
    Core::PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool HasEventFlag(Core::EventFlags value, Core::EventFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) &
        static_cast<std::uint32_t>(flag)) != 0U;
}

bool IsAsciiWhitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

Base::StringView TrimAscii(Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    while (begin < value.SizeBytes() && IsAsciiWhitespace(value[begin])) {
        ++begin;
    }

    std::uint32_t end = value.SizeBytes();
    while (end > begin && IsAsciiWhitespace(value[end - 1U])) {
        --end;
    }
    return value.Substr(begin, end - begin);
}

char ToAsciiLower(char value) noexcept {
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value - 'A' + 'a')
        : value;
}

bool EqualsAsciiInsensitive(
    Base::StringView value,
    const char* literal,
    std::uint32_t literalLength) noexcept {
    if (value.SizeBytes() != literalLength) {
        return false;
    }
    for (std::uint32_t index = 0U; index < literalLength; ++index) {
        if (ToAsciiLower(value[index]) != literal[index]) {
            return false;
        }
    }
    return true;
}

Base::Result<std::uint64_t> ParseUnsignedMagnitude(
    Base::StringView value,
    std::uint64_t limit) noexcept {
    if (value.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            MessageInvalidScalarValue);
    }

    std::uint64_t result = 0U;
    for (char character : value) {
        if (character < '0' || character > '9') {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageInvalidScalarValue);
        }
        const std::uint64_t digit =
            static_cast<std::uint64_t>(character - '0');
        if (result > (limit - digit) / 10U) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                MessageInvalidScalarValue);
        }
        result = result * 10U + digit;
    }
    return result;
}

Base::Result<std::int64_t> ParseSignedInteger(
    Base::StringView text) noexcept {
    Base::StringView value = TrimAscii(text);
    bool negative = false;
    if (!value.Empty() && (value[0] == '+' || value[0] == '-')) {
        negative = value[0] == '-';
        value = value.Substr(1U, value.SizeBytes() - 1U);
    }

    const std::uint64_t negativeLimit =
        static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max()) + 1U;
    const std::uint64_t limit = negative
        ? negativeLimit
        : static_cast<std::uint64_t>(
            std::numeric_limits<std::int64_t>::max());
    Base::Result<std::uint64_t> magnitude =
        ParseUnsignedMagnitude(value, limit);
    if (!magnitude) {
        return magnitude.GetStatus();
    }

    if (!negative) {
        return static_cast<std::int64_t>(magnitude.Value());
    }
    if (magnitude.Value() == negativeLimit) {
        return std::numeric_limits<std::int64_t>::min();
    }
    return -static_cast<std::int64_t>(magnitude.Value());
}

Base::Result<std::uint64_t> ParseUnsignedInteger(
    Base::StringView text) noexcept {
    Base::StringView value = TrimAscii(text);
    if (!value.Empty() && value[0] == '+') {
        value = value.Substr(1U, value.SizeBytes() - 1U);
    }
    if (!value.Empty() && value[0] == '-') {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            MessageInvalidScalarValue);
    }
    return ParseUnsignedMagnitude(
        value,
        std::numeric_limits<std::uint64_t>::max());
}

Base::Result<double> ParseDouble(Base::StringView text) noexcept {
    const Base::StringView value = TrimAscii(text);
    if (value.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            MessageInvalidScalarValue);
    }

    std::uint32_t index = 0U;
    bool negative = false;
    if (value[index] == '+' || value[index] == '-') {
        negative = value[index] == '-';
        ++index;
    }

    double result = 0.0;
    bool sawDigit = false;
    while (index < value.SizeBytes() &&
        value[index] >= '0' && value[index] <= '9') {
        sawDigit = true;
        result = result * 10.0 +
            static_cast<double>(value[index] - '0');
        if (!std::isfinite(result)) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                MessageInvalidScalarValue);
        }
        ++index;
    }

    if (index < value.SizeBytes() && value[index] == '.') {
        ++index;
        double place = 0.1;
        while (index < value.SizeBytes() &&
            value[index] >= '0' && value[index] <= '9') {
            sawDigit = true;
            result += static_cast<double>(value[index] - '0') * place;
            place *= 0.1;
            ++index;
        }
    }

    if (!sawDigit) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            MessageInvalidScalarValue);
    }

    if (index < value.SizeBytes() &&
        (value[index] == 'e' || value[index] == 'E')) {
        ++index;
        bool exponentNegative = false;
        if (index < value.SizeBytes() &&
            (value[index] == '+' || value[index] == '-')) {
            exponentNegative = value[index] == '-';
            ++index;
        }
        if (index >= value.SizeBytes() ||
            value[index] < '0' || value[index] > '9') {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                MessageInvalidScalarValue);
        }

        std::uint32_t exponent = 0U;
        while (index < value.SizeBytes() &&
            value[index] >= '0' && value[index] <= '9') {
            const std::uint32_t digit =
                static_cast<std::uint32_t>(value[index] - '0');
            if (exponent > 10000U) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    MessageInvalidScalarValue);
            }
            exponent = exponent * 10U + digit;
            ++index;
        }

        const double scale = std::pow(
            10.0,
            exponentNegative
                ? -static_cast<double>(exponent)
                : static_cast<double>(exponent));
        result *= scale;
    }

    if (index != value.SizeBytes() || !std::isfinite(result)) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            MessageInvalidScalarValue);
    }
    return negative ? -result : result;
}

} // namespace

XamlSchemaContext::XamlSchemaContext(
    Core::TypeRegistry& types,
    Base::IAllocator* allocator) noexcept
    : types_(&types),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      memberAccessor_(types, allocator_),
      scalarTypes_(allocator_),
      textConverters_(allocator_),
      memberAdapters_(allocator_),
      memberProviders_(allocator_),
      typeAdapters_(allocator_),
      markupExtensions_(allocator_) {}

Base::Result<void> XamlSchemaContext::TryRegisterScalarType(
    Core::TypeId type,
    XamlScalarKind kind) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaAlreadyFrozen);
    }
    const Core::TypeInfo* info = types_->FindType(type);
    if (info == nullptr ||
        !HasTypeFlag(info->Flags(), Core::TypeFlags::ValueType) ||
        static_cast<std::uint8_t>(kind) >
            static_cast<std::uint8_t>(XamlScalarKind::Double)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidScalarType);
    }
    if (FindScalarType(type) != nullptr || FindTextConverter(type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML scalar converter is already registered");
    }
    return scalarTypes_.TryPushBack({type, kind});
}

Base::Result<void> XamlSchemaContext::TryRegisterTextConverter(
    const XamlTextConverterRegistration& registration) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaAlreadyFrozen);
    }
    const Core::TypeInfo* info = types_->FindType(registration.type);
    if (info == nullptr || registration.convert == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML text converter registration is invalid");
    }
    if (FindScalarType(registration.type) != nullptr ||
        FindTextConverter(registration.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML text converter is already registered");
    }
    return textConverters_.TryPushBack(registration);
}

Base::Result<void> XamlSchemaContext::TryRegisterMemberAdapter(
    const XamlMemberAdapterRegistration& registration) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaAlreadyFrozen);
    }
    if (registration.member == Core::InvalidMemberId ||
        (registration.set == nullptr && registration.setWithServices == nullptr) ||
        types_->FindProperty(registration.member) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML member adapter registration is invalid");
    }
    if (FindMemberAdapter(registration.member) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML member adapter is already registered");
    }
    return memberAdapters_.TryPushBack(registration);
}

Base::Result<void> XamlSchemaContext::TryRegisterMemberProvider(
    const XamlMemberProviderRegistration& registration) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaAlreadyFrozen);
    }
    if (registration.handles == nullptr || registration.set == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML member provider registration is invalid");
    }
    for (const XamlMemberProviderRegistration& current : memberProviders_) {
        if (current.handles == registration.handles &&
            current.set == registration.set &&
            current.context == registration.context) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "XAML member provider is already registered");
        }
    }
    return memberProviders_.TryPushBack(registration);
}

Base::Result<void> XamlSchemaContext::TryRegisterTypeAdapter(
    const XamlTypeAdapterRegistration& registration) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaAlreadyFrozen);
    }
    const Core::TypeInfo* type = types_->FindType(registration.type);
    if (type == nullptr ||
        HasTypeFlag(type->Flags(), Core::TypeFlags::ValueType)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML type adapter registration is invalid");
    }
    if (FindTypeAdapterExact(registration.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML type adapter is already registered");
    }
    if (registration.contentMember != Core::InvalidMemberId) {
        const Core::PropertyInfo* content =
            types_->FindProperty(registration.contentMember);
        if (content == nullptr ||
            (!HasPropertyFlag(content->Flags(), Core::PropertyFlags::Attached) &&
             !types_->IsDerivedFrom(
                 registration.type,
                 content->OwnerType()))) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                MessageInvalidContentMember);
        }
    }
    return typeAdapters_.TryPushBack(registration);
}

Base::Result<void> XamlSchemaContext::TryRegisterMarkupExtension(
    const XamlMarkupExtensionRegistration& registration) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaAlreadyFrozen);
    }
    const Core::TypeInfo* type = types_->FindType(registration.type);
    if (type == nullptr || registration.provideValue == nullptr ||
        !HasTypeFlag(type->Flags(), Core::TypeFlags::MarkupExtension)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageInvalidMarkupExtension);
    }
    if (FindMarkupExtension(registration.type) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML markup-extension provider is already registered");
    }
    return markupExtensions_.TryPushBack(registration);
}

Base::Result<void> XamlSchemaContext::Freeze() noexcept {
    if (frozen_) {
        return {};
    }
    if (!types_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TypeRegistry must be frozen before the XAML schema context");
    }
    Base::Result<void> accessors = memberAccessor_.Freeze();
    if (!accessors) return accessors.GetStatus();
    frozen_ = true;
    return {};
}

Base::Result<const Core::TypeInfo*> XamlSchemaContext::ResolveType(
    Base::StringView xamlNamespace,
    Base::StringView localName) const noexcept {
    if (!frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    const Core::TypeInfo* type =
        types_->FindType(xamlNamespace, localName);
    if (type == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            MessageTypeNotFound);
    }
    return type;
}

Base::Result<XamlResolvedMember> XamlSchemaContext::ResolveMember(
    Core::TypeId targetType,
    const XamlQualifiedName& name,
    XamlMemberSyntax syntax) const noexcept {
    if (!frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    const Core::TypeInfo* target = types_->FindType(targetType);
    if (target == nullptr || name.LocalName().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageMemberNotFound);
    }

    const Base::StringView localName = name.LocalName();
    std::uint32_t dot = localName.SizeBytes();
    for (std::uint32_t index = 0U;
         index < localName.SizeBytes();
         ++index) {
        if (localName[index] != '.') {
            continue;
        }
        if (dot != localName.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                MessageMemberNotFound);
        }
        dot = index;
    }

    if (dot == localName.SizeBytes()) {
        if (!name.NamespaceUri().Empty() &&
            name.NamespaceUri() != target->XamlNamespace()) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                MessageMemberNotFound);
        }
        return ResolvePropertyOrEvent(
            targetType,
            targetType,
            localName,
            syntax,
            false);
    }

    if (dot == 0U || dot + 1U >= localName.SizeBytes()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageMemberNotFound);
    }

    const Base::StringView ownerName = localName.Substr(0U, dot);
    const Base::StringView memberName = localName.Substr(
        dot + 1U,
        localName.SizeBytes() - dot - 1U);
    const Base::StringView ownerNamespace = name.NamespaceUri().Empty()
        ? target->XamlNamespace()
        : name.NamespaceUri();
    const Core::TypeInfo* owner =
        types_->FindType(ownerNamespace, ownerName);
    if (owner == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            MessageMemberNotFound);
    }
    return ResolvePropertyOrEvent(
        targetType,
        owner->Id(),
        memberName,
        syntax,
        true);
}

Base::Result<XamlResolvedMember> XamlSchemaContext::ResolveContentMember(
    Core::TypeId targetType) const noexcept {
    if (!frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    Core::MemberId contentMember = types_->FindContentMember(targetType);
    // Deprecated compatibility fallback for one release cycle.
    if (contentMember == Core::InvalidMemberId) {
        const XamlTypeAdapterRegistration* adapter = FindTypeAdapter(targetType);
        if (adapter != nullptr) contentMember = adapter->contentMember;
    }
    if (contentMember == Core::InvalidMemberId) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            MessageContentMemberNotFound);
    }

    const Core::PropertyInfo* property =
        types_->FindProperty(contentMember);
    if (property == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageInvalidContentMember);
    }

    XamlResolvedMember resolved;
    resolved.id = property->Id();
    resolved.kind = Core::MemberKind::Property;
    resolved.ownerType = property->OwnerType();
    resolved.valueType = property->ValueType();
    resolved.propertyFlags = property->Flags();
    resolved.attached = HasPropertyFlag(
        property->Flags(),
        Core::PropertyFlags::Attached);
    return resolved;
}

Base::Result<Base::Ref<Base::Object>> XamlSchemaContext::CreateObject(
    Core::TypeId type) const noexcept {
    if (!frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    const Core::TypeInfo* info = types_->FindType(type);
    if (info == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            MessageTypeNotFound);
    }
    if (HasTypeFlag(info->Flags(), Core::TypeFlags::Abstract) ||
        HasTypeFlag(info->Flags(), Core::TypeFlags::ValueType) ||
        info->Factory() == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            MessageTypeNotConstructible);
    }

    Base::Result<Base::Ref<Base::Object>> created =
        info->Factory()(*allocator_);
    if (!created) {
        return created.GetStatus();
    }
    if (!created.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "XAML object factory returned a null object");
    }
    return created;
}

Base::Result<XamlValue> XamlSchemaContext::ConvertText(
    Core::TypeId type,
    Base::StringView text) const noexcept {
    if (!frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    Base::Result<Core::Value> reflected = types_->TryConvertText(
        type, text, allocator_);
    if (reflected) {
        return reflected;
    }
    if (reflected.GetStatus().code != Base::ErrorCode::NotFound) {
        return reflected.GetStatus();
    }
    const XamlTextConverterRegistration* converter = FindTextConverter(type);
    if (converter != nullptr) {
        Base::Result<XamlValue> converted = converter->convert(
            type, text, *allocator_, converter->context);
        if (!converted) {
            return converted.GetStatus();
        }
        if (converted.Value().Type() != type ||
            converted.Value().Kind() == XamlValueKind::None) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML text converter returned an incompatible value");
        }
        return converted;
    }
    const ScalarRegistration* scalar = FindScalarType(type);
    if (scalar == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            MessageMissingScalarConverter);
    }

    switch (scalar->kind) {
    case XamlScalarKind::String:
        return XamlValue::TryFromString(type, text, allocator_);
    case XamlScalarKind::Boolean: {
        const Base::StringView value = TrimAscii(text);
        if (EqualsAsciiInsensitive(value, "true", 4U)) {
            return XamlValue::FromBoolean(type, true, allocator_);
        }
        if (EqualsAsciiInsensitive(value, "false", 5U)) {
            return XamlValue::FromBoolean(type, false, allocator_);
        }
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            MessageInvalidScalarValue);
    }
    case XamlScalarKind::SignedInteger: {
        Base::Result<std::int64_t> parsed = ParseSignedInteger(text);
        if (!parsed) {
            return parsed.GetStatus();
        }
        return XamlValue::FromSignedInteger(
            type,
            parsed.Value(),
            allocator_);
    }
    case XamlScalarKind::UnsignedInteger: {
        Base::Result<std::uint64_t> parsed = ParseUnsignedInteger(text);
        if (!parsed) {
            return parsed.GetStatus();
        }
        return XamlValue::FromUnsignedInteger(
            type,
            parsed.Value(),
            allocator_);
    }
    case XamlScalarKind::Double: {
        Base::Result<double> parsed = ParseDouble(text);
        if (!parsed) {
            return parsed.GetStatus();
        }
        return XamlValue::FromDouble(type, parsed.Value(), allocator_);
    }
    }

    return Base::Status::Failure(
        Base::ErrorCode::InternalError,
        MessageInvalidScalarValue);
}

Base::Result<void> XamlSchemaContext::SetMember(
    Base::Object& object,
    Core::TypeId objectType,
    const XamlResolvedMember& member,
    const XamlValue& value,
    const XamlServiceProvider* services) const noexcept {
    if (!frozen_ || !member.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    if (member.kind != Core::MemberKind::Property) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            MessageUnsupportedEvent);
    }
    if (!member.attached &&
        !types_->IsDerivedFrom(objectType, member.ownerType)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            MessageMemberTypeMismatch);
    }

    const XamlMemberAdapterRegistration* adapter = FindMemberAdapter(member.id);
    const Core::PropertyInfo* metaProperty = types_->FindProperty(member.id);
    const bool metaWritable = adapter == nullptr && metaProperty != nullptr &&
        metaProperty->Access() != Core::PropertyAccessKind::External;
    const XamlMemberProviderRegistration* provider =
        adapter == nullptr && !metaWritable ? FindMemberProvider(member) : nullptr;
    if ((adapter == nullptr ||
         (adapter->set == nullptr && adapter->setWithServices == nullptr)) &&
        !metaWritable && provider == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            MessageMissingMemberAdapter);
    }
    const bool acceptsAnyValue = adapter != nullptr
        ? adapter->acceptsAnyValue
        : (metaWritable ? false : provider->acceptsAnyValue);
    if (!acceptsAnyValue) {
        bool compatible = value.Type() == member.valueType;
        if (value.Kind() == XamlValueKind::Object && value.AsObject()) {
            compatible = types_->IsDerivedFrom(
                value.Type(),
                member.valueType);
        }
        if (!compatible) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                MessageMemberTypeMismatch);
        }
    }
    if (adapter != nullptr && adapter->setWithServices != nullptr) {
        if (services == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                MessageServicesRequired);
        }
        return adapter->setWithServices(
            object,
            value,
            *services,
            adapter->context);
    }
    if (adapter != nullptr) {
        return adapter->set(object, value, adapter->context);
    }
    if (metaWritable) {
        return memberAccessor_.SetProperty(object, *metaProperty, value);
    }
    if (services == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageServicesRequired);
    }
    return provider->set(object, value, *services, provider->context);
}

Base::Result<XamlValue> XamlSchemaContext::ProvideMarkupExtensionValue(
    Core::TypeId type,
    Base::StringView arguments,
    const XamlServiceProvider& services) const noexcept {
    if (!frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            MessageSchemaNotFrozen);
    }
    const Core::TypeInfo* info = types_->FindType(type);
    const XamlMarkupExtensionRegistration* registration =
        FindMarkupExtension(type);
    if (info == nullptr ||
        !HasTypeFlag(info->Flags(), Core::TypeFlags::MarkupExtension) ||
        registration == nullptr || registration->provideValue == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            MessageMissingMarkupExtension);
    }
    return registration->provideValue(
        arguments,
        services,
        registration->context);
}

Base::Result<void> XamlSchemaContext::BeginInit(
    Core::TypeId type,
    Base::Object& object) const noexcept {
    const XamlTypeAdapterRegistration* adapter = FindTypeAdapter(type);
    if (adapter == nullptr || adapter->beginInit == nullptr) {
        return {};
    }
    return adapter->beginInit(object, adapter->context);
}

Base::Result<void> XamlSchemaContext::EndInit(
    Core::TypeId type,
    Base::Object& object) const noexcept {
    const XamlTypeAdapterRegistration* adapter = FindTypeAdapter(type);
    if (adapter == nullptr || adapter->endInit == nullptr) {
        return {};
    }
    return adapter->endInit(object, adapter->context);
}

void XamlSchemaContext::AbortInit(
    Core::TypeId type,
    Base::Object& object) const noexcept {
    const XamlTypeAdapterRegistration* adapter = FindTypeAdapter(type);
    if (adapter != nullptr && adapter->abortInit != nullptr) {
        adapter->abortInit(object, adapter->context);
    }
}

bool XamlSchemaContext::CreatesNameScope(Core::TypeId type) const noexcept {
    const XamlTypeAdapterRegistration* adapter =
        FindTypeAdapterExact(type);
    return adapter != nullptr && adapter->createsNameScope;
}

bool XamlSchemaContext::CreatesResourceScope(
    Core::TypeId type) const noexcept {
    const XamlTypeAdapterRegistration* adapter =
        FindTypeAdapterExact(type);
    return adapter != nullptr && adapter->createsResourceScope;
}

Base::Result<void> XamlSchemaContext::RegisterName(
    Core::TypeId scopeType,
    Base::Object& scopeOwner,
    Base::StringView name,
    Base::Object& object) const noexcept {
    const XamlTypeAdapterRegistration* adapter =
        FindTypeAdapter(scopeType);
    if (adapter == nullptr || adapter->registerName == nullptr) {
        return {};
    }
    return adapter->registerName(
        scopeOwner,
        name,
        object,
        adapter->context);
}

Base::Result<void> XamlSchemaContext::AddResource(
    Core::TypeId scopeType,
    Base::Object& scopeOwner,
    Base::StringView key,
    Core::TypeId valueType,
    const Base::Ref<Base::Object>& value) const noexcept {
    const XamlTypeAdapterRegistration* adapter =
        FindTypeAdapter(scopeType);
    if (adapter == nullptr || adapter->addResource == nullptr) {
        return {};
    }
    return adapter->addResource(
        scopeOwner,
        key,
        valueType,
        value,
        adapter->context);
}

const XamlMemberAdapterRegistration* XamlSchemaContext::FindMemberAdapter(
    Core::MemberId member) const noexcept {
    for (const XamlMemberAdapterRegistration& adapter : memberAdapters_) {
        if (adapter.member == member) {
            return &adapter;
        }
    }
    return nullptr;
}

XamlMemberWritePolicy XamlSchemaContext::ResolveMemberWritePolicy(
    const XamlResolvedMember& member) const noexcept {
    const XamlMemberAdapterRegistration* adapter = FindMemberAdapter(member.id);
    if (adapter != nullptr &&
        (adapter->set != nullptr || adapter->setWithServices != nullptr)) {
        return {adapter->mode, adapter->acceptsAnyValue, true};
    }
    const Core::PropertyInfo* property = types_->FindProperty(member.id);
    if (property != nullptr &&
        property->Access() != Core::PropertyAccessKind::External) {
        return {XamlMemberWriteMode::SetOnce, false, true};
    }
    const XamlMemberProviderRegistration* provider = FindMemberProvider(member);
    if (provider != nullptr) {
        return {provider->mode, provider->acceptsAnyValue, true};
    }
    return {};
}

const XamlTypeAdapterRegistration* XamlSchemaContext::FindTypeAdapter(
    Core::TypeId type) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId) {
        const XamlTypeAdapterRegistration* adapter =
            FindTypeAdapterExact(current);
        if (adapter != nullptr) {
            return adapter;
        }
        const Core::TypeInfo* info = types_->FindType(current);
        if (info == nullptr) {
            break;
        }
        current = info->BaseType();
    }
    return nullptr;
}

const XamlMarkupExtensionRegistration*
XamlSchemaContext::FindMarkupExtension(Core::TypeId type) const noexcept {
    for (const XamlMarkupExtensionRegistration& registration :
         markupExtensions_) {
        if (registration.type == type) {
            return &registration;
        }
    }
    return nullptr;
}

const XamlSchemaContext::ScalarRegistration*
XamlSchemaContext::FindScalarType(Core::TypeId type) const noexcept {
    for (const ScalarRegistration& registration : scalarTypes_) {
        if (registration.type == type) {
            return &registration;
        }
    }
    return nullptr;
}

const XamlTextConverterRegistration* XamlSchemaContext::FindTextConverter(
    Core::TypeId type) const noexcept {
    for (const XamlTextConverterRegistration& registration : textConverters_) {
        if (registration.type == type) {
            return &registration;
        }
    }
    return nullptr;
}

const XamlMemberProviderRegistration* XamlSchemaContext::FindMemberProvider(
    const XamlResolvedMember& member) const noexcept {
    if (!member.IsValid()) {
        return nullptr;
    }
    for (const XamlMemberProviderRegistration& provider : memberProviders_) {
        if (provider.handles != nullptr &&
            provider.handles(member, provider.context)) {
            return &provider;
        }
    }
    return nullptr;
}

const XamlTypeAdapterRegistration*
XamlSchemaContext::FindTypeAdapterExact(Core::TypeId type) const noexcept {
    for (const XamlTypeAdapterRegistration& adapter : typeAdapters_) {
        if (adapter.type == type) {
            return &adapter;
        }
    }
    return nullptr;
}

Base::Result<XamlResolvedMember>
XamlSchemaContext::ResolvePropertyOrEvent(
    Core::TypeId targetType,
    Core::TypeId ownerType,
    Base::StringView memberName,
    XamlMemberSyntax syntax,
    bool ownerWasExplicit) const noexcept {
    const Core::PropertyInfo* property = types_->FindProperty(
        ownerType,
        memberName,
        true);
    if (property != nullptr) {
        const bool attached = HasPropertyFlag(
            property->Flags(),
            Core::PropertyFlags::Attached);
        if (ownerWasExplicit && syntax == XamlMemberSyntax::Attribute &&
            !attached) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                MessageInvalidAttachedMember);
        }
        if (ownerWasExplicit &&
            syntax == XamlMemberSyntax::PropertyElement &&
            !attached &&
            !types_->IsDerivedFrom(targetType, property->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                MessageInvalidPropertyElement);
        }

        XamlResolvedMember resolved;
        resolved.id = property->Id();
        resolved.kind = Core::MemberKind::Property;
        resolved.ownerType = property->OwnerType();
        resolved.valueType = property->ValueType();
        resolved.propertyFlags = property->Flags();
        resolved.attached = attached;
        return resolved;
    }

    const Core::EventInfo* event = types_->FindEvent(
        ownerType,
        memberName,
        true);
    if (event != nullptr) {
        const bool attached = HasEventFlag(
            event->Flags(),
            Core::EventFlags::Attached);
        if (ownerWasExplicit && syntax == XamlMemberSyntax::Attribute &&
            !attached) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                MessageInvalidAttachedMember);
        }
        if (ownerWasExplicit &&
            syntax == XamlMemberSyntax::PropertyElement &&
            !attached &&
            !types_->IsDerivedFrom(targetType, event->OwnerType())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                MessageInvalidPropertyElement);
        }

        XamlResolvedMember resolved;
        resolved.id = event->Id();
        resolved.kind = Core::MemberKind::Event;
        resolved.ownerType = event->OwnerType();
        resolved.valueType = event->EventArgsType();
        resolved.eventFlags = event->Flags();
        resolved.attached = attached;
        return resolved;
    }

    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        MessageMemberNotFound);
}

} // namespace Aero::Markup
