#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include "TestMetadataConverters.hpp"

#include <cstdint>
#include <memory>
#include <cstdio>
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

class TestElement final : public Object {
public:
    TestElement(TypeId type, bool leaf) noexcept
        : type_(type), leaf_(leaf) {
        ++liveCount_;
    }

    ~TestElement() override {
        --liveCount_;
    }

    TypeId RuntimeType() const noexcept override { return type_; }
    StringView Title() const noexcept { return title_.View(); }
    bool Enabled() const noexcept { return enabled_; }
    std::int64_t Count() const noexcept { return count_; }
    double Ratio() const noexcept { return ratio_; }
    std::int64_t Row() const noexcept { return row_; }
    bool IsLeaf() const noexcept { return leaf_; }
    bool BeginCalled() const noexcept { return beginCalled_; }
    bool EndCalled() const noexcept { return endCalled_; }
    bool Aborted() const noexcept { return aborted_; }
    const Ref<Object>& Child() const noexcept { return child_; }
    Span<const Ref<Object>> Children() const noexcept {
        return {children_.Data(), children_.Size()};
    }

    static void ResetCounters() noexcept {
        liveCount_ = 0U;
        beginCount_ = 0U;
        endCount_ = 0U;
        abortCount_ = 0U;
    }

    static std::uint32_t LiveCount() noexcept {
        return liveCount_;
    }
    static std::uint32_t BeginCount() noexcept {
        return beginCount_;
    }
    static std::uint32_t EndCount() noexcept {
        return endCount_;
    }
    static std::uint32_t AbortCount() noexcept {
        return abortCount_;
    }

private:
    friend Result<void> SetTitle(Object&, const XamlValue&, void*) noexcept;
    friend Result<void> SetEnabled(Object&, const XamlValue&, void*) noexcept;
    friend Result<void> SetCount(Object&, const XamlValue&, void*) noexcept;
    friend Result<void> SetRatio(Object&, const XamlValue&, void*) noexcept;
    friend Result<void> SetRow(Object&, const XamlValue&, void*) noexcept;
    friend Result<void> SetChild(Object&, const XamlValue&, void*) noexcept;
    friend Result<void> AddChild(Object&, const XamlValue&, void*) noexcept;
    friend Result<void> BeginElement(Object&, void*) noexcept;
    friend Result<void> EndElement(Object&, void*) noexcept;
    friend void AbortElement(Object&, void*) noexcept;

    TypeId type_ = InvalidTypeId;
    String title_;
    Ref<Object> child_;
    Vector<Ref<Object>> children_;
    bool enabled_ = false;
    std::int64_t count_ = 0;
    double ratio_ = 0.0;
    std::int64_t row_ = 0;
    bool leaf_ = false;
    bool beginCalled_ = false;
    bool endCalled_ = false;
    bool aborted_ = false;

    static std::uint32_t liveCount_;
    static std::uint32_t beginCount_;
    static std::uint32_t endCount_;
    static std::uint32_t abortCount_;
};

std::uint32_t TestElement::liveCount_ = 0U;
std::uint32_t TestElement::beginCount_ = 0U;
std::uint32_t TestElement::endCount_ = 0U;
std::uint32_t TestElement::abortCount_ = 0U;

Result<Ref<Object>> MakeElement() noexcept {
    Result<Ref<TestElement>> created = MakeRef<TestElement>(
        MakeTypeId(StringView("urn:test"), StringView("Element")), false);
    if (!created) {
        return created.GetStatus();
    }
    Ref<TestElement> typed = std::move(created).Value();
    Ref<Object> result(std::move(typed));
    return result;
}

Result<Ref<Object>> MakeLeaf() noexcept {
    Result<Ref<TestElement>> created = MakeRef<TestElement>(
        MakeTypeId(StringView("urn:test"), StringView("Leaf")), true);
    if (!created) {
        return created.GetStatus();
    }
    Ref<TestElement> typed = std::move(created).Value();
    Ref<Object> result(std::move(typed));
    return result;
}

Result<void> SetTitle(
    Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::String) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Title expects a XAML string value");
    }
    return static_cast<TestElement&>(object).title_.TryAssign(value.AsString());
}

Result<void> SetEnabled(
    Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::Boolean) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Enabled expects a XAML Boolean value");
    }
    static_cast<TestElement&>(object).enabled_ = value.AsBoolean();
    return {};
}

Result<void> SetCount(
    Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::SignedInteger) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Count expects a XAML signed integer value");
    }
    static_cast<TestElement&>(object).count_ = value.AsSignedInteger();
    return {};
}

Result<void> SetRatio(
    Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::Double) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Ratio expects a XAML Double value");
    }
    static_cast<TestElement&>(object).ratio_ = value.AsDouble();
    return {};
}

