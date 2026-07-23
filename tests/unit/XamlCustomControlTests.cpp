#include <Aero/Core/BuiltinTypeIds.hpp>
#include <Aero/Core/ControlPrimitives.hpp>
#include <Aero/Core/Controls.hpp>
#include <Aero/Core/MetadataRuntime.hpp>
#include <Aero/Core/RuntimeMetadata.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlDependencyProperty.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdio>
#include <memory>
#include <utility>

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
constexpr StringView CustomModuleName("Aero.Tests.Badge");
constexpr StringView FailingModuleName("Aero.Tests.FailingMetadata");

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
            return Status::Failure(
                ErrorCode::InvalidArgument,
                "Invalid CornerRadius separator");
        }
        ++offset;
        if (!ParseNumber(text, offset, radius.topRight) ||
            offset != text.SizeBytes()) {
            return Status::Failure(
                ErrorCode::InvalidArgument,
                "Invalid CornerRadius pair");
        }
        radius.bottomRight = radius.topLeft;
        radius.bottomLeft = radius.topRight;
    }
    return types->TryCreateValue(type, &radius);
}

class Badge final : public Control {
    AERO_DECLARE_METADATA_NAMED(Badge, Control, "urn:aero-custom", "Badge")
public:
    Badge() noexcept : Control(StaticTypeId()) {}

    AERO_DECLARE_DEPENDENCY_PROPERTY(CornerRadius);
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
        BuiltinTypes::UnsignedInteger, badge.Code());
}

Result<void> SetBadgeCode(Object& object, const Value& value, void*) noexcept {
    static_cast<Badge&>(object).SetCode(
        static_cast<std::uint32_t>(value.AsUnsignedInteger()));
    return {};
}

Result<Value> IncrementBadgeCode(
    Object& object,
    Span<const Value> arguments,
    void*) noexcept {
    auto& badge = static_cast<Badge&>(object);
    badge.SetCode(badge.Code() +
        static_cast<std::uint32_t>(arguments[0].AsUnsignedInteger()));
    return Value::FromUnsignedInteger(
        BuiltinTypes::UnsignedInteger, badge.Code());
}

AERO_IMPLEMENT_METADATA(Badge, TypeFlags::None) {
    MetaRegistrationContext& context = helper.Context();
    const TypeId cornerRadiusType = MakeTypeId(
        CustomNamespace, StringView("CornerRadius"));
    const CornerRadius zero{};
    Result<Value> defaultValue = context.types.TryCreateValue(
        cornerRadiusType, &zero);
    if (!defaultValue) {
        helper.Fail(defaultValue.GetStatus());
        return;
    }
    AeroDP(CornerRadius,
        cornerRadiusType,
        std::move(defaultValue).Value(),
        PropertyMetadataFlags::AffectsRender);

    PropertyRegistration codeProperty;
    codeProperty.name = StringView("Code");
    codeProperty.valueType = BuiltinTypes::UnsignedInteger;
    codeProperty.access = PropertyAccessKind::Ordinary;
    codeProperty.get = &GetBadgeCode;
    codeProperty.set = &SetBadgeCode;
    AeroProp(codeProperty);

    const MethodParameterRegistration incrementParameters[] = {
        {StringView("amount"), BuiltinTypes::UnsignedInteger}
    };
    MethodRegistration increment;
    increment.name = StringView("Increment");
    increment.returnType = BuiltinTypes::UnsignedInteger;
    increment.parameters = {incrementParameters, 1U};
    increment.invoke = &IncrementBadgeCode;
    AeroMethod(increment);
    AeroEvent(Activated, BuiltinTypes::RoutedEventArgs,
        RoutingStrategy::Bubble);
}

Result<void> RegisterBadgeMetadata(
    MetaRegistrationContext& context,
    void*) noexcept {
    const TypeId cornerRadiusType = MakeTypeId(
        CustomNamespace, StringView("CornerRadius"));
    Result<TypeId> type = context.types.TryRegisterType({
        CustomNamespace,
        StringView("CornerRadius"),
        InvalidTypeId,
        TypeFlags::ValueType | TypeFlags::Sealed,
        nullptr});
    if (!type) return type.GetStatus();
    if (type.Value() != cornerRadiusType) {
        return Status::Failure(
            ErrorCode::IdCollision,
            "CornerRadius stable type id mismatch");
    }

    Result<void> status = context.types.TryRegisterValueSemantics(
        cornerRadiusType,
        {sizeof(CornerRadius), alignof(CornerRadius), nullptr, nullptr,
         &EqualCornerRadius, nullptr, true});
    if (!status) return status.GetStatus();
    status = context.types.TryRegisterTextConverter(
        {cornerRadiusType, &ConvertCornerRadius, &context.types});
    if (!status) return status.GetStatus();
    return Badge::TryRegisterMetadata(context);
}

