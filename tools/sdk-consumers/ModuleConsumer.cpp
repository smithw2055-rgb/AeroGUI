#include <Aero/ModuleSdk.hpp>

#include <type_traits>

namespace {

class ConsumerControl final : public Aero::Controls::Control {
    AERO_DECLARE_TYPE_NAMED(
        ConsumerControl,
        Aero::Controls::Control,
        "urn:aero-sdk-consumer",
        "ConsumerControl")

public:
    ConsumerControl() noexcept
        : Control(StaticTypeId()) {}

    inline static constexpr Members::Property<bool>
        ActiveProperty{"Active"};
    inline static constexpr Members::RoutedEvent<
        Aero::RoutedEventArgs>
        ActivatedEvent{"Activated"};
};

class ConsumerButton final : public Aero::Controls::Button {
    AERO_DECLARE_TYPE_NAMED(
        ConsumerButton,
        Aero::Controls::Button,
        "urn:aero-sdk-consumer",
        "ConsumerButton")

public:
    ConsumerButton() noexcept
        : Button(StaticTypeId()) {}
};

Aero::Base::Result<void> RegisterConsumerModule(
    Aero::Meta::Context& context) noexcept {
    Aero::Base::Result<void> status =
        Aero::Meta::Describe<ConsumerControl>(context)
            .Property(
                ConsumerControl::ActiveProperty,
                Aero::Meta::PropertyOptions(false)
                    .AffectsRender())
            .Event(
                ConsumerControl::ActivatedEvent,
                Aero::Meta::Routing::Bubble)
            .Factory()
            .Result();
    if (!status) return status.GetStatus();

    return Aero::Meta::Describe<ConsumerButton>(context)
        .Factory()
        .Result();
}

constexpr Aero::ModuleRegistration ConsumerModule =
    Aero::DefineModule(
        "Aero.SdkConsumer",
        &RegisterConsumerModule);

constexpr Aero::Core::PropertyProviderToken CanonicalStyleTriggerToken{
    Aero::Core::PropertyValueRank::StyleTrigger,
    Aero::Core::FirstCanonicalProviderOrigin,
    0U};
constexpr Aero::Core::PropertyProviderToken LaterStyleTriggerToken{
    Aero::Core::PropertyValueRank::StyleTrigger,
    Aero::Core::FirstCanonicalProviderOrigin,
    1U};

[[maybe_unused]] Aero::Core::PropertyProviderSet ProviderSet;

[[maybe_unused]] bool ExerciseCanonicalProviderSet() {
    Aero::Core::PropertyProviderSet providers;
    const Aero::Core::PropertyValue first =
        Aero::Core::PropertyValue::FromBoolean(
            Aero::Core::TypeOf<bool>(), false);
    const Aero::Core::PropertyValue second =
        Aero::Core::PropertyValue::FromBoolean(
            Aero::Core::TypeOf<bool>(), true);
    if (!providers.Set(CanonicalStyleTriggerToken, first) ||
        !providers.Set(LaterStyleTriggerToken, second)) {
        return false;
    }
    const Aero::Core::PropertyProviderContribution* winner =
        providers.Winner();
    if (providers.Count() != 2U || winner == nullptr ||
        winner->token != LaterStyleTriggerToken ||
        !winner->value.AsBoolean()) {
        return false;
    }
    if (!providers.Set(CanonicalStyleTriggerToken, second) ||
        providers.Count() != 2U) {
        return false;
    }
    if (!providers.Remove(LaterStyleTriggerToken) ||
        providers.Count() != 1U) {
        return false;
    }
    winner = providers.Winner();
    return winner != nullptr &&
        winner->token == CanonicalStyleTriggerToken &&
        winner->value.AsBoolean();
}

static_assert(
    std::is_same<
        decltype(ConsumerModule.registerModule),
        Aero::ModuleRegisterCallback>::value,
    "Module SDK must expose the typed registration callback");

static_assert(
    std::is_same<
        Aero::Meta::Context,
        Aero::MetadataContext>::value,
    "Meta context must preserve the compatibility authoring identity");

static_assert(
    std::is_same<
        Aero::DependencyObject,
        Aero::Core::DependencyObject>::value,
    "Root DependencyObject must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::UIElement,
        Aero::UIElement>::value,
    "Root UIElement must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::FrameworkElement,
        Aero::FrameworkElement>::value,
    "Root FrameworkElement must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::RoutedEventArgs,
        Aero::RoutedEventArgs>::value,
    "Root routed-event arguments must preserve type identity");

static_assert(
    std::is_same<Aero::Thickness, Aero::Base::Thickness>::value,
    "Root Thickness must preserve value type identity");

static_assert(
    std::is_same<
        Aero::HorizontalAlignment,
        Aero::HorizontalAlignment>::value,
    "Root alignment must preserve value type identity");

static_assert(
    std::is_base_of<
        Aero::Controls::Button,
        ConsumerButton>::value,
    "Standard Button must remain derivable for custom controls");

static_assert(
    std::is_same<
        Aero::Controls::Primitives::ButtonBase,
        Aero::Controls::ButtonBase>::value,
    "Controls.Primitives projection must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::Data::Binding,
        Aero::Data::Binding>::value,
    "Data binding projection must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::Input::ICommand,
        Aero::Input::ICommand>::value,
    "Input projection must preserve command runtime type identity");

static_assert(
    std::is_same<
        Aero::Media::Brush,
        Aero::Media::Brush>::value,
    "Media projection must preserve brush runtime type identity");

static_assert(
    std::is_same<
        Aero::Media::Geometry,
        Aero::Media::Geometry>::value,
    "Media geometry projection must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::Media::Animation::Timeline,
        Aero::Media::Animation::Timeline>::value,
    "Media.Animation projection must preserve timeline runtime type identity");

static_assert(
    std::is_same<
        Aero::ResourceDictionary,
        Aero::ResourceDictionary>::value,
    "Root ResourceDictionary projection must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::Style,
        Aero::Style>::value,
    "Root Style projection must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::Shapes::Rectangle,
        Aero::Controls::Rectangle>::value,
    "Shapes projection must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::Threading::Dispatcher,
        Aero::Core::Dispatcher>::value,
    "Threading projection must preserve dispatcher identity");

static_assert(
    CanonicalStyleTriggerToken.IsValid(),
    "Property provider tokens require a non-default rank and origin");

static_assert(
    Aero::Core::FirstCanonicalProviderOrigin >
        Aero::Core::AnimationValueProviderOrigin,
    "Manager providers must not reuse engine-owned local or animation origins");

static_assert(
    static_cast<unsigned>(
        Aero::Core::PropertyValueRank::Local) >
    static_cast<unsigned>(
        Aero::Core::PropertyValueRank::StyleTrigger),
    "Local values must outrank style triggers");

static_assert(
    static_cast<unsigned>(
        Aero::Core::PropertyValueRank::StyleTrigger) >
    static_cast<unsigned>(
        Aero::Core::PropertyValueRank::TemplateTrigger),
    "Style triggers must outrank template triggers");

} // namespace