Result<void> SetRow(
    Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::SignedInteger) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Grid.Row expects a XAML signed integer value");
    }
    static_cast<TestElement&>(object).row_ = value.AsSignedInteger();
    return {};
}

Result<void> SetChild(
    Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::Object) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Child expects a XAML object value");
    }
    static_cast<TestElement&>(object).child_ = value.AsObject();
    return {};
}

Result<void> AddChild(
    Object& object,
    const XamlValue& value,
    void*) noexcept {
    if (value.Kind() != XamlValueKind::Object) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Children expects a XAML object value");
    }
    return static_cast<TestElement&>(object).children_.TryPushBack(
        value.AsObject());
}

Result<void> BeginElement(Object& object, void*) noexcept {
    TestElement& element = static_cast<TestElement&>(object);
    if (element.beginCalled_) {
        return Status::Failure(
            ErrorCode::InvalidState,
            "BeginInit was called twice");
    }
    element.beginCalled_ = true;
    ++TestElement::beginCount_;
    return {};
}

Result<void> EndElement(Object& object, void*) noexcept {
    TestElement& element = static_cast<TestElement&>(object);
    if (!element.beginCalled_ || element.endCalled_) {
        return Status::Failure(
            ErrorCode::InvalidState,
            "EndInit was called in an invalid state");
    }
    element.endCalled_ = true;
    ++TestElement::endCount_;
    return {};
}

void AbortElement(Object& object, void*) noexcept {
    TestElement& element = static_cast<TestElement&>(object);
    if (!element.aborted_) {
        element.aborted_ = true;
        ++TestElement::abortCount_;
    }
}

struct Fixture final {
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;
    std::unique_ptr<XamlSchemaContext> schema;

    TypeId objectType = InvalidTypeId;
    TypeId stringType = InvalidTypeId;
    TypeId booleanType = InvalidTypeId;
    TypeId integerType = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId elementType = InvalidTypeId;
    TypeId leafType = InvalidTypeId;
    TypeId gridType = InvalidTypeId;

    MemberId title = InvalidMemberId;
    MemberId enabled = InvalidMemberId;
    MemberId count = InvalidMemberId;
    MemberId ratio = InvalidMemberId;
    MemberId child = InvalidMemberId;
    MemberId children = InvalidMemberId;
    MemberId row = InvalidMemberId;

    static Result<void> RegisterModule(
        MetaRegistrationContext& context,
        void* userContext) noexcept {
        return static_cast<Fixture*>(userContext)->RegisterMetadata(context);
    }

    Result<void> RegisterMetadata(MetaRegistrationContext& context) noexcept {
        MetadataRegistrationTypes types = context.Types();
        const StringView ns("urn:test");
        Result<TypeId> type = types.TryRegisterType(TypeRegistration::Object(ns, StringView("Object"), InvalidTypeId, TypeFlags::None, nullptr));
        if (!type) return type.GetStatus();
        type = types.TryRegisterType(TypeRegistration::Primitive(ns, StringView("String"), TypeFlags::ValueType | TypeFlags::Sealed));
        if (!type) return type.GetStatus();
        type = types.TryRegisterType(TypeRegistration::Primitive(ns, StringView("Boolean"), TypeFlags::ValueType | TypeFlags::Sealed));
        if (!type) return type.GetStatus();
        type = types.TryRegisterType(TypeRegistration::Primitive(ns, StringView("Int64"), TypeFlags::ValueType | TypeFlags::Sealed));
        if (!type) return type.GetStatus();
        type = types.TryRegisterType(TypeRegistration::Primitive(ns, StringView("Double"), TypeFlags::ValueType | TypeFlags::Sealed));
        if (!type) return type.GetStatus();
        const struct {
            TypeId type;
            TextValueConverterCallback converter;
        } converters[] = {
            {stringType, &Aero::Tests::ConvertTestString},
            {booleanType, &Aero::Tests::ConvertTestBoolean},
            {integerType, &Aero::Tests::ConvertTestSignedInteger},
            {doubleType, &Aero::Tests::ConvertTestDouble}
        };
        for (const auto& converter : converters) {
            Result<void> registered = Aero::Tests::RegisterTestTextConverter(
                context, converter.type, converter.converter);
            if (!registered) return registered.GetStatus();
        }
        type = types.TryRegisterType(TypeRegistration::Object(ns, StringView("Element"), objectType, TypeFlags::None, &MakeElement));
        if (!type) return type.GetStatus();
        type = types.TryRegisterType(TypeRegistration::Object(ns, StringView("Leaf"), elementType, TypeFlags::Sealed, &MakeLeaf));
        if (!type) return type.GetStatus();
        type = types.TryRegisterType(TypeRegistration::Object(ns, StringView("Grid"), objectType, TypeFlags::None, nullptr));
        if (!type) return type.GetStatus();

        Result<MemberId> member = types.TryRegisterProperty(
            elementType,
            {StringView("Title"), stringType, PropertyFlags::None});
        if (!member) return member.GetStatus();
        title = member.Value();
        member = types.TryRegisterProperty(
            elementType,
            {StringView("Enabled"), booleanType, PropertyFlags::None});
        if (!member) return member.GetStatus();
        enabled = member.Value();
        member = types.TryRegisterProperty(
            elementType,
            {StringView("Count"), integerType, PropertyFlags::None});
        if (!member) return member.GetStatus();
        count = member.Value();
        member = types.TryRegisterProperty(
            elementType,
            {StringView("Ratio"), doubleType, PropertyFlags::None});
        if (!member) return member.GetStatus();
        ratio = member.Value();
        member = types.TryRegisterProperty(
            elementType,
            {StringView("Child"), elementType, PropertyFlags::None});
        if (!member) return member.GetStatus();
        child = member.Value();
        member = types.TryRegisterProperty(
            elementType,
            {StringView("Children"), elementType,
             PropertyFlags::Structural | PropertyFlags::Collection});
        if (!member) return member.GetStatus();
        children = member.Value();
        Result<void> status = types.TrySetContentMember(elementType, children);
        if (!status) return status.GetStatus();
        member = types.TryRegisterProperty(
            gridType,
            {StringView("Row"), integerType, PropertyFlags::Attached});
        if (!member) return member.GetStatus();
        row = member.Value();
        return {};
    }

