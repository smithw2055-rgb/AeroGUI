#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Controls/ControlPrimitives.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/Metadata/MetadataDsl.hpp>
#include <Aero/Core/Metadata/MetadataRegistrationValues.hpp>
#include <Aero/Controls/RuntimeMetadata.hpp>
#include <Aero/Markup/Runtime/XamlActivation.hpp>
#include <Aero/Markup/Parsing/XamlNodeReader.hpp>
#include <Aero/Markup/Runtime/XamlObjectWriter.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>
#include <Aero/Markup/Parsing/XmlTokenizer.hpp>

#include <cstdio>
#include <memory>
#include <utility>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;
using namespace Aero::Controls;
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
    AERO_TYPED_META_NAMED(
        CornerRadius, NoMetadataBase, "urn:aero-custom", "CornerRadius")
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
    auto* registrations =
        static_cast<MetadataValueRegistrationStore*>(context);
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
    return MetadataRegistrationValues(*registrations).TryCreateValue(
        type, &radius);
}

class Badge final : public Control {
    AERO_TYPED_META_NAMED(Badge, Control, "urn:aero-custom", "Badge")
public:
    Badge() noexcept : Control(StaticTypeId()) {}

    inline static constexpr Aero::Core::DependencyPropertyHandle
        CornerRadiusProperty = Aero::Core::MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "CornerRadius");
    inline static constexpr RoutedEventHandle ActivatedEvent =
        MakeRoutedEventHandle(StaticTypeIdValue_, "Activated");
    RoutedEvent_<RoutedEventHandler> Activated() noexcept {
        return {*this, ActivatedEvent};
    }

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

Result<void> RegisterBadgeMetadata(
    MetaRegistrationContext& context,
    void*) noexcept {
    MetaTypeBuilder<CornerRadius> cornerRadius =
        MetaTypeBuilder<CornerRadius>::Struct(context);
    cornerRadius
        .Field<&CornerRadius::topLeft>("TopLeft")
        .Field<&CornerRadius::topRight>("TopRight")
        .Field<&CornerRadius::bottomRight>("BottomRight")
        .Field<&CornerRadius::bottomLeft>("BottomLeft")
        .ValueSemantics({sizeof(CornerRadius), alignof(CornerRadius),
            nullptr, nullptr, &EqualCornerRadius, nullptr, true})
        .TextConverter(&ConvertCornerRadius, &context.ValueRegistrations());
    Result<void> status = cornerRadius.Finish();
    if (!status) return status.GetStatus();

    const CornerRadius zero{};
    Result<Value> defaultValue = context.Values().TryCreateValue(
        TypeOf<CornerRadius>(), &zero);
    if (!defaultValue) return defaultValue.GetStatus();

    MetaTypeBuilder<Badge> badge = MetaTypeBuilder<Badge>::Object(context);
    badge.DefaultFactory().DependencyProperty(Badge::CornerRadiusProperty, "CornerRadius",
        TypeOf<CornerRadius>(), std::move(defaultValue).Value(),
        PropertyMetadataFlags::AffectsRender);

    PropertyRegistration codeProperty;
    codeProperty.name = "Code";
    codeProperty.valueType = BuiltinTypes::UnsignedInteger;
    codeProperty.access = PropertyAccessKind::Ordinary;
    codeProperty.get = &GetBadgeCode;
    codeProperty.set = &SetBadgeCode;
    badge.Property(codeProperty);

    const MethodParameterRegistration incrementParameters[] = {
        {"amount", BuiltinTypes::UnsignedInteger}
    };
    MethodRegistration increment;
    increment.name = "Increment";
    increment.returnType = BuiltinTypes::UnsignedInteger;
    increment.parameters = {incrementParameters, 1U};
    increment.invoke = &IncrementBadgeCode;
    badge.Method(increment)
        .RoutedEvent(Badge::ActivatedEvent, "Activated",
            BuiltinTypes::RoutedEventArgs, RoutingStrategy::Bubble);
    return badge.Finish();
}

Result<void> RegisterFailingMetadata(
    MetaRegistrationContext& context,
    void*) noexcept {
    Result<TypeId> transient = context.Types().TryRegisterType(TypeRegistration::Object(CustomNamespace, StringView("MustNotLeak"), InvalidTypeId, TypeFlags::None, nullptr));
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
    std::unique_ptr<ActivationProviderRegistry> activation;
    TypeId badgeType = Badge::StaticTypeId();
    TypeId cornerRadiusType = MakeTypeId(
        CustomNamespace, StringView("CornerRadius"));

    bool Build() {
        CHECK(metadata.IsValid());
        CHECK(Aero::Controls::TryRegisterBuiltInUiMetadata(metadata));

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
        CHECK(metadata.ModuleCount() == 4U);
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
        const ContentFacet* panelContent =
            facets.FindContent(BuiltinTypes::Panel);
        CHECK(panelContent != nullptr &&
            panelContent->kind == ContentKind::Collection &&
            HasContentFlag(panelContent->flags, ContentFlags::Visual) &&
            panelContent->write != nullptr && panelContent->clear != nullptr);
        CHECK(facets.FindContentMember(BuiltinTypes::StackPanel) ==
            panelContent->member);
        CHECK(facets.FindContentByMember(panelContent->member) ==
            panelContent);
        CHECK(facets.FindTypeFactory(BuiltinTypes::StackPanel) != nullptr);
        CHECK(facets.FindTypeFactory(badgeType) != nullptr);
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
        CHECK(facets.FindRoutedEvent(activated->Id())->strategy ==
            RoutingStrategy::Bubble);

        runtime = std::make_unique<MetadataRuntime>(metadata);
        schema = std::make_unique<XamlSchemaContext>(metadata, *runtime);
        CHECK(schema->UsesRuntime());
        activation = std::make_unique<ActivationProviderRegistry>(
            metadata.Descriptors());
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

        CHECK(schema->Freeze());
        CHECK(activation->Freeze());
        Result<XamlValue> schemaConverted = schema->ConvertText(
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
    Result<XamlLoadResult> loaded = LoadXamlWithActivation(
        writer, reader, *fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    Badge* badge =
        static_cast<Badge*>(loaded.Value().root.Get());
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