Result<void> RegisterFailingMetadata(
    MetaRegistrationContext& context,
    void*) noexcept {
    Result<TypeId> transient = context.types.TryRegisterType({
        CustomNamespace,
        StringView("MustNotLeak"),
        InvalidTypeId,
        TypeFlags::None,
        nullptr});
    if (!transient) return transient.GetStatus();
    return Status::Failure(
        ErrorCode::ValidationFailed,
        "Intentional metadata transaction failure");
}

struct Fixture final {
    Dispatcher dispatcher;
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;
    std::unique_ptr<XamlSchemaContext> schema;
    std::unique_ptr<XamlActivationProviderRegistry> activation;
    std::unique_ptr<XamlDependencyPropertyBridge> dependencyProperties;
    TypeId badgeType = Badge::StaticTypeId();
    TypeId cornerRadiusType = MakeTypeId(
        CustomNamespace, StringView("CornerRadius"));

    static Result<Ref<Object>> Activate(
        TypeId type,
        const XamlActivationContext& context,
        void*) noexcept {
        if (context.dispatcher == nullptr ||
            context.dependencyProperties == nullptr) {
            return Status::Failure(
                ErrorCode::InvalidArgument,
                "Activation services are missing");
        }
        if (type != Badge::StaticTypeId()) {
            return Status::Failure(
                ErrorCode::InvalidArgument,
                "Activation type is not Badge");
        }
        Result<Ref<Badge>> made = MakeRef<Badge>();
        if (!made) return made.GetStatus();
        return Ref<Object>(std::move(made).Value());
    }

