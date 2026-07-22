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

Result<Value> ConvertCornerRadius(TypeId type, StringView text,
    IAllocator&, void* context) noexcept {
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
public:
    Badge(Dispatcher& dispatcher, DependencyPropertyRegistry& registry,
        TypeId runtimeType, IAllocator* allocator) noexcept
        : Border(dispatcher, registry, runtimeType, allocator) {}

    static DependencyPropertyHandle CornerRadiusProperty() noexcept {
        const TypeId badge = MakeTypeId(CustomNamespace, StringView("Badge"));
        return {MakeMemberId(badge, MemberKind::Property, StringView("CornerRadius"))};
    }

    Result<CornerRadius> GetCornerRadius() const noexcept {
        Result<Value> value = GetValue(CornerRadiusProperty());
        if (!value) return value.GetStatus();
        return *static_cast<const CornerRadius*>(value.Value().AsCustom());
    }
};

struct Fixture final {
    Dispatcher dispatcher;
    TypeRegistry types;
    DependencyPropertyRegistry properties{types};
    XamlSchemaContext schema{types};
    XamlActivationProviderRegistry activation{schema};
    XamlDependencyPropertyBridge dependencyProperties{schema, properties};
    TypeId badgeType = InvalidTypeId;
    TypeId cornerRadiusType = InvalidTypeId;

    static Result<Ref<Object>> Activate(TypeId type,
        const XamlActivationContext& context, IAllocator& allocator, void*) noexcept {
        if (context.dispatcher == nullptr || context.dependencyProperties == nullptr) {
            return Status::Failure(ErrorCode::InvalidArgument, "Activation services are missing");
        }
        Result<Ref<Badge>> made = MakeRefWithAllocator<Badge>(allocator,
            *context.dispatcher, *context.dependencyProperties, type, &allocator);
        if (!made) return made.GetStatus();
        return Ref<Object>(std::move(made).Value());
    }

    bool Build() {
        Result<CorePresentationMetadata> core =
            TryRegisterCorePresentationMetadata(types, properties);
        CHECK(core);
        badgeType = MakeTypeId(CustomNamespace, StringView("Badge"));
        cornerRadiusType = MakeTypeId(CustomNamespace, StringView("CornerRadius"));
        CHECK(types.TryRegisterType({CustomNamespace, StringView("Badge"),
            core.Value().borderType, TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({CustomNamespace, StringView("CornerRadius"),
            InvalidTypeId, TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterValueSemantics(cornerRadiusType,
            {sizeof(CornerRadius), alignof(CornerRadius), nullptr, nullptr,
             &EqualCornerRadius, nullptr, true}));
        CHECK(types.TryRegisterTextConverter(
            {cornerRadiusType, &ConvertCornerRadius, &types}));
        const CornerRadius zero{};
        Result<Value> defaultValue = types.TryCreateValue(cornerRadiusType, &zero);
        CHECK(defaultValue);
        DependencyPropertyRegistration registration;
        registration.name = StringView("CornerRadius");
        registration.ownerType = badgeType;
        registration.valueType = cornerRadiusType;
        registration.metadata.defaultValue = defaultValue.Value();
        registration.metadata.flags = PropertyMetadataFlags::AffectsRender;
        CHECK(properties.TryRegister(registration));

        CHECK(types.Freeze());
        CHECK(properties.Freeze());
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
        "CornerRadius=\"4,8\" Width=\"40\" Margin=\"2\" aero:Grid.Row=\"1\"/>"),
        &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlWithActivation(
        writer, reader, fixture.activation, fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    Badge* badge = static_cast<Badge*>(loaded.Value().Get());
    CHECK(badge != nullptr && badge->HasWidth() && badge->Width() == 40.0);
    CHECK(badge->Margin().left == 2.0 && badge->Margin().top == 2.0 &&
        badge->Margin().right == 2.0 && badge->Margin().bottom == 2.0);
    Result<CornerRadius> radius = badge->GetCornerRadius();
    CHECK(radius && radius.Value().topLeft == 4.0 &&
        radius.Value().topRight == 8.0 && radius.Value().bottomRight == 4.0 &&
        radius.Value().bottomLeft == 8.0);
    Result<Value> row = badge->GetValue(Grid::RowProperty());
    CHECK(row && row.Value().AsUnsignedInteger() == 1U);
    return true;
}

} // namespace

int main() {
    if (!TestCustomControlUsesGenericReflectionChannel()) return 1;
    std::puts("Aero custom-control XAML tests passed");
    return 0;
}
