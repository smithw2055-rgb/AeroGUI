#include <Aero/Core/Controls.hpp>
#include <Aero/Core/Presentation.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlDependencyProperty.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdio>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Markup;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

constexpr StringView CustomNamespace("urn:aero-custom");

struct CornerRadius final {
    double topLeft = 0.0;
    double topRight = 0.0;
    double bottomRight = 0.0;
    double bottomLeft = 0.0;
};

bool EqualCornerRadius(const void* left, const void* right, void*) noexcept {
    const auto& a = *static_cast<const CornerRadius*>(left);
    const auto& b = *static_cast<const CornerRadius*>(right);
    return a.topLeft == b.topLeft && a.topRight == b.topRight &&
        a.bottomRight == b.bottomRight && a.bottomLeft == b.bottomLeft;
}

bool ParseNumber(StringView text, std::uint32_t& offset, double& output) noexcept {
    while (offset < text.SizeBytes() && text[offset] == ' ') ++offset;
    if (offset >= text.SizeBytes()) return false;
    double value = 0.0;
    bool digits = false;
    while (offset < text.SizeBytes() && text[offset] >= '0' && text[offset] <= '9') {
        digits = true;
        value = value * 10.0 + static_cast<double>(text[offset] - '0');
        ++offset;
    }
    if (offset < text.SizeBytes() && text[offset] == '.') {
        ++offset;
        double scale = 0.1;
        while (offset < text.SizeBytes() && text[offset] >= '0' && text[offset] <= '9') {
            digits = true;
            value += static_cast<double>(text[offset] - '0') * scale;
            scale *= 0.1;
            ++offset;
        }
    }
    output = value;
    while (offset < text.SizeBytes() && text[offset] == ' ') ++offset;
    return digits;
}

Result<Value> ConvertCornerRadius(
    TypeId type, StringView text, void* context) noexcept {
    auto* types = static_cast<TypeRegistry*>(context);
    std::uint32_t offset = 0U;
    CornerRadius radius;
    if (!ParseNumber(text, offset, radius.topLeft)) {
        return Status::Failure(ErrorCode::InvalidArgument, "Invalid CornerRadius");
    }
    radius.topRight = radius.topLeft;
    radius.bottomRight = radius.topLeft;
    radius.bottomLeft = radius.topLeft;
    if (offset < text.SizeBytes()) {
        if (text[offset] != ',') {
            return Status::Failure(ErrorCode::InvalidArgument, "Invalid CornerRadius separator");
        }
        ++offset;
        if (!ParseNumber(text, offset, radius.topRight) || offset != text.SizeBytes()) {
            return Status::Failure(ErrorCode::InvalidArgument, "Invalid CornerRadius pair");
        }
        radius.bottomRight = radius.topLeft;
        radius.bottomLeft = radius.topRight;
    }
    return types->TryCreateValue(type, &radius);
}

class Badge final : public Border {
    AERO_DECLARE_METADATA(Badge, Border, "urn:aero-custom", "Badge")
public:
    Badge() noexcept : Border(StaticTypeId()) {}

    // Dependency properties
    AERO_DECLARE_DEPENDENCY_PROPERTY(CornerRadius);

    // Routed events
    AERO_DECLARE_ROUTED_EVENT(Activated, RoutedEventHandler);

    Result<CornerRadius> GetCornerRadius() const noexcept {
        Result<Value> value = GetValue(CornerRadiusProperty);
        if (!value) return value.GetStatus();
        return *static_cast<const CornerRadius*>(value.Value().AsCustom());
    }

    std::uint32_t Code() const noexcept { return code_; }
    void SetCode(std::uint32_t value) noexcept { code_ = value; }

private:
    std::uint32_t code_ = 0U;
};

Result<Value> GetBadgeCode(const Object& object, void*) noexcept {
    const auto& badge = static_cast<const Badge&>(object);
    return Value::FromUnsignedInteger(
        MakeTypeId("UInt32"), badge.Code());
}

Result<void> SetBadgeCode(Object& object, const Value& value, void*) noexcept {
    static_cast<Badge&>(object).SetCode(
        static_cast<std::uint32_t>(value.AsUnsignedInteger()));
    return {};
}

Result<Value> IncrementBadgeCode(Object& object, Span<const Value> arguments,
    void*) noexcept {
    auto& badge = static_cast<Badge&>(object);
    badge.SetCode(badge.Code() +
        static_cast<std::uint32_t>(arguments[0].AsUnsignedInteger()));
    return Value::FromUnsignedInteger(
        MakeTypeId("UInt32"), badge.Code());
}

AERO_IMPLEMENT_METADATA(Badge, TypeFlags::None) {
    MetaRegistrationContext& context = helper.Context();
    const CornerRadius zero{};
    Result<Value> defaultValue = context.types.TryCreateValue(
        MakeTypeId("urn:aero-custom", "CornerRadius"), &zero);
    if (!defaultValue) {
        helper.Fail(defaultValue.GetStatus());
        return;
    }
    AeroDP(CornerRadius,
        MakeTypeId("urn:aero-custom", "CornerRadius"),
        std::move(defaultValue).Value(),
        PropertyMetadataFlags::AffectsRender);

    PropertyRegistration codeProperty;
    codeProperty.name = StringView("Code");
    codeProperty.valueType = context.core.unsignedIntegerType;
    codeProperty.access = PropertyAccessKind::Ordinary;
    codeProperty.get = &GetBadgeCode;
    codeProperty.set = &SetBadgeCode;
    AeroProp(codeProperty);

    const MethodParameterRegistration incrementParameters[] = {
        {StringView("amount"), context.core.unsignedIntegerType}
    };
    MethodRegistration increment;
    increment.name = StringView("Increment");
    increment.returnType = context.core.unsignedIntegerType;
    increment.parameters = {incrementParameters, 1U};
    increment.invoke = &IncrementBadgeCode;
    AeroMethod(increment);
    AeroEvent(Activated, context.core.routedEventArgsType,
        RoutingStrategy::Bubble);
}