    bool Build() {
        CHECK(metadata.IsValid());
        CHECK(TryRegisterAeroPresentationMetadata(metadata));

        const std::uint32_t typeCountBeforeFailure =
            metadata.Types().TypeCount();
        const StringView failingName = FailingModuleName;
        Result<void> failed = metadata.TryRegisterModule({
            MakeMetadataModuleId(failingName),
            failingName,
            1U,
            &RegisterFailingMetadata,
            nullptr});
        CHECK(!failed &&
            failed.GetStatus().code == ErrorCode::ValidationFailed);
        CHECK(metadata.Types().TypeCount() == typeCountBeforeFailure);
        CHECK(metadata.Types().FindType(
            CustomNamespace, StringView("MustNotLeak")) == nullptr);

        const StringView customName = CustomModuleName;
        CHECK(metadata.TryRegisterModule({
            MakeMetadataModuleId(customName),
            customName,
            1U,
            &RegisterBadgeMetadata,
            nullptr}));
        CHECK(metadata.ModuleCount() == 2U);
        CHECK(metadata.Seal());
        CHECK(metadata.IsSealed());
        CHECK(metadata.Descriptors().IsSealed());
        CHECK(metadata.Facets().IsSealed());
        CHECK(metadata.Facets().ValueFacetsSealed());
        CHECK(metadata.ComputeSchemaHash());

        const MetadataDescriptorStore& descriptors = metadata.Descriptors();
        const MetadataFacetStore& facets = metadata.Facets();
        CHECK(descriptors.FindType(BuiltinTypes::Visual)->BaseType() ==
            BuiltinTypes::DependencyObject);
        CHECK(descriptors.FindType(BuiltinTypes::UIElement)->BaseType() ==
            BuiltinTypes::Visual);
        CHECK(descriptors.FindType(BuiltinTypes::FrameworkElement)->BaseType() ==
            BuiltinTypes::UIElement);
        CHECK(descriptors.FindType(BuiltinTypes::Panel)->BaseType() ==
            BuiltinTypes::FrameworkElement);
        CHECK(descriptors.FindType(BuiltinTypes::Decorator)->BaseType() ==
            BuiltinTypes::FrameworkElement);
        CHECK(descriptors.FindType(BuiltinTypes::Control)->BaseType() ==
            BuiltinTypes::FrameworkElement);
        CHECK(descriptors.FindType(BuiltinTypes::ContentControl)->BaseType() ==
            BuiltinTypes::Control);
        CHECK(descriptors.FindType(BuiltinTypes::UserControl)->BaseType() ==
            BuiltinTypes::ContentControl);
        CHECK(descriptors.FindType(BuiltinTypes::StackPanel)->BaseType() ==
            BuiltinTypes::Panel);
        CHECK(descriptors.FindType(BuiltinTypes::Border)->BaseType() ==
            BuiltinTypes::Decorator);
        CHECK(descriptors.FindType(BuiltinTypes::TextBlock)->BaseType() ==
            BuiltinTypes::FrameworkElement);
        CHECK(descriptors.FindType(badgeType)->BaseType() ==
            BuiltinTypes::Control);
        CHECK(descriptors.IsDerivedFrom(badgeType, BuiltinTypes::DependencyObject));
        CHECK(facets.FindContentMember(BuiltinTypes::StackPanel) !=
            InvalidMemberId);
        CHECK(facets.HasTypeFacet(
            cornerRadiusType, MetadataFacetKind::ValueSemantics));
        CHECK(facets.HasTypeFacet(
            cornerRadiusType, MetadataFacetKind::TextConverter));
        CHECK(facets.FindValueSemantics(cornerRadiusType) != nullptr);
        CHECK(facets.FindTextConverter(cornerRadiusType) != nullptr);

        const MetadataPropertyDescriptor* code = descriptors.FindProperty(
            badgeType, StringView("Code"), false);
        const MetadataPropertyDescriptor* corner = descriptors.FindProperty(
            badgeType, StringView("CornerRadius"), false);
        const MetadataEventDescriptor* activated = descriptors.FindEvent(
            badgeType, StringView("Activated"), false);
        const TypeId parameterTypes[] = {BuiltinTypes::UnsignedInteger};
        const MetadataMethodDescriptor* increment = descriptors.FindMethod(
            badgeType, StringView("Increment"), {parameterTypes, 1U}, false);
        CHECK(code != nullptr && corner != nullptr && activated != nullptr &&
            increment != nullptr);
        CHECK(facets.HasMemberFacet(
            code->Id(), MetadataFacetKind::PropertyAccessor));
        CHECK(facets.HasMemberFacet(
            corner->Id(), MetadataFacetKind::DependencyProperty));
        CHECK(facets.HasMemberFacet(
            activated->Id(), MetadataFacetKind::RoutedEvent));
        CHECK(facets.HasMemberFacet(
            increment->Id(), MetadataFacetKind::MethodInvoker));
        CHECK(facets.FindPropertyAccessor(code->Id())->access ==
            PropertyAccessKind::Ordinary);
        CHECK(facets.FindDependencyProperty(corner->Id())->property != nullptr);
        CHECK(facets.FindRoutedEvent(activated->Id())->registry ==
            &metadata.RoutedEvents());

        runtime = std::make_unique<MetadataRuntime>(metadata);
        CHECK(TryRegisterDependencyPropertyRuntimeProvider(
            *runtime,
            metadata.DependencyProperties(),
            BuiltinTypes::DependencyObject));
        CHECK(runtime->Freeze());

        const CornerRadius source{1.0, 2.0, 3.0, 4.0};
        Result<Value> created = runtime->TryCreateValue(cornerRadiusType, &source);
        CHECK(created && created.Value().Kind() == ValueKind::Custom);
        Result<Value> converted = runtime->TryConvertText(
            cornerRadiusType, StringView("3,6"));
        CHECK(converted && converted.Value().Kind() == ValueKind::Custom);
        const auto& convertedRadius = *static_cast<const CornerRadius*>(
            converted.Value().AsCustom());
        CHECK(convertedRadius.topLeft == 3.0 &&
            convertedRadius.topRight == 6.0);

        schema = std::make_unique<XamlSchemaContext>(metadata, *runtime);
        CHECK(schema->UsesRuntime());
        CHECK(schema->Members().UsesRuntime());
        activation = std::make_unique<XamlActivationProviderRegistry>(*schema);
        dependencyProperties = std::make_unique<XamlDependencyPropertyBridge>(
            *schema, metadata.DependencyProperties());
        CHECK(TryRegisterAeroPresentationXaml(*dependencyProperties));
        CHECK(activation->TryRegister({badgeType, &Activate, nullptr}));
        CHECK(schema->Freeze());
        CHECK(activation->Freeze());
        Result<XamlValue> schemaConverted = schema->ConvertTextRuntime(
            cornerRadiusType, StringView("2,5"));
        CHECK(schemaConverted &&
            schemaConverted.Value().Kind() == XamlValueKind::Custom);
        return true;
    }