    bool Build() {
        const StringView ns("urn:test");
        objectType = MakeTypeId(ns, StringView("Object"));
        stringType = MakeTypeId(ns, StringView("String"));
        booleanType = MakeTypeId(ns, StringView("Boolean"));
        integerType = MakeTypeId(ns, StringView("Int64"));
        doubleType = MakeTypeId(ns, StringView("Double"));
        elementType = MakeTypeId(ns, StringView("Element"));
        leafType = MakeTypeId(ns, StringView("Leaf"));
        gridType = MakeTypeId(ns, StringView("Grid"));

        const StringView moduleName("Tests.XamlObjectWriter");
        CHECK(metadata.TryRegisterModule({
            MakeMetadataModuleId(moduleName), moduleName, 1U,
            &Fixture::RegisterModule, this}));
        CHECK(metadata.Seal());
        runtime = std::make_unique<MetadataRuntime>(metadata);
        CHECK(runtime->Freeze());
        schema = std::make_unique<XamlSchemaContext>(metadata, *runtime);

        CHECK(schema->TryRegisterMemberAdapter({
            title, XamlMemberWriteMode::SetOnce, &SetTitle, nullptr}));
        CHECK(schema->TryRegisterMemberAdapter({
            enabled, XamlMemberWriteMode::SetOnce, &SetEnabled, nullptr}));
        CHECK(schema->TryRegisterMemberAdapter({
            count, XamlMemberWriteMode::SetOnce, &SetCount, nullptr}));
        CHECK(schema->TryRegisterMemberAdapter({
            ratio, XamlMemberWriteMode::SetOnce, &SetRatio, nullptr}));
        CHECK(schema->TryRegisterMemberAdapter({
            child, XamlMemberWriteMode::SetOnce, &SetChild, nullptr}));
        CHECK(schema->TryRegisterMemberAdapter({
            children, XamlMemberWriteMode::Collection, &AddChild, nullptr}));
        CHECK(schema->TryRegisterMemberAdapter({
            row, XamlMemberWriteMode::SetOnce, &SetRow, nullptr}));
        CHECK(schema->TryRegisterTypeAdapter({
            elementType,
            &BeginElement,
            &EndElement,
            &AbortElement,
            nullptr}));
        CHECK(schema->Freeze());
        return true;
    }
};
Result<Ref<Object>> LoadDocument(
    Fixture& fixture,
    StringView xaml,
    DiagnosticBag& diagnostics) noexcept {
    Utf8XmlTokenizer tokenizer;
    Result<void> reset = tokenizer.Reset(xaml, &diagnostics);
    if (!reset) {
        return reset.GetStatus();
    }

    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    return writer.Load(reader);
}

