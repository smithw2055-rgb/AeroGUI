#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/Dispatcher.hpp>
#include <Aero/Core/Presentation.hpp>
#include <Aero/Core/TypeRegistry.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlDependencyProperty.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#include <cstdint>
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

class TestElement final : public DependencyObject {
public:
    explicit TestElement(TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {
        ++liveCount_;
    }

    ~TestElement() override {
        --liveCount_;
    }

    static void ResetCounters() noexcept {
        liveCount_ = 0U;
        beginCount_ = 0U;
        endCount_ = 0U;
        abortCount_ = 0U;
    }

    static std::uint32_t LiveCount() noexcept { return liveCount_; }
    static std::uint32_t BeginCount() noexcept { return beginCount_; }
    static std::uint32_t EndCount() noexcept { return endCount_; }
    static std::uint32_t AbortCount() noexcept { return abortCount_; }

    static Result<void> Begin(Object& object, void*) noexcept {
        TestElement& element = static_cast<TestElement&>(object);
        if (element.begun_) {
            return Status::Failure(
                ErrorCode::InvalidState,
                "BeginInit called twice");
        }
        element.begun_ = true;
        ++beginCount_;
        return {};
    }

    static Result<void> End(Object& object, void*) noexcept {
        TestElement& element = static_cast<TestElement&>(object);
        if (!element.begun_ || element.ended_) {
            return Status::Failure(
                ErrorCode::InvalidState,
                "EndInit state is invalid");
        }
        element.ended_ = true;
        ++endCount_;
        return {};
    }

    static void Abort(Object& object, void*) noexcept {
        TestElement& element = static_cast<TestElement&>(object);
        if (!element.aborted_) {
            element.aborted_ = true;
            ++abortCount_;
        }
    }

private:
    bool begun_ = false;
    bool ended_ = false;
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

bool ValidateWidth(const PropertyValue& value) noexcept {
    return value.Kind() == PropertyValueKind::Double &&
        value.AsDouble() >= 0.0;
}

Result<PropertyValue> CoerceWidth(
    DependencyObject&,
    const DependencyProperty&,
    const PropertyValue& value) noexcept {
    const double current = value.AsDouble();
    return PropertyValue::FromDouble(
        value.Type(),
        current > 100.0 ? 100.0 : current);
}

struct Fixture final {
    Dispatcher dispatcher;
    TypeRegistry types;
    DependencyPropertyRegistry properties{types};
    PresentationContextScope presentation{dispatcher, properties};
    XamlSchemaContext schema{types};
    XamlActivationProviderRegistry activations{schema};
    XamlDependencyPropertyBridge bridge{schema, properties};

    TypeId objectType = InvalidTypeId;
    TypeId booleanType = InvalidTypeId;
    TypeId integerType = InvalidTypeId;
    TypeId unsignedType = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId elementType = InvalidTypeId;
    TypeId buttonType = InvalidTypeId;
    TypeId gridType = InvalidTypeId;

    DependencyPropertyHandle width;
    DependencyPropertyHandle enabled;
    DependencyPropertyHandle count;
    DependencyPropertyHandle unsignedValue;
    DependencyPropertyHandle dataContext;
    DependencyPropertyHandle child;
    DependencyPropertyHandle isLocked;
    DependencyPropertyHandle row;

    std::uint32_t activationCount = 0U;
    TypeId lastRequestedType = InvalidTypeId;
    bool failActivation = false;
    std::uint32_t applicationMarker = 0xA311U;
    std::uint32_t hostMarker = 0xB411U;

    static Result<Ref<Object>> Activate(
        TypeId requestedType,
        const XamlActivationContext& activation,
        void* context) noexcept {
        auto* fixture = static_cast<Fixture*>(context);
        if (fixture == nullptr ||
            activation.dispatcher != &fixture->dispatcher ||
            activation.dependencyProperties != &fixture->properties ||
            activation.applicationServices != &fixture->applicationMarker ||
            activation.hostContext != &fixture->hostMarker) {
            return Status::Failure(
                ErrorCode::InvalidArgument,
                "XAML activation services do not match the fixture");
        }
        if (fixture->failActivation) {
            return Status::Failure(
                ErrorCode::InternalError,
                "Fixture activation failure");
        }

        ++fixture->activationCount;
        fixture->lastRequestedType = requestedType;
        Result<Ref<TestElement>> made = MakeRef<TestElement>(requestedType);
        if (!made) {
            return made.GetStatus();
        }
        Ref<TestElement> typed = std::move(made).Value();
        Ref<Object> object(std::move(typed));
        return object;
    }