    XamlActivationContext Activation() noexcept {
        XamlActivationContext context = XamlActivationContext::Create();
        context.dispatcher = &dispatcher;
        context.dependencyProperties = &metadata.DependencyProperties();
        return context;
    }
};

bool TestCustomControlUsesUnifiedMetadataAndActivation() {
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
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlWithActivation(
        writer, reader, *fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    Badge* badge = static_cast<Badge*>(loaded.Value().Get());
    CHECK(badge != nullptr && badge->HasWidth() && badge->Width() == 40.0);
    CHECK(badge->Code() == 7U);
    CHECK(fixture.metadata.Descriptors().IsDerivedFrom(
        badge->RuntimeType(), fixture.badgeType));
    CHECK(badge->Margin().left == 2.0 && badge->Margin().top == 2.0 &&
        badge->Margin().right == 2.0 && badge->Margin().bottom == 2.0);
    Result<CornerRadius> radius = badge->GetCornerRadius();
    CHECK(radius && radius.Value().topLeft == 4.0 &&
        radius.Value().topRight == 8.0 && radius.Value().bottomRight == 4.0 &&
        radius.Value().bottomLeft == 8.0);
    Result<Value> row = badge->GetValue(Grid::RowProperty);
    CHECK(row && row.Value().AsUnsignedInteger() == 1U);

    const MetadataPropertyDescriptor* code =
        fixture.metadata.Descriptors().FindProperty(
            fixture.badgeType, StringView("Code"), false);
    const MetadataPropertyDescriptor* corner =
        fixture.metadata.Descriptors().FindProperty(
            fixture.badgeType, StringView("CornerRadius"), false);
    const TypeId parameterTypes[] = {BuiltinTypes::UnsignedInteger};
    const MetadataMethodDescriptor* increment =
        fixture.metadata.Descriptors().FindMethod(
            fixture.badgeType,
            StringView("Increment"),
            {parameterTypes, 1U},
            false);
    CHECK(code != nullptr && corner != nullptr && increment != nullptr);

    Result<Value> reflectedCode = fixture.runtime->GetProperty(*badge, code->Id());
    CHECK(reflectedCode && reflectedCode.Value().AsUnsignedInteger() == 7U);
    CHECK(fixture.runtime->SetProperty(
        *badge,
        code->Id(),
        Value::FromUnsignedInteger(BuiltinTypes::UnsignedInteger, 9U)));
    CHECK(badge->Code() == 9U);
    Result<Value> reflectedCorner = fixture.runtime->GetProperty(
        *badge, corner->Id());
    CHECK(reflectedCorner && reflectedCorner.Value().Kind() == ValueKind::Custom);

    const Value arguments[] = {
        Value::FromUnsignedInteger(parameterTypes[0], 5U)
    };
    Result<Value> invoked = fixture.runtime->InvokeMethod(
        *badge, increment->Id(), {arguments, 1U});
    CHECK(invoked && invoked.Value().AsUnsignedInteger() == 14U &&
        badge->Code() == 14U);
    Result<Value> wrongCount = fixture.runtime->InvokeMethod(
        *badge, increment->Id(), {});
    CHECK(!wrongCount &&
        wrongCount.GetStatus().code == ErrorCode::InvalidArgument);
    const Value wrongArguments[] = {
        Value::FromDouble(BuiltinTypes::Double, 1.0)
    };
    Result<Value> wrongType = fixture.runtime->InvokeMethod(
        *badge, increment->Id(), {wrongArguments, 1U});
    CHECK(!wrongType &&
        wrongType.GetStatus().code == ErrorCode::InvalidArgument);
    return true;
}

} // namespace

int main() {
    if (!TestCustomControlUsesUnifiedMetadataAndActivation()) return 1;
    std::puts("Aero custom-control metadata tests passed");
    return 0;
}
