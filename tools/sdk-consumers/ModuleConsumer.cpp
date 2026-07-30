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
        Aero::Presentation::UIElement>::value,
    "Root UIElement must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::FrameworkElement,
        Aero::Presentation::FrameworkElement>::value,
    "Root FrameworkElement must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::RoutedEventArgs,
        Aero::Presentation::RoutedEventArgs>::value,
    "Root routed-event arguments must preserve type identity");

static_assert(
    std::is_same<Aero::Thickness, Aero::Base::Thickness>::value,
    "Root Thickness must preserve value type identity");

static_assert(
    std::is_same<
        Aero::HorizontalAlignment,
        Aero::Presentation::HorizontalAlignment>::value,
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
        Aero::Presentation::BindingSpec>::value,
    "Data binding projection must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::Input::ICommand,
        Aero::Presentation::ICommand>::value,
    "Input projection must preserve command runtime type identity");

static_assert(
    std::is_same<
        Aero::Media::Brush,
        Aero::Presentation::Brush>::value,
    "Media projection must preserve brush runtime type identity");

static_assert(
    std::is_same<
        Aero::Media::Geometry,
        Aero::Presentation::Geometry>::value,
    "Media geometry projection must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::Media::Animation::Timeline,
        Aero::Animation::Timeline>::value,
    "Media.Animation projection must preserve timeline runtime type identity");

static_assert(
    std::is_same<
        Aero::ResourceDictionary,
        Aero::Presentation::ResourceDictionary>::value,
    "Root ResourceDictionary projection must preserve runtime type identity");

static_assert(
    std::is_same<
        Aero::Style,
        Aero::Presentation::Style>::value,
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

} // namespace