bool TestAttributesAttachedPropertyAndContentCollection() {
    TestElement::ResetCounters();
    {
        Fixture fixture;
        CHECK(fixture.Build());
        DiagnosticBag diagnostics;
        Result<Ref<Object>> loaded = LoadDocument(
            fixture,
            StringView(
                "<Element xmlns=\"urn:test\" Title=\"Root\" Enabled=\"TRUE\" "
                "Count=\"-42\" Ratio=\"1.25\" Grid.Row=\"3\">"
                "<Leaf Title=\"First\"/><Leaf Title=\"Second\"/>"
                "</Element>"),
            diagnostics);
        CHECK(loaded);
        CHECK(diagnostics.Size() == 0U);

        Ref<Object> rootObject = std::move(loaded).Value();
        TestElement* root = static_cast<TestElement*>(rootObject.Get());
        CHECK(root != nullptr);
        CHECK(root->Title() == StringView("Root"));
        CHECK(root->Enabled());
        CHECK(root->Count() == -42);
        CHECK(root->Ratio() == 1.25);
        CHECK(root->Row() == 3);
        CHECK(root->BeginCalled());
        CHECK(root->EndCalled());
        CHECK(root->Children().Size() == 2U);

        TestElement* first = static_cast<TestElement*>(root->Children()[0].Get());
        TestElement* second = static_cast<TestElement*>(root->Children()[1].Get());
        CHECK(first->IsLeaf() && second->IsLeaf());
        CHECK(first->Title() == StringView("First"));
        CHECK(second->Title() == StringView("Second"));
        CHECK(TestElement::LiveCount() == 3U);
        CHECK(TestElement::BeginCount() == 3U);
        CHECK(TestElement::EndCount() == 3U);
        CHECK(TestElement::AbortCount() == 0U);
    }
    CHECK(TestElement::LiveCount() == 0U);
    return true;
}

bool TestPropertyElementAssignment() {
    TestElement::ResetCounters();
    {
        Fixture fixture;
        CHECK(fixture.Build());
        DiagnosticBag diagnostics;
        Result<Ref<Object>> loaded = LoadDocument(
            fixture,
            StringView(
                "<Element xmlns=\"urn:test\">"
                "<Element.Child><Leaf Title=\"Nested\"/></Element.Child>"
                "</Element>"),
            diagnostics);
        CHECK(loaded);
        CHECK(diagnostics.Size() == 0U);

        Ref<Object> rootObject = std::move(loaded).Value();
        TestElement* root = static_cast<TestElement*>(rootObject.Get());
        CHECK(root->Child());
        TestElement* child = static_cast<TestElement*>(root->Child().Get());
        CHECK(child->IsLeaf());
        CHECK(child->Title() == StringView("Nested"));
        CHECK(TestElement::BeginCount() == 2U);
        CHECK(TestElement::EndCount() == 2U);
        CHECK(TestElement::AbortCount() == 0U);
    }
    CHECK(TestElement::LiveCount() == 0U);
    return true;
}

bool TestRollbackAfterCompletedChild() {
    TestElement::ResetCounters();
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = LoadDocument(
        fixture,
        StringView(
            "<Element xmlns=\"urn:test\">\n"
            "  <Leaf/>\n"
            "  <Element.Unknown>1</Element.Unknown>\n"
            "</Element>"),
        diagnostics);
    CHECK(!loaded);
    CHECK(loaded.GetStatus().code == ErrorCode::NotFound);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() ==
        XamlObjectWriterDiagnosticCodes::UnknownMember);
    CHECK(diagnostics.Items()[0].Source().begin.line == 3U);
    CHECK(TestElement::AbortCount() == 2U);
    CHECK(TestElement::LiveCount() == 0U);
    return true;
}

bool TestInvalidScalarDiagnosticAndRollback() {
    TestElement::ResetCounters();
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = LoadDocument(
        fixture,
        StringView(
            "<Element xmlns=\"urn:test\" Count=\"not-an-integer\"/>"),
        diagnostics);
    CHECK(!loaded);
    CHECK(loaded.GetStatus().code == ErrorCode::ValidationFailed);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() ==
        XamlObjectWriterDiagnosticCodes::InvalidValue);
    CHECK(diagnostics.Items()[0].Source().begin.line == 1U);
    CHECK(diagnostics.Items()[0].Source().begin.column != 0U);
    CHECK(TestElement::AbortCount() == 1U);
    CHECK(TestElement::LiveCount() == 0U);
    return true;
}

bool TestDuplicateSetOnceMember() {
    TestElement::ResetCounters();
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = LoadDocument(
        fixture,
        StringView(
            "<Element xmlns=\"urn:test\" Title=\"One\">"
            "<Element.Title>Two</Element.Title>"
            "</Element>"),
        diagnostics);
    CHECK(!loaded);
    CHECK(loaded.GetStatus().code == ErrorCode::AlreadyExists);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() ==
        XamlObjectWriterDiagnosticCodes::DuplicateMemberValue);
    CHECK(TestElement::AbortCount() == 1U);
    CHECK(TestElement::LiveCount() == 0U);
    return true;
}

} // namespace

int main() {
    if (!TestAttributesAttachedPropertyAndContentCollection()) return 1;
    if (!TestPropertyElementAssignment()) return 1;
    if (!TestRollbackAfterCompletedChild()) return 1;
    if (!TestInvalidScalarDiagnosticAndRollback()) return 1;
    if (!TestDuplicateSetOnceMember()) return 1;
    std::puts("Aero XAML object writer tests passed");
    return 0;
}