struct Fixture final {
    Dispatcher dispatcher;
    TypeRegistry types;
    DependencyPropertyRegistry properties{types};
    PresentationContextScope presentation{dispatcher, properties};
    RoutedEventRegistry routedEvents{types};
    XamlSchemaContext schema{types};
    XamlActivationProviderRegistry activation{schema};
    XamlDependencyPropertyBridge dependencyProperties{schema, properties};
    TypeId badgeType = InvalidTypeId;
    TypeId cornerRadiusType = InvalidTypeId;
    TypeId unsignedIntegerType = InvalidTypeId;

    static Result<Ref<Object>> Activate(TypeId type,
        const XamlActivationContext& context, void*) noexcept {
        if (context.dispatcher == nullptr || context.dependencyProperties == nullptr) {
            return Status::Failure(ErrorCode::InvalidArgument, "Activation services are missing");
        }
        if (type != Badge::StaticTypeId()) {
            return Status::Failure(ErrorCode::InvalidArgument,
                "Activation type is not Badge");
        }
        Result<Ref<Badge>> made = MakeRef<Badge>();
        if (!made) return made.GetStatus();
        return Ref<Object>(std::move(made).Value());
    }

    bool Build() {
        Result<CorePresentationMetadata> core =
            TryRegisterCorePresentationMetadata(types, properties, &routedEvents);
        CHECK(core);
        unsignedIntegerType = core.Value().unsignedIntegerType;
        badgeType = MakeTypeId(CustomNamespace, StringView("Badge"));
        cornerRadiusType = MakeTypeId(CustomNamespace, StringView("CornerRadius"));
        CHECK(types.TryRegisterType({CustomNamespace, StringView("CornerRadius"),
            InvalidTypeId, TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterValueSemantics(cornerRadiusType,
            {sizeof(CornerRadius), alignof(CornerRadius), nullptr, nullptr,
             &EqualCornerRadius, nullptr, true}));
        CHECK(types.TryRegisterTextConverter(
            {cornerRadiusType, &ConvertCornerRadius, &types}));
        MetaRegistrationContext registrationContext{
            types, properties, core.Value(), &routedEvents};
        CHECK(Badge::TryRegisterMetadata(registrationContext));

        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        CHECK(routedEvents.Freeze());
        CHECK(TryRegisterCorePresentationXaml(dependencyProperties));
        CHECK(activation.TryRegister({badgeType, &Activate, nullptr}));
        CHECK(schema.Freeze());
        CHECK(activation.Freeze());
        return true;
    }

    XamlActivationContext Activation() noexcept {
        XamlActivationContext context = XamlActivationContext::Create();
        context.dispatcher = &dispatcher;
        context.dependencyProperties = &properties;
        return context;
    }
};

bool TestCustomControlUsesGenericReflectionChannel() {
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<Badge xmlns=\"urn:aero-custom\" xmlns:aero=\"urn:aero\" "
        "CornerRadius=\"4,8\" Code=\"7\" Width=\"40\" Margin=\"2\" "
        "aero:Grid.Row=\"1\"/>"),
        &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlWithActivation(
        writer, reader, fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    Badge* badge = static_cast<Badge*>(loaded.Value().Get());
    CHECK(badge != nullptr && badge->HasWidth() && badge->Width() == 40.0);
    CHECK(badge->Code() == 7U);
    CHECK(fixture.types.IsInstanceOf(*badge, fixture.badgeType));
    CHECK(fixture.types.TryCast<Badge>(*badge) == badge);
    CHECK(badge->Margin().left == 2.0 && badge->Margin().top == 2.0 &&
        badge->Margin().right == 2.0 && badge->Margin().bottom == 2.0);
    Result<CornerRadius> radius = badge->GetCornerRadius();
    CHECK(radius && radius.Value().topLeft == 4.0 &&
        radius.Value().topRight == 8.0 && radius.Value().bottomRight == 4.0 &&
        radius.Value().bottomLeft == 8.0);
    Result<Value> row = badge->GetValue(Grid::RowProperty);
    CHECK(row && row.Value().AsUnsignedInteger() == 1U);

    const TypeId parameterTypes[] = {
        MakeTypeId(StringView("UInt32"))
    };
    const MethodInfo* increment = fixture.types.FindMethod(
        fixture.badgeType, StringView("Increment"), {parameterTypes, 1U});
    CHECK(increment != nullptr);
    const Value arguments[] = {
        Value::FromUnsignedInteger(parameterTypes[0], 5U)
    };
    Result<Value> invoked = fixture.schema.Members().InvokeMethod(
        *badge, *increment, {arguments, 1U});
    CHECK(invoked && invoked.Value().AsUnsignedInteger() == 12U &&
        badge->Code() == 12U);
    Result<Value> wrongCount = fixture.schema.Members().InvokeMethod(
        *badge, *increment, {});
    CHECK(!wrongCount && wrongCount.GetStatus().code == ErrorCode::InvalidArgument);
    const Value wrongArguments[] = {
        Value::FromDouble(MakeTypeId(StringView("Double")), 1.0)
    };
    Result<Value> wrongType = fixture.schema.Members().InvokeMethod(
        *badge, *increment, {wrongArguments, 1U});
    CHECK(!wrongType && wrongType.GetStatus().code == ErrorCode::InvalidArgument);
    return true;
}

} // namespace

int main() {
    if (!TestCustomControlUsesGenericReflectionChannel()) return 1;
    std::puts("Aero custom-control XAML tests passed");
    return 0;
}
